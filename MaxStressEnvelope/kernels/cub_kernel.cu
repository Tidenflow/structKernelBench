#include "cub_kernel.h"

#ifdef HAS_CUDA

#include <cuda_runtime.h>
#include <cstdio>

#include <cub/cub.cuh>

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

// ---- Phase 1: 计算所有工况的 VM 应力 ----
__global__ void vm_all_kernel(const float* sx, const float* sy, const float* sz,
                               const float* txy, const float* tyz, const float* tzx,
                               float* vm_all, int M, int N) {
    int idx = blockDim.x * blockIdx.x + threadIdx.x;
    int total = M * N;
    if (idx >= total) return;

    float ds = sx[idx] - sy[idx];
    float ss = sy[idx] - sz[idx];
    float ts = sz[idx] - sx[idx];
    vm_all[idx] = sqrtf(0.5f * (ds*ds + ss*ss + ts*ts +
                                 6.0f * (txy[idx]*txy[idx] + tyz[idx]*tyz[idx] + tzx[idx]*tzx[idx])));
}

// ---- Phase 2 helper: 填充 segment offset 数组 ----
__global__ void fill_segment_offsets(int* d_offsets, int M, int N) {
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    if (i > M) return;   // 需要 M+1 个 offset (最后一个 = M*N)

    d_offsets[i] = i * N;
}

int envelope_cub(const float* h_sx,  const float* h_sy,  const float* h_sz,
                  const float* h_txy, const float* h_tyz, const float* h_tzx,
                  float* h_envelope, int M, int N,
                  double* kernel_time_ms, double* transfer_time_ms)
{
    size_t bytes_in = static_cast<size_t>(M) * N * sizeof(float);
    size_t bytes_out = M * sizeof(float);
    size_t offsets_bytes = static_cast<size_t>(M + 1) * sizeof(int);

    float *d_sx = nullptr, *d_sy = nullptr, *d_sz = nullptr;
    float *d_txy = nullptr, *d_tyz = nullptr, *d_tzx = nullptr;
    float *d_vm_all = nullptr;
    float *d_envelope = nullptr;
    int   *d_offsets = nullptr;
    void  *d_temp = nullptr;
    size_t temp_bytes = 0;

    cudaEvent_t start_t = nullptr, stop_t = nullptr;
    cudaEvent_t start_k = nullptr, stop_k = nullptr;

    CUDA_CHECK(cudaMalloc(&d_sx,       bytes_in));
    CUDA_CHECK(cudaMalloc(&d_sy,       bytes_in));
    CUDA_CHECK(cudaMalloc(&d_sz,       bytes_in));
    CUDA_CHECK(cudaMalloc(&d_txy,      bytes_in));
    CUDA_CHECK(cudaMalloc(&d_tyz,      bytes_in));
    CUDA_CHECK(cudaMalloc(&d_tzx,      bytes_in));
    CUDA_CHECK(cudaMalloc(&d_vm_all,   bytes_in));
    CUDA_CHECK(cudaMalloc(&d_envelope, bytes_out));
    CUDA_CHECK(cudaMalloc(&d_offsets,  offsets_bytes));

    CUDA_CHECK(cudaEventCreate(&start_t));
    CUDA_CHECK(cudaEventCreate(&stop_t));
    CUDA_CHECK(cudaEventCreate(&start_k));
    CUDA_CHECK(cudaEventCreate(&stop_k));

    // ---- H2D ----
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(d_sx,  h_sx,  bytes_in, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_sy,  h_sy,  bytes_in, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_sz,  h_sz,  bytes_in, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_txy, h_txy, bytes_in, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_tyz, h_tyz, bytes_in, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_tzx, h_tzx, bytes_in, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_h2d = 0;
    cudaEventElapsedTime(&t_h2d, start_t, stop_t);

    // ---- Phase 2 准备: 填充 segment offsets ----
    {
        int block = 256;
        int grid  = (M + 1 + block - 1) / block;
        fill_segment_offsets<<<grid, block>>>(d_offsets, M, N);
        CUDA_KERNEL_CHECK();
    }

    // ---- 获取 CUB temp storage 大小 ----
    cub::DeviceSegmentedReduce::Max(d_temp, temp_bytes,
                                     d_vm_all, d_envelope,
                                     M, d_offsets, d_offsets + 1);
    CUDA_CHECK(cudaMalloc(&d_temp, temp_bytes));

    // ---- Benchmark: 计时 kernel 部分 ----
    CUDA_CHECK(cudaEventRecord(start_k));

    // Phase 1: 逐元素计算所有 VM 应力
    {
        int total = M * N;
        int block = 256;
        int grid  = (total + block - 1) / block;
        vm_all_kernel<<<grid, block>>>(d_sx, d_sy, d_sz, d_txy, d_tyz, d_tzx, d_vm_all, M, N);
        CUDA_KERNEL_CHECK();
    }

    // Phase 2: CUB segmented max
    cub::DeviceSegmentedReduce::Max(d_temp, temp_bytes,
                                     d_vm_all, d_envelope,
                                     M, d_offsets, d_offsets + 1);
    CUDA_KERNEL_CHECK();

    CUDA_CHECK(cudaEventRecord(stop_k));
    CUDA_CHECK(cudaEventSynchronize(stop_k));
    float t_kernel = 0;
    cudaEventElapsedTime(&t_kernel, start_k, stop_k);

    // ---- D2H ----
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(h_envelope, d_envelope, bytes_out, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_d2h = 0;
    cudaEventElapsedTime(&t_d2h, start_t, stop_t);

    *kernel_time_ms   = static_cast<double>(t_kernel);
    *transfer_time_ms = static_cast<double>(t_h2d + t_d2h);

    cudaFree(d_sx);  cudaFree(d_sy);  cudaFree(d_sz);
    cudaFree(d_txy); cudaFree(d_tyz); cudaFree(d_tzx);
    cudaFree(d_vm_all); cudaFree(d_envelope);
    cudaFree(d_offsets); cudaFree(d_temp);
    cudaEventDestroy(start_t); cudaEventDestroy(stop_t);
    cudaEventDestroy(start_k); cudaEventDestroy(stop_k);

    return 0;
}

#else
int envelope_cub(const float*, const float*, const float*,
                  const float*, const float*, const float*,
                  float*, int, int, double*, double*) {
    return 1;
}
#endif
