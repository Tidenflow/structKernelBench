#include "simd.h"

// 注意: CSR 格式下 x[col_idx[k]] 是间接访存 (gather)，
// 编译器向量化能力有限。真正发挥 SIMD 需要 ELL/SELL 等列对齐格式。
// 此处 #pragma GCC ivdep 提示编译器忽略循环携带依赖，
// 配合 -O3 -march=native -ffast-math 尽力向量化。

void spmv_simd(const double* values,
               const int*    col_idx,
               const int*    row_ptr,
               const double* x,
               double*       y,
               int           N)
{
    for (int i = 0; i < N; ++i) {
        double sum = 0.0;
        int start  = row_ptr[i];
        int end    = row_ptr[i + 1];
        for (int k = start; k < end; ++k) {
            sum += values[k] * x[col_idx[k]];
        }
        y[i] = sum;
    }
}
