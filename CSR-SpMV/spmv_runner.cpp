#include "spmv_runner.h"
#include "baseline.h"
#include "openmp.h"
#include "simd.h"
#include "cuda_kernel.h"
#include "bench_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

// ============================================================
// 内部工具: 生成模拟结构刚度矩阵的稀疏矩阵 (CSR 格式)
// ============================================================
static void generate_sparse_matrix(int N, int nnz_per_row,
                                   std::vector<double>& values,
                                   std::vector<int>&    col_idx,
                                   std::vector<int>&    row_ptr)
{
    values.clear();
    col_idx.clear();
    row_ptr.resize(N + 1);

    int bandwidth = std::max(nnz_per_row * 5, 1);

    int nnz = 0;
    for (int i = 0; i < N; ++i) {
        row_ptr[i] = nnz;

        std::vector<int> cols;
        cols.reserve(nnz_per_row + 1);
        cols.push_back(i);  // 对角线必须有

        // 生成 nnz_per_row-1 个额外的随机列号
        for (int attempt = 0;
             static_cast<int>(cols.size()) < nnz_per_row && attempt < nnz_per_row * 10;
             ++attempt)
        {
            int offset = (std::rand() % (2 * bandwidth + 1)) - bandwidth;
            int col    = std::max(0, std::min(N - 1, i + offset));
            // 去重
            bool dup = false;
            for (int c : cols) { if (c == col) { dup = true; break; } }
            if (!dup) cols.push_back(col);
        }

        std::sort(cols.begin(), cols.end());

        for (int col : cols) {
            col_idx.push_back(col);
            double val = (col == i)
                ? 10.0 + (std::rand() % 1000) / 10.0
                : (std::rand() % 2000 - 1000) / 1000.0;
            values.push_back(val);
            ++nnz;
        }
    }
    row_ptr[N] = nnz;
}

// ============================================================
// 内部工具: 生成稠密向量
// ============================================================
static void generate_dense_vector(int N, std::vector<double>& x) {
    x.resize(N);
    for (int i = 0; i < N; ++i)
        x[i] = (std::rand() % 2000 - 1000) / 1000.0;
}

// ============================================================
// 内部工具: 用小规模稠密矩阵验证 CSR-SpMV 正确性
// ============================================================
static double validate_spmv(const std::vector<double>& values,
                            const std::vector<int>&    col_idx,
                            const std::vector<int>&    row_ptr,
                            const std::vector<double>& x,
                            int N)
{
    std::vector<double> A(N * N, 0.0);
    for (int i = 0; i < N; ++i)
        for (int k = row_ptr[i]; k < row_ptr[i + 1]; ++k)
            A[i * N + col_idx[k]] = values[k];

    std::vector<double> y_ref(N, 0.0);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            y_ref[i] += A[i * N + j] * x[j];

    std::vector<double> y(N, 0.0);
    spmv_baseline(values.data(), col_idx.data(), row_ptr.data(),
                  x.data(), y.data(), N);

    double max_err = 0.0;
    for (int i = 0; i < N; ++i) {
        double err = std::abs(y[i] - y_ref[i]);
        if (err > max_err) max_err = err;
    }
    return max_err;
}

// ============================================================
// 后端选择
// ============================================================

using SpmvFunc = void(*)(const double*, const int*, const int*,
                          const double*, double*, int);

static int select_backend(const std::string& backend,
                          SpmvFunc& func, int& threads, bool& is_cuda)
{
    is_cuda = false;
    if (backend == "baseline") {
        func = spmv_baseline;
        threads = 1;
        return 0;
    }
    if (backend == "openmp") {
        func = spmv_openmp;
#ifdef _OPENMP
        threads = omp_get_max_threads();
#else
        threads = 1;
#endif
        return 0;
    }
    if (backend == "simd") {
        func = spmv_simd;
        threads = 1;
        return 0;
    }
    if (backend == "cuda") {
        is_cuda = true;
        threads = 0;
        return 0;
    }
    return 1;  // unknown backend
}

