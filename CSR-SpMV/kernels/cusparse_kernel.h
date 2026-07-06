#pragma once

// ============================================================
// CSR-SpMV cuSPARSE 工业级参考实现
//
// 使用 NVIDIA cuSPARSE 库的 cusparseSpMV()，
// 作为手写 CUDA kernel 的性能上界参考。
// ============================================================

int spmv_cusparse(const double* h_values, const int* h_col_idx,
                  const int* h_row_ptr,   const double* h_x,
                  double* h_y, int N, int nnz,
                  double* kernel_time_ms, double* transfer_time_ms);
// 返回 0 成功, 非 0 失败
