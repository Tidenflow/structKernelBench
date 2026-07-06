#pragma once

// ============================================================
// 多工况最大应力包络 OpenMP 多线程版本
// ============================================================

void envelope_openmp(const float* sx,  const float* sy,  const float* sz,
                      const float* txy, const float* tyz, const float* tzx,
                      float* envelope, int M, int N);