// ============================================================
// 公开接口
// ============================================================
int run_spmv_benchmark(int rows, int nnz_per_row,
                       const std::string& backend,
                       BenchmarkResult& result)
{
    SpmvFunc spmv_fn = nullptr;
    int      threads = 1;
    bool     is_cuda = false;

    if (select_backend(backend, spmv_fn, threads, is_cuda) != 0) {
        result.validated = false;
        return 1;
    }

    // ---- 验证 (小规模, baseline) ----
    {
        std::vector<double> v, x;
        std::vector<int>    ci, rp;
        generate_sparse_matrix(500, nnz_per_row, v, ci, rp);
        generate_dense_vector(500, x);
        double max_err = validate_spmv(v, ci, rp, x, 500);
        result.validated = (max_err < 1e-10);
        result.max_error = max_err;
        if (!result.validated) return 1;
    }

    // ---- 生成数据 ----
    std::vector<double> values, x, y(rows);
    std::vector<int>    col_idx, row_ptr;

    generate_sparse_matrix(rows, nnz_per_row, values, col_idx, row_ptr);
    generate_dense_vector(rows, x);

    int nnz = static_cast<int>(values.size());

    // ---- 计时 ----
    constexpr int WARMUP = 3;
    constexpr int BENCH  = 10;
    double cuda_transfer_ms = 0.0;

    if (is_cuda) {
#ifdef HAS_CUDA
        // CUDA 路径: 内部计时 kernel + transfer
        {
            double kt, tt;
            int err = spmv_cuda(values.data(), col_idx.data(), row_ptr.data(),
                                x.data(), y.data(), rows, nnz, &kt, &tt);
            if (err != 0) { result.validated = false; return 1; }
        }
        for (int iter = 0; iter < WARMUP; ++iter) {
            double kt, tt;
            spmv_cuda(values.data(), col_idx.data(), row_ptr.data(),
                      x.data(), y.data(), rows, nnz, &kt, &tt);
        }

        std::vector<double> ktimes, ttimes;
        for (int iter = 0; iter < BENCH; ++iter) {
            double kt, tt;
            int err = spmv_cuda(values.data(), col_idx.data(), row_ptr.data(),
                                x.data(), y.data(), rows, nnz, &kt, &tt);
            if (err != 0) { result.validated = false; return 1; }
            ktimes.push_back(kt);
            ttimes.push_back(tt);
        }
        double time_med = median(ktimes);
        cuda_transfer_ms = median(ttimes);

        double gflops = (2.0 * nnz) / (time_med / 1000.0) / 1e9;
        result.time_ms  = time_med;
        result.time_min = *std::min_element(ktimes.begin(), ktimes.end());
        result.time_max = *std::max_element(ktimes.begin(), ktimes.end());
        result.gflops   = gflops;
#else
        result.validated = false;
        result.max_error = -1;
        return 1;
#endif
    } else {
        // CPU 路径
        for (int iter = 0; iter < WARMUP; ++iter)
            spmv_fn(values.data(), col_idx.data(), row_ptr.data(),
                    x.data(), y.data(), rows);

        std::vector<double> times;
        Timer timer;
        for (int iter = 0; iter < BENCH; ++iter) {
            timer.start();
            spmv_fn(values.data(), col_idx.data(), row_ptr.data(),
                    x.data(), y.data(), rows);
            times.push_back(timer.stop_ms());
        }
        double time_med = median(times);
        double gflops   = (2.0 * nnz) / (time_med / 1000.0) / 1e9;
        result.time_ms  = time_med;
        result.time_min = *std::min_element(times.begin(), times.end());
        result.time_max = *std::max_element(times.begin(), times.end());
        result.gflops   = gflops;
    }

    // ---- 填充结果 ----
    result.kernel      = "CSR-SpMV";
    result.backend     = backend;
    result.rows        = rows;
    result.nnz         = nnz;
    result.nnz_per_row = nnz_per_row;
    result.threads     = threads;
    result.timestamp   = now_str();

    return 0;
}
