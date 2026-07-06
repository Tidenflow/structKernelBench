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

// ---- GPU kernel ----
__global__ void spmv_cuda_kernel(const double* values, const int* col_idx,
                                  const int* row_ptr, const double* x,
                                  double* y, int N) {
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    if (i >= N) return;

    double sum = 0.0;
    int start = row_ptr[i];
    int end   = row_ptr[i + 1];
    for (int k = start; k < end; ++k) {
        sum += values[k] * x[col_idx[k]];
    }
    y[i] = sum;
}

// ---- Host wrapper ----
int spmv_cuda(const double* h_values, const int* h_col_idx,
              const int* h_row_ptr,   const double* h_x,
              double* h_y, int N, int nnz,
              double* kernel_time_ms, double* transfer_time_ms)
{
    double *d_values = nullptr, *d_x = nullptr, *d_y = nullptr;
    int    *d_col_idx = nullptr, *d_row_ptr = nullptr;
    cudaEvent_t start_t = nullptr, stop_t = nullptr;
    cudaEvent_t start_k = nullptr, stop_k = nullptr;

    // Alloc
    CUDA_CHECK(cudaMalloc(&d_values,  nnz * sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_col_idx, nnz * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_row_ptr, (N + 1) * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_x,       N * sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_y,       N * sizeof(double)));

    CUDA_CHECK(cudaEventCreate(&start_t));
    CUDA_CHECK(cudaEventCreate(&stop_t));
    CUDA_CHECK(cudaEventCreate(&start_k));
    CUDA_CHECK(cudaEventCreate(&stop_k));

    // H2D
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(d_values,  h_values,  nnz * sizeof(double), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_col_idx, h_col_idx, nnz * sizeof(int),    cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_row_ptr, h_row_ptr, (N + 1) * sizeof(int), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_x,       h_x,       N * sizeof(double),   cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_h2d = 0;
    cudaEventElapsedTime(&t_h2d, start_t, stop_t);

    // Launch
    int blockSize = 256;
    int gridSize  = (N + blockSize - 1) / blockSize;

    CUDA_CHECK(cudaEventRecord(start_k));
    spmv_cuda_kernel<<<gridSize, blockSize>>>(d_values, d_col_idx, d_row_ptr, d_x, d_y, N);
    CUDA_KERNEL_CHECK();
    CUDA_CHECK(cudaEventRecord(stop_k));
    CUDA_CHECK(cudaEventSynchronize(stop_k));
    float t_kernel = 0;
    cudaEventElapsedTime(&t_kernel, start_k, stop_k);

    // D2H
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(h_y, d_y, N * sizeof(double), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_d2h = 0;
    cudaEventElapsedTime(&t_d2h, start_t, stop_t);

    *kernel_time_ms   = static_cast<double>(t_kernel);
    *transfer_time_ms = static_cast<double>(t_h2d + t_d2h);

    // Cleanup
    cudaFree(d_values);
    cudaFree(d_col_idx);
    cudaFree(d_row_ptr);
    cudaFree(d_x);
    cudaFree(d_y);
    cudaEventDestroy(start_t);
    cudaEventDestroy(stop_t);
    cudaEventDestroy(start_k);
    cudaEventDestroy(stop_k);

    return 0;
}

#else
// ---- CUDA 不可用时的 stub ----
int spmv_cuda(const double*, const int*, const int*, const double*,
              double*, int, int, double*, double*) {
    return 1;  // 错误码
}
#endif // HAS_CUDA
