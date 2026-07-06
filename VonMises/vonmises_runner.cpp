#include "vonmises_runner.h"
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
// 内部工具: 生成随机 6 分量应力数据 (SoA 布局)
// ============================================================
static void generate_stress_data(int N,
                                 std::vector<float>& sx,  std::vector<float>& sy,
                                 std::vector<float>& sz,  std::vector<float>& txy,
                                 std::vector<float>& tyz, std::vector<float>& tzx)
{
    sx.resize(N);  sy.resize(N);  sz.resize(N);
    txy.resize(N); tyz.resize(N); tzx.resize(N);
    for (int i = 0; i < N; ++i) {
        // 模拟真实应力分布: 正应力 ~100 MPa 量级, 剪应力 ~50 MPa
        sx[i]  = static_cast<float>(std::rand() % 20000) / 100.0f;  // 0~200
        sy[i]  = static_cast<float>(std::rand() % 20000) / 100.0f;
        sz[i]  = static_cast<float>(std::rand() % 20000) / 100.0f;
        txy[i] = static_cast<float>(std::rand() % 10000 - 5000) / 100.0f; // -50~50
        tyz[i] = static_cast<float>(std::rand() % 10000 - 5000) / 100.0f;
        tzx[i] = static_cast<float>(std::rand() % 10000 - 5000) / 100.0f;
    }
}

// ============================================================
// 内部工具: 验证 — baseline 做参考，小规模 double 精度冗余校验
// ============================================================
static double validate_vonmises(const std::vector<float>& sx,  const std::vector<float>& sy,
                                const std::vector<float>& sz,  const std::vector<float>& txy,
                                const std::vector<float>& tyz, const std::vector<float>& tzx,
                                int N)
{
    // 参考值: double 精度逐元素计算
    std::vector<double> ref(N);
    for (int i = 0; i < N; ++i) {
        double ds = (double)sx[i] - (double)sy[i];
        double ss = (double)sy[i] - (double)sz[i];
        double ts = (double)sz[i] - (double)sx[i];
        ref[i] = std::sqrt(0.5 * (ds*ds + ss*ss + ts*ts +
                                   6.0 * ((double)txy[i]*(double)txy[i] +
                                          (double)tyz[i]*(double)tyz[i] +
                                          (double)tzx[i]*(double)tzx[i])));
    }

    std::vector<float> vm(N);
    vonmises_baseline(sx.data(), sy.data(), sz.data(),
                      txy.data(), tyz.data(), tzx.data(),
                      vm.data(), N);

    double max_err = 0.0;
    for (int i = 0; i < N; ++i) {
        double err = std::abs((double)vm[i] - ref[i]);
        if (err > max_err) max_err = err;
    }
    return max_err;
}

// ============================================================
// 后端选择
// ============================================================

using VonMisesFunc = void(*)(const float*, const float*, const float*,
                               const float*, const float*, const float*,
                               float*, int);

static int select_backend(const std::string& backend,
                          VonMisesFunc& func, int& threads,
                          bool& is_cuda, bool& is_thrust)
{
    is_cuda   = false;
    is_thrust = false;
    if (backend == "baseline") {
        func = vonmises_baseline; threads = 1; return 0;
    }
    if (backend == "openmp") {
        func = vonmises_openmp;
#ifdef _OPENMP
        threads = omp_get_max_threads();
#else
        threads = 1;
#endif
        return 0;
    }
    if (backend == "simd") {
        func = vonmises_simd; threads = 1; return 0;
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
int run_vonmises_benchmark(int num_elements,
                           const std::string& backend,
                           VonMisesResult& result)
{
    VonMisesFunc vm_fn = nullptr;
    int          threads = 1;
    bool         is_cuda = false;
    bool         is_thrust = false;

    if (select_backend(backend, vm_fn, threads, is_cuda, is_thrust) != 0) {
        result.validated = false;
        return 1;
    }

    // ---- 验证 (小规模) ----
    {
        std::vector<float> sx, sy, sz, txy, tyz, tzx;
        generate_stress_data(500, sx, sy, sz, txy, tyz, tzx);
        double max_err = validate_vonmises(sx, sy, sz, txy, tyz, tzx, 500);
        result.validated = (max_err < 5e-4);  // float 精度容差
        result.max_error = max_err;
        if (!result.validated) return 1;
    }

    // ---- 生成数据 ----
    std::vector<float> sx, sy, sz, txy, tyz, tzx;
    generate_stress_data(num_elements, sx, sy, sz, txy, tyz, tzx);
    std::vector<float> vm(num_elements);

    // ---- 计时 ----
    constexpr int WARMUP = 3;
    constexpr int BENCH  = 10;

    if (is_cuda || is_thrust) {
#ifdef HAS_CUDA
        auto gpu_fn = is_thrust ? vonmises_thrust : vonmises_cuda;

        {
            double kt, tt;
            int err = gpu_fn(sx.data(), sy.data(), sz.data(),
                              txy.data(), tyz.data(), tzx.data(),
                              vm.data(), num_elements, &kt, &tt);
            if (err != 0) { result.validated = false; return 1; }
        }
        for (int iter = 0; iter < WARMUP; ++iter) {
            double kt, tt;
            gpu_fn(sx.data(), sy.data(), sz.data(),
                   txy.data(), tyz.data(), tzx.data(),
                   vm.data(), num_elements, &kt, &tt);
        }

        std::vector<double> ktimes, ttimes;
        for (int iter = 0; iter < BENCH; ++iter) {
            double kt, tt;
            int err = gpu_fn(sx.data(), sy.data(), sz.data(),
                              txy.data(), tyz.data(), tzx.data(),
                              vm.data(), num_elements, &kt, &tt);
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
            vm_fn(sx.data(), sy.data(), sz.data(),
                  txy.data(), tyz.data(), tzx.data(),
                  vm.data(), num_elements);

        std::vector<double> times;
        Timer timer;
        for (int iter = 0; iter < BENCH; ++iter) {
            timer.start();
            vm_fn(sx.data(), sy.data(), sz.data(),
                  txy.data(), tyz.data(), tzx.data(),
                  vm.data(), num_elements);
            times.push_back(timer.stop_ms());
        }
        double time_med = median(times);
        result.time_ms  = time_med;
        result.time_min = *std::min_element(times.begin(), times.end());
        result.time_max = *std::max_element(times.begin(), times.end());
    }

    // 吞吐量: 百万元素/秒
    result.throughput = (num_elements / 1e6) / (result.time_ms / 1000.0);

    result.kernel       = "Von Mises";
    result.backend      = backend;
    result.num_elements = num_elements;
    result.threads      = threads;
    result.timestamp    = now_str();

    return 0;
}
