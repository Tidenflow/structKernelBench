#pragma once

// ============================================================
// CSR-SpMV CUDA GPU 版本
//
// 单行单线程: y[i] = Σ values[k] * x[col_idx[k]]
// host wrapper 负责 device mem 管理 + kernel 启动
// ============================================================

int spmv_cuda(const double* h_values, const int* h_col_idx,
              const int* h_row_ptr,   const double* h_x,
              double* h_y, int N, int nnz,
              double* kernel_time_ms, double* transfer_time_ms);
// 返回 0 成功, 非 0 失败
