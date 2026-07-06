#include "cuda_kernel.h"

#ifdef HAS_CUDA

#include <cuda_runtime.h>
#include <cstdio>

#define CUDA_CHECK(call) do {                              \
    cudaError_t err = call;                                \
    if (err != cudaSuccess) {                              \
        std::fprintf(stderr, "[CUDA ERROR] %s:%d %s: %s\n", \
                     __FILE__, __LINE__, #call,             \
                     cudaGetErrorString(err));              \
        return err;                                        \
    }                                                      \
} while(0)

#define CUDA_KERNEL_CHECK() do {                           \
    cudaError_t err = cudaGetLastError();                  \
    if (err != cudaSuccess) {                              \
        std::fprintf(stderr, "[CUDA KERNEL ERROR] %s:%d: %s\n", \
                     __FILE__, __LINE__,                    \
                     cudaGetErrorString(err));              \
        return err;                                        \
    }                                                      \
} while(0)

__global__ void envelope_cuda_kernel(const float* sx, const float* sy, const float* sz,
                                      const float* txy, const float* tyz, const float* tzx,
                                      float* envelope, int M, int N) {
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    if (i >= M) return;

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

int envelope_cuda(const float* h_sx,  const float* h_sy,  const float* h_sz,
                   const float* h_txy, const float* h_tyz, const float* h_tzx,
                   float* h_envelope, int M, int N,
                   double* kernel_time_ms, double* transfer_time_ms)
{
    float *d_sx = nullptr, *d_sy = nullptr, *d_sz = nullptr;
    float *d_txy = nullptr, *d_tyz = nullptr, *d_tzx = nullptr;
    float *d_envelope = nullptr;
    cudaEvent_t start_t = nullptr, stop_t = nullptr;
    cudaEvent_t start_k = nullptr, stop_k = nullptr;

    size_t bytes = static_cast<size_t>(M) * N * sizeof(float);
    size_t out_bytes = M * sizeof(float);

    CUDA_CHECK(cudaMalloc(&d_sx,  bytes));
    CUDA_CHECK(cudaMalloc(&d_sy,  bytes));
    CUDA_CHECK(cudaMalloc(&d_sz,  bytes));
    CUDA_CHECK(cudaMalloc(&d_txy, bytes));
    CUDA_CHECK(cudaMalloc(&d_tyz, bytes));
    CUDA_CHECK(cudaMalloc(&d_tzx, bytes));
    CUDA_CHECK(cudaMalloc(&d_envelope, out_bytes));

    CUDA_CHECK(cudaEventCreate(&start_t));
    CUDA_CHECK(cudaEventCreate(&stop_t));
    CUDA_CHECK(cudaEventCreate(&start_k));
    CUDA_CHECK(cudaEventCreate(&stop_k));

    // H2D
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(d_sx,  h_sx,  bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_sy,  h_sy,  bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_sz,  h_sz,  bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_txy, h_txy, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_tyz, h_tyz, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_tzx, h_tzx, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_h2d = 0;
    cudaEventElapsedTime(&t_h2d, start_t, stop_t);

    // Launch
    int blockSize = 256;
    int gridSize  = (M + blockSize - 1) / blockSize;

    CUDA_CHECK(cudaEventRecord(start_k));
    envelope_cuda_kernel<<<gridSize, blockSize>>>(d_sx, d_sy, d_sz, d_txy, d_tyz, d_tzx, d_envelope, M, N);
    CUDA_KERNEL_CHECK();
    CUDA_CHECK(cudaEventRecord(stop_k));
    CUDA_CHECK(cudaEventSynchronize(stop_k));
    float t_kernel = 0;
    cudaEventElapsedTime(&t_kernel, start_k, stop_k);

    // D2H
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(h_envelope, d_envelope, out_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_d2h = 0;
    cudaEventElapsedTime(&t_d2h, start_t, stop_t);

    *kernel_time_ms   = static_cast<double>(t_kernel);
    *transfer_time_ms = static_cast<double>(t_h2d + t_d2h);

    cudaFree(d_sx);  cudaFree(d_sy);  cudaFree(d_sz);
    cudaFree(d_txy); cudaFree(d_tyz); cudaFree(d_tzx);
    cudaFree(d_envelope);
    cudaEventDestroy(start_t); cudaEventDestroy(stop_t);
    cudaEventDestroy(start_k); cudaEventDestroy(stop_k);

    return 0;
}

#else
int envelope_cuda(const float*, const float*, const float*,
                   const float*, const float*, const float*,
                   float*, int, int, double*, double*) {
    return 1;
}
#endif
