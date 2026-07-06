#include "thrust_kernel.h"

#ifdef HAS_CUDA

#include <cuda_runtime.h>
#include <cstdio>

#include <thrust/device_vector.h>
#include <thrust/for_each.h>
#include <thrust/execution_policy.h>
#include <thrust/iterator/counting_iterator.h>

#define CUDA_CHECK(call) do {                              \
    cudaError_t err = call;                                \
    if (err != cudaSuccess) {                              \
        std::fprintf(stderr, "[CUDA ERROR] %s:%d %s: %s\n", \
                     __FILE__, __LINE__, #call,             \
                     cudaGetErrorString(err));              \
        return err;                                        \
    }                                                      \
} while(0)

#define THRUST_CHECK() do {                                \
    cudaError_t err = cudaGetLastError();                  \
    if (err != cudaSuccess) {                              \
        std::fprintf(stderr, "[THRUST ERROR] %s:%d: %s\n", \
                     __FILE__, __LINE__,                    \
                     cudaGetErrorString(err));              \
        return 1;                                          \
    }                                                      \
} while(0)

// ---- 包络计算 functor ----
struct EnvelopeFunctor {
    const float* sx;
    const float* sy;
    const float* sz;
    const float* txy;
    const float* tyz;
    const float* tzx;
    float*       envelope;
    int          N;   // 工况数

    __host__ __device__
    void operator()(int i) const {
        float max_vm = 0.0f;
        for (int j = 0; j < N; ++j) {
            int idx = i * N + j;
            float ds = sx[idx] - sy[idx];
            float ss = sy[idx] - sz[idx];
            float ts = sz[idx] - sx[idx];
            float vm = sqrtf(0.5f * (ds*ds + ss*ss + ts*ts +
                                      6.0f * (txy[idx]*txy[idx] + tyz[idx]*tyz[idx] + tzx[idx]*tzx[idx])));
            max_vm = fmaxf(max_vm, vm);
        }
        envelope[i] = max_vm;
    }
};

int envelope_thrust(const float* h_sx,  const float* h_sy,  const float* h_sz,
                     const float* h_txy, const float* h_tyz, const float* h_tzx,
                     float* h_envelope, int M, int N,
                     double* kernel_time_ms, double* transfer_time_ms)
{
    cudaEvent_t start_t = nullptr, stop_t = nullptr;
    cudaEvent_t start_k = nullptr, stop_k = nullptr;

    CUDA_CHECK(cudaEventCreate(&start_t));
    CUDA_CHECK(cudaEventCreate(&stop_t));
    CUDA_CHECK(cudaEventCreate(&start_k));
    CUDA_CHECK(cudaEventCreate(&stop_k));

    // ---- H2D ----
    CUDA_CHECK(cudaEventRecord(start_t));
    thrust::device_vector<float> d_sx(h_sx,  h_sx  + M * N);
    thrust::device_vector<float> d_sy(h_sy,  h_sy  + M * N);
    thrust::device_vector<float> d_sz(h_sz,  h_sz  + M * N);
    thrust::device_vector<float> d_txy(h_txy, h_txy + M * N);
    thrust::device_vector<float> d_tyz(h_tyz, h_tyz + M * N);
    thrust::device_vector<float> d_tzx(h_tzx, h_tzx + M * N);
    thrust::device_vector<float> d_envelope(M);
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_h2d = 0;
    cudaEventElapsedTime(&t_h2d, start_t, stop_t);

    // ---- Kernel: thrust::for_each ----
    EnvelopeFunctor fn{
        thrust::raw_pointer_cast(d_sx.data()),
        thrust::raw_pointer_cast(d_sy.data()),
        thrust::raw_pointer_cast(d_sz.data()),
        thrust::raw_pointer_cast(d_txy.data()),
        thrust::raw_pointer_cast(d_tyz.data()),
        thrust::raw_pointer_cast(d_tzx.data()),
        thrust::raw_pointer_cast(d_envelope.data()),
        N
    };

    CUDA_CHECK(cudaEventRecord(start_k));
    thrust::for_each(thrust::device,
                      thrust::make_counting_iterator(0),
                      thrust::make_counting_iterator(M),
                      fn);
    THRUST_CHECK();
    CUDA_CHECK(cudaEventRecord(stop_k));
    CUDA_CHECK(cudaEventSynchronize(stop_k));
    float t_kernel = 0;
    cudaEventElapsedTime(&t_kernel, start_k, stop_k);

    // ---- D2H ----
    CUDA_CHECK(cudaEventRecord(start_t));
    thrust::copy(d_envelope.begin(), d_envelope.end(), h_envelope);
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
int envelope_thrust(const float*, const float*, const float*,
                     const float*, const float*, const float*,
                     float*, int, int, double*, double*) {
    return 1;
}
#endif
