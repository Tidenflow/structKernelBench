#include "openmp.h"

#ifdef _OPENMP
#include <omp.h>
#endif

void spmv_openmp(const double* values,
                 const int*    col_idx,
                 const int*    row_ptr,
                 const double* x,
                 double*       y,
                 int           N)
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif
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
