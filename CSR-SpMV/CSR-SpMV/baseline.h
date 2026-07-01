#pragma once

// ============================================================
// CSR-SpMV 基准实现 — 单线程 CPU 版本
//
// 计算 y = A * x，其中 A 以 CSR 格式存储。
//
// 参数:
//   values   - 非零元素数组，长度 row_ptr[N]
//   col_idx  - 每个非零元素的列号，长度 row_ptr[N]
//   row_ptr  - 每行起始偏移，长度 N+1
//   x        - 输入向量，长度 N
//   y        - 输出向量，长度 N（调用者分配）
//   N        - 矩阵行数 / 向量长度
//
// 注意: 本文件是算子核心代码，不依赖 bench_utils。
//       可直接复制到其他项目使用。
// ============================================================

void spmv_baseline(const double* values,
                   const int*    col_idx,
                   const int*    row_ptr,
                   const double* x,
                   double*       y,
                   int           N);
