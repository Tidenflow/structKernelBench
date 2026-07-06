#pragma once

// ============================================================
// CSR-SpMV OpenMP 多线程版本
//
// 外层行循环 #pragma omp parallel for
// 每行计算独立，无数据竞争，无需同步。
// ============================================================

void spmv_openmp(const double* values,
                 const int*    col_idx,
                 const int*    row_ptr,
                 const double* x,
                 double*       y,
                 int           N);
