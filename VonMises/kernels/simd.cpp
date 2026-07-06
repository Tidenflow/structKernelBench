#include "simd.h"
#include <cmath>

// 纯计算、连续访存、无分支 — 编译器自动向量化的理想场景。
// -O3 -march=native -ffast-math 下应生成 AVX2/AVX-512 指令。

void vonmises_simd(const float* sx,  const float* sy,  const float* sz,
                   const float* txy, const float* tyz, const float* tzx,
                   float* vm, int N)
{
    for (int i = 0; i < N; ++i) {
        float ds = sx[i] - sy[i];
        float ss = sy[i] - sz[i];
        float ts = sz[i] - sx[i];
        vm[i] = std::sqrt(0.5f * (ds*ds + ss*ss + ts*ts +
                                   6.0f * (txy[i]*txy[i] + tyz[i]*tyz[i] + tzx[i]*tzx[i])));
    }
}
