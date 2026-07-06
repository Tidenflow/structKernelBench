#include "simd.h"
#include <cmath>
#include <algorithm>

// 内层循环连续访存、无分支 — 编译器自动向量化的理想场景。
// -O3 -march=native -ffast-math 下应生成 AVX2/AVX-512 指令。

void envelope_simd(const float* sx,  const float* sy,  const float* sz,
                    const float* txy, const float* tyz, const float* tzx,
                    float* envelope, int M, int N)
{
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
