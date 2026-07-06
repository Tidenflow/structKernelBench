#include "openmp.h"
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

void vonmises_openmp(const float* sx,  const float* sy,  const float* sz,
                     const float* txy, const float* tyz, const float* tzx,
                     float* vm, int N)
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (int i = 0; i < N; ++i) {
        float ds = sx[i] - sy[i];
        float ss = sy[i] - sz[i];
        float ts = sz[i] - sx[i];
        vm[i] = std::sqrt(0.5f * (ds*ds + ss*ss + ts*ts +
                                   6.0f * (txy[i]*txy[i] + tyz[i]*tyz[i] + tzx[i]*tzx[i])));
    }
}
