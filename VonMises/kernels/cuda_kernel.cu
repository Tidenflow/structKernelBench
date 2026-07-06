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

__global__ void vonmises_cuda_kernel(const float* sx, const float* sy, const float* sz,
                                      const float* txy, const float* tyz, const float* tzx,
                                      float* vm, int N) {
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    if (i >= N) return;

    float ds = sx[i] - sy[i];
    float ss = sy[i] - sz[i];
    float ts = sz[i] - sx[i];
    vm[i] = sqrtf(0.5f * (ds*ds + ss*ss + ts*ts +
                           6.0f * (txy[i]*txy[i] + tyz[i]*tyz[i] + tzx[i]*tzx[i])));
}

int vonmises_cuda(const float* h_sx,  const float* h_sy,  const float* h_sz,
                  const float* h_txy, const float* h_tyz, const float* h_tzx,
                  float* h_vm, int N,
                  double* kernel_time_ms, double* transfer_time_ms)
{
    float *d_sx = nullptr, *d_sy = nullptr, *d_sz = nullptr;
    float *d_txy = nullptr, *d_tyz = nullptr, *d_tzx = nullptr;
    float *d_vm = nullptr;
    cudaEvent_t start_t = nullptr, stop_t = nullptr;
    cudaEvent_t start_k = nullptr, stop_k = nullptr;

    size_t bytes = N * sizeof(float);

    CUDA_CHECK(cudaMalloc(&d_sx,  bytes));
    CUDA_CHECK(cudaMalloc(&d_sy,  bytes));
    CUDA_CHECK(cudaMalloc(&d_sz,  bytes));
    CUDA_CHECK(cudaMalloc(&d_txy, bytes));
    CUDA_CHECK(cudaMalloc(&d_tyz, bytes));
    CUDA_CHECK(cudaMalloc(&d_tzx, bytes));
    CUDA_CHECK(cudaMalloc(&d_vm,  bytes));

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
    int gridSize  = (N + blockSize - 1) / blockSize;

    CUDA_CHECK(cudaEventRecord(start_k));
    vonmises_cuda_kernel<<<gridSize, blockSize>>>(d_sx, d_sy, d_sz, d_txy, d_tyz, d_tzx, d_vm, N);
    CUDA_KERNEL_CHECK();
    CUDA_CHECK(cudaEventRecord(stop_k));
    CUDA_CHECK(cudaEventSynchronize(stop_k));
    float t_kernel = 0;
    cudaEventElapsedTime(&t_kernel, start_k, stop_k);

    // D2H
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(h_vm, d_vm, bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_d2h = 0;
    cudaEventElapsedTime(&t_d2h, start_t, stop_t);

    *kernel_time_ms   = static_cast<double>(t_kernel);
    *transfer_time_ms = static_cast<double>(t_h2d + t_d2h);

    cudaFree(d_sx);  cudaFree(d_sy);  cudaFree(d_sz);
    cudaFree(d_txy); cudaFree(d_tyz); cudaFree(d_tzx);
    cudaFree(d_vm);
    cudaEventDestroy(start_t); cudaEventDestroy(stop_t);
    cudaEventDestroy(start_k); cudaEventDestroy(stop_k);

    return 0;
}

#else
int vonmises_cuda(const float*, const float*, const float*,
                  const float*, const float*, const float*,
                  float*, int, double*, double*) {
    return 1;
}
#endif
