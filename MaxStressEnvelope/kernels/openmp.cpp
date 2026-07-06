#include "openmp.h"
#include <cmath>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

void envelope_openmp(const float* sx,  const float* sy,  const float* sz,
                      const float* txy, const float* tyz, const float* tzx,
                      float* envelope, int M, int N)
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (int i = 0; i < M; ++i) {
        float max_vm = 0.0f;
        for (int j = 0; j < N; ++j) {
            int idx = i * N + j;
            float ds = sx[idx] - sy[idx];
            float ss = sy[idx] - sz[idx];
            float ts = sz[idx] - sx[idx];
            float vm = std::sqrt(0.5f * (ds*ds + ss*ss + ts*ts +
                                         6.0f * (txy[idx]*txy[idx] + tyz[idx]*tyz[idx] + tzx[idx]*tzx[idx])));
            max_vm = std::max(max_vm, vm);
        }
        envelope[i] = max_vm;
    }
}
