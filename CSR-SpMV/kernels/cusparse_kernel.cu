#include "cusparse_kernel.h"

#ifdef HAS_CUDA

#include <cuda_runtime.h>
#include <cusparse.h>
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

#define CUSPARSE_CHECK(call) do {                          \
    cusparseStatus_t err = call;                           \
    if (err != CUSPARSE_STATUS_SUCCESS) {                  \
        std::fprintf(stderr, "[cuSPARSE ERROR] %s:%d %s: %d\n", \
                     __FILE__, __LINE__, #call, (int)err);  \
        return 1;                                          \
    }                                                      \
} while(0)

// ---- Host wrapper ----
int spmv_cusparse(const double* h_values, const int* h_col_idx,
                  const int* h_row_ptr,   const double* h_x,
                  double* h_y, int N, int nnz,
                  double* kernel_time_ms, double* transfer_time_ms)
{
    // 持久化 cuSPARSE handle，避免每次调用重建
    static cusparseHandle_t cusparse_handle = nullptr;
    if (!cusparse_handle) {
        CUSPARSE_CHECK(cusparseCreate(&cusparse_handle));
    }

    double *d_values = nullptr, *d_x = nullptr, *d_y = nullptr;
    int    *d_col_idx = nullptr, *d_row_ptr = nullptr;
    void   *d_buffer  = nullptr;
    cusparseSpMatDescr_t matA = nullptr;
    cusparseDnVecDescr_t vecX = nullptr, vecY = nullptr;

    cudaEvent_t start_t = nullptr, stop_t = nullptr;
    cudaEvent_t start_k = nullptr, stop_k = nullptr;

    // ---- Device alloc ----
    CUDA_CHECK(cudaMalloc(&d_values,  nnz * sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_col_idx, nnz * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_row_ptr, (N + 1) * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_x,       N * sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_y,       N * sizeof(double)));

    CUDA_CHECK(cudaEventCreate(&start_t));
    CUDA_CHECK(cudaEventCreate(&stop_t));
    CUDA_CHECK(cudaEventCreate(&start_k));
    CUDA_CHECK(cudaEventCreate(&stop_k));

    // ---- H2D transfer ----
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(d_values,  h_values,  nnz * sizeof(double), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_col_idx, h_col_idx, nnz * sizeof(int),    cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_row_ptr, h_row_ptr, (N + 1) * sizeof(int), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_x,       h_x,       N * sizeof(double),   cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_h2d = 0;
    cudaEventElapsedTime(&t_h2d, start_t, stop_t);

    // ---- 构建 cuSPARSE 矩阵描述符 ----
    CUSPARSE_CHECK(cusparseCreateCsr(
        &matA, N, N, nnz,
        d_row_ptr, d_col_idx, d_values,
        CUSPARSE_INDEX_32I, CUSPARSE_INDEX_32I,
        CUSPARSE_INDEX_BASE_ZERO, CUDA_R_64F));

    // ---- 构建向量描述符 ----
    CUSPARSE_CHECK(cusparseCreateDnVec(&vecX, N, d_x, CUDA_R_64F));
    CUSPARSE_CHECK(cusparseCreateDnVec(&vecY, N, d_y, CUDA_R_64F));

    // ---- 查询 buffer 大小 ----
    const double alpha = 1.0;
    const double beta  = 0.0;
    size_t buffer_size = 0;

    CUSPARSE_CHECK(cusparseSpMV_bufferSize(
        cusparse_handle, CUSPARSE_OPERATION_NON_TRANSPOSE,
        &alpha, matA, vecX, &beta, vecY,
        CUDA_R_64F, CUSPARSE_SPMV_ALG_DEFAULT, &buffer_size));

    CUDA_CHECK(cudaMalloc(&d_buffer, buffer_size));

    // ---- Kernel launch (计时) ----
    CUDA_CHECK(cudaEventRecord(start_k));
    CUSPARSE_CHECK(cusparseSpMV(
        cusparse_handle, CUSPARSE_OPERATION_NON_TRANSPOSE,
        &alpha, matA, vecX, &beta, vecY,
        CUDA_R_64F, CUSPARSE_SPMV_ALG_DEFAULT, d_buffer));
    CUDA_CHECK(cudaEventRecord(stop_k));
    CUDA_CHECK(cudaEventSynchronize(stop_k));
    float t_kernel = 0;
    cudaEventElapsedTime(&t_kernel, start_k, stop_k);

    // ---- D2H transfer ----
    CUDA_CHECK(cudaEventRecord(start_t));
    CUDA_CHECK(cudaMemcpy(h_y, d_y, N * sizeof(double), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(stop_t));
    CUDA_CHECK(cudaEventSynchronize(stop_t));
    float t_d2h = 0;
    cudaEventElapsedTime(&t_d2h, start_t, stop_t);

    *kernel_time_ms   = static_cast<double>(t_kernel);
    *transfer_time_ms = static_cast<double>(t_h2d + t_d2h);

    // ---- Cleanup ----
    cusparseDestroySpMat(matA);
    cusparseDestroyDnVec(vecX);
    cusparseDestroyDnVec(vecY);
    cudaFree(d_buffer);
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
int spmv_cusparse(const double*, const int*, const int*, const double*,
                  double*, int, int, double*, double*) {
    return 1;
}
#endif // HAS_CUDA
