#pragma once

// ============================================================
// CSR-SpMV SIMD / 编译器自动向量化版本
//
// 内层循环 #pragma omp simd reduction(+:sum)
// 让编译器尝试将 sum 累加向量化 (AVX2/AVX-512)
//
// 注意: x[col_idx[k]] 是间接访存 (gather)，
// CSR 格式下 SIMD 收益有限。
// ============================================================

void spmv_simd(const double* values,
               const int*    col_idx,
               const int*    row_ptr,
               const double* x,
               double*       y,
               int           N);
