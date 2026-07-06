#pragma once

// ============================================================
// Von Mises OpenMP 多线程版本
// ============================================================

void vonmises_openmp(const float* sx,  const float* sy,  const float* sz,
                     const float* txy, const float* tyz, const float* tzx,
                     float* vm, int N);
