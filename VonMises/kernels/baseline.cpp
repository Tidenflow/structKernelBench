#include "baseline.h"
#include <cmath>

void vonmises_baseline(const float* sx,  const float* sy,  const float* sz,
                       const float* txy, const float* tyz, const float* tzx,
                       float* vm, int N)
{
    for (int i = 0; i < N; ++i) {
        float ds = sx[i] - sy[i];   // σx - σy
        float ss = sy[i] - sz[i];   // σy - σz
        float ts = sz[i] - sx[i];   // σz - σx
        vm[i] = std::sqrt(0.5f * (ds*ds + ss*ss + ts*ts +
                                   6.0f * (txy[i]*txy[i] + tyz[i]*tyz[i] + tzx[i]*tzx[i])));
    }
}
