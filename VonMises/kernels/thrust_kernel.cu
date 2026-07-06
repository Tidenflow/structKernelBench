#include "thrust_kernel.h"

#ifdef HAS_CUDA

#include <cuda_runtime.h>
#include <cstdio>

#include <thrust/device_vector.h>
#include <thrust/transform.h>
#include <thrust/execution_policy.h>
#include <thrust/tuple.h>

#define CUDA_CHECK(call) do {                              \
    cudaError_t err = call;                                \
    if (err != cudaSuccess) {                              \
        std::fprintf(stderr, "[CUDA ERROR] %s:%d %s: %s\n", \
                     __FILE__, __LINE__, #call,             \
                     cudaGetErrorString(err));              \
        return err;                                        \
    }                                                      \
} while(0)

// Thrust 错误通过 cudaGetLastError 检测
#define THRUST_CHECK() do {                                \
    cudaError_t err = cudaGetLastError();                  \
    if (err != cudaSuccess) {                              \
        std::fprintf(stderr, "[THRUST ERROR] %s:%d: %s\n", \
                     __FILE__, __LINE__,                    \
                     cudaGetErrorString(err));              \
        return 1;                                          \
    }                                                      \
} while(0)

// ---- 计算 functor (接收 zip_iterator 产生的 tuple) ----
struct VonMisesFunctor {
    __host__ __device__
    float operator()(const thrust::tuple<float,float,float,float,float,float>& t) const {
        float sx  = thrust::get<0>(t);
        float sy  = thrust::get<1>(t);
        float sz  = thrust::get<2>(t);
        float txy = thrust::get<3>(t);
        float tyz = thrust::get<4>(t);
        float tzx = thrust::get<5>(t);
        float ds = sx - sy;
        float ss = sy - sz;
        float ts = sz - sx;
        return sqrtf(0.5f * (ds*ds + ss*ss + ts*ts +
                              6.0f * (txy*txy + tyz*tyz + tzx*tzx)));
    }
};

int vonmises_thrust(const float* h_sx,  const float* h_sy,  const float* h_sz,
                    const float* h_txy, const float* h_tyz, const float* h_tzx,
                    float* h_vm, int N,
                    double* kernel_time_ms, double* transfer_time_ms)
{
    cudaEvent_t start_t = nullptr, stop_t = nullptr;
    cudaEvent_t start_k = nullptr, stop_k = nullptr;

    CUDA_CHECK(cudaEventCreate(&start_t));
    CUDA_CHECK(cudaEventCreate(&stop_t));
    CUDA_CHECK(cudaEventCreate(&start_k));
    CUDA_CHECK(cudaEventCreate(&stop_k));

    // ---- H2D: 用 thrust::device_vector 自动管理 ----
    CUDA_CHECK(cudaEventRecord(start_t));
    thrust::device_vector<float> d_sx (h_sx,  h_sx  + N);
    thrust::device_vector<float> d_sy (h_sy,  h_sy  + N);
    thrust::device_vector<float> d_sz (h_sz,  h_sz  + N);
    thrust::device_vector<float> d_txy(h_txy, h_txy + N);
    thrust::device_vector<float> d_tyz(h_tyz, h_tyz + N);
    thrust::device_vector<float> d_tzx(h_tzx, h_tzx + N);
    thrust::device_vector<float> d_vm(N);
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_h2d = 0;
    cudaEventElapsedTime(&t_h2d, start_t, stop_t);

    // ---- Kernel: thrust::transform ----
    CUDA_CHECK(cudaEventRecord(start_k));
    thrust::transform(
        thrust::device,
        thrust::make_zip_iterator(
            d_sx.begin(), d_sy.begin(), d_sz.begin(),
            d_txy.begin(), d_tyz.begin(), d_tzx.begin()),
        thrust::make_zip_iterator(
            d_sx.end(), d_sy.end(), d_sz.end(),
            d_txy.end(), d_tyz.end(), d_tzx.end()),
        d_vm.begin(),
        VonMisesFunctor());
    THRUST_CHECK();
    CUDA_CHECK(cudaEventRecord(stop_k));
    CUDA_CHECK(cudaEventSynchronize(stop_k));
    float t_kernel = 0;
    cudaEventElapsedTime(&t_kernel, start_k, stop_k);

    // ---- D2H ----
    CUDA_CHECK(cudaEventRecord(start_t));
    thrust::copy(d_vm.begin(), d_vm.end(), h_vm);
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_d2h = 0;
    cudaEventElapsedTime(&t_d2h, start_t, stop_t);

    *kernel_time_ms   = static_cast<double>(t_kernel);
    *transfer_time_ms = static_cast<double>(t_h2d + t_d2h);

    cudaEventDestroy(start_t); cudaEventDestroy(stop_t);
    cudaEventDestroy(start_k); cudaEventDestroy(stop_k);

    return 0;
}

#else
int vonmises_thrust(const float*, const float*, const float*,
                    const float*, const float*, const float*,
                    float*, int, double*, double*) {
    return 1;
}
#endif
