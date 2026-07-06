#include "envelope_runner.h"
#include "kernels/baseline.h"
#include "kernels/openmp.h"
#include "kernels/simd.h"
#include "kernels/cuda_kernel.h"
#include "kernels/thrust_kernel.h"
#include "common/bench_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

// ============================================================
// 内部工具: 生成随机多工况应力数据 (element-major 布局)
// sx[i*N + j] = 第 i 单元在第 j 工况的 σx
// ============================================================
static void generate_envelope_data(int M, int N,
                                    std::vector<float>& sx,  std::vector<float>& sy,
                                    std::vector<float>& sz,  std::vector<float>& txy,
                                    std::vector<float>& tyz, std::vector<float>& tzx)
{
    int total = M * N;
    sx.resize(total);   sy.resize(total);  sz.resize(total);
    txy.resize(total);  tyz.resize(total); tzx.resize(total);
    for (int k = 0; k < total; ++k) {
        // 模拟真实应力分布: 正应力 ~100 MPa 量级, 剪应力 ~50 MPa
        sx[k]  = static_cast<float>(std::rand() % 20000) / 100.0f;  // 0~200
        sy[k]  = static_cast<float>(std::rand() % 20000) / 100.0f;
        sz[k]  = static_cast<float>(std::rand() % 20000) / 100.0f;
        txy[k] = static_cast<float>(std::rand() % 10000 - 5000) / 100.0f; // -50~50
        tyz[k] = static_cast<float>(std::rand() % 10000 - 5000) / 100.0f;
        tzx[k] = static_cast<float>(std::rand() % 10000 - 5000) / 100.0f;
    }
}

// ============================================================
// 内部工具: 验证 — baseline 做参考，小规模 double 精度冗余校验
// ============================================================
static double validate_envelope(const std::vector<float>& sx,  const std::vector<float>& sy,
                                 const std::vector<float>& sz,  const std::vector<float>& txy,
                                 const std::vector<float>& tyz, const std::vector<float>& tzx,
                                 int M, int N)
{
    // 参考值: double 精度逐元素 + 逐工况计算
    std::vector<double> ref(M);
    for (int i = 0; i < M; ++i) {
        double max_vm = 0.0;
        for (int j = 0; j < N; ++j) {
            int idx = i * N + j;
            double ds = (double)sx[idx] - (double)sy[idx];
            double ss = (double)sy[idx] - (double)sz[idx];
            double ts = (double)sz[idx] - (double)sx[idx];
            double vm = std::sqrt(0.5 * (ds*ds + ss*ss + ts*ts +
                                         6.0 * ((double)txy[idx]*(double)txy[idx] +
                                                (double)tyz[idx]*(double)tyz[idx] +
                                                (double)tzx[idx]*(double)tzx[idx])));
            max_vm = std::max(max_vm, vm);
        }
        ref[i] = max_vm;
    }

    std::vector<float> envelope(M);
    envelope_baseline(sx.data(), sy.data(), sz.data(),
                       txy.data(), tyz.data(), tzx.data(),
                       envelope.data(), M, N);

    double max_err = 0.0;
    for (int i = 0; i < M; ++i) {
        double err = std::abs((double)envelope[i] - ref[i]);
        if (err > max_err) max_err = err;
    }
    return max_err;
}

// ============================================================
// 后端选择
// ============================================================

using EnvelopeFunc = void(*)(const float*, const float*, const float*,
                               const float*, const float*, const float*,
                               float*, int, int);

static int select_backend(const std::string& backend,
                          EnvelopeFunc& func, int& threads,
                          bool& is_cuda, bool& is_thrust)
{
    is_cuda   = false;
    is_thrust = false;
    if (backend == "baseline") {
        func = envelope_baseline; threads = 1; return 0;
    }
    if (backend == "openmp") {
        func = envelope_openmp;
#ifdef _OPENMP
        threads = omp_get_max_threads();
#else
        threads = 1;
#endif
        return 0;
    }
    if (backend == "simd") {
        func = envelope_simd; threads = 1; return 0;
    }
    if (backend == "cuda") {
        is_cuda = true; threads = 0; return 0;
    }
    if (backend == "thrust") {
        is_thrust = true; threads = 0; return 0;
    }
    return 1;
}

// ============================================================
// 公开接口
// ============================================================
int run_envelope_benchmark(int num_elements,
                            int num_load_cases,
                            const std::string& backend,
                            EnvelopeResult& result)
{
    EnvelopeFunc env_fn = nullptr;
    int          threads = 1;
    bool         is_cuda = false;
    bool         is_thrust = false;

    if (select_backend(backend, env_fn, threads, is_cuda, is_thrust) != 0) {
        result.validated = false;
        return 1;
    }

    // ---- 验证 (小规模) ----
    {
        std::vector<float> sx, sy, sz, txy, tyz, tzx;
        generate_envelope_data(200, 5, sx, sy, sz, txy, tyz, tzx);
        double max_err = validate_envelope(sx, sy, sz, txy, tyz, tzx, 200, 5);
        result.validated = (max_err < 5e-4);  // float 精度容差
        result.max_error = max_err;
        if (!result.validated) return 1;
    }

    // ---- 生成数据 ----
    std::vector<float> sx, sy, sz, txy, tyz, tzx;
    generate_envelope_data(num_elements, num_load_cases, sx, sy, sz, txy, tyz, tzx);
    std::vector<float> envelope(num_elements);

    // ---- 计时 ----
    constexpr int WARMUP = 3;
    constexpr int BENCH  = 10;

    if (is_cuda || is_thrust) {
#ifdef HAS_CUDA
        auto gpu_fn = is_thrust ? envelope_thrust : envelope_cuda;

        {
            double kt, tt;
            int err = gpu_fn(sx.data(), sy.data(), sz.data(),
                              txy.data(), tyz.data(), tzx.data(),
                              envelope.data(), num_elements, num_load_cases, &kt, &tt);
            if (err != 0) { result.validated = false; return 1; }
        }
        for (int iter = 0; iter < WARMUP; ++iter) {
            double kt, tt;
            gpu_fn(sx.data(), sy.data(), sz.data(),
                   txy.data(), tyz.data(), tzx.data(),
                   envelope.data(), num_elements, num_load_cases, &kt, &tt);
        }

        std::vector<double> ktimes, ttimes;
        for (int iter = 0; iter < BENCH; ++iter) {
            double kt, tt;
            int err = gpu_fn(sx.data(), sy.data(), sz.data(),
                              txy.data(), tyz.data(), tzx.data(),
                              envelope.data(), num_elements, num_load_cases, &kt, &tt);
            if (err != 0) { result.validated = false; return 1; }
            ktimes.push_back(kt);
            ttimes.push_back(tt);
        }
        double time_med = median(ktimes);
        result.time_ms  = time_med;
        result.time_min = *std::min_element(ktimes.begin(), ktimes.end());
        result.time_max = *std::max_element(ktimes.begin(), ktimes.end());
#else
        result.validated = false;
        return 1;
#endif
    } else {
        for (int iter = 0; iter < WARMUP; ++iter)
            env_fn(sx.data(), sy.data(), sz.data(),
                   txy.data(), tyz.data(), tzx.data(),
                   envelope.data(), num_elements, num_load_cases);

        std::vector<double> times;
        Timer timer;
        for (int iter = 0; iter < BENCH; ++iter) {
            timer.start();
            env_fn(sx.data(), sy.data(), sz.data(),
                   txy.data(), tyz.data(), tzx.data(),
                   envelope.data(), num_elements, num_load_cases);
            times.push_back(timer.stop_ms());
        }
        double time_med = median(times);
        result.time_ms  = time_med;
        result.time_min = *std::min_element(times.begin(), times.end());
        result.time_max = *std::max_element(times.begin(), times.end());
    }

    // 吞吐量: 百万元素/秒 (只算输出元素数)
    result.throughput = (num_elements / 1e6) / (result.time_ms / 1000.0);

    result.kernel         = "MaxStressEnvelope";
    result.backend        = backend;
    result.num_elements   = num_elements;
    result.num_load_cases = num_load_cases;
    result.threads        = threads;
    result.timestamp      = now_str();

    return 0;
}
