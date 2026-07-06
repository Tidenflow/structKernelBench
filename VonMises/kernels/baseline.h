#pragma once

// ============================================================
// Von Mises 等效应力 — CPU 单线程 baseline
//
// σ_vm = sqrt(0.5 * [(sx-sy)² + (sy-sz)² + (sz-sx)² + 6(txy²+tyz²+tzx²)])
//
// 输入: 6 个应力分量数组 (SoA 布局)
// 输出: vm, 长度 N
// ============================================================

void vonmises_baseline(const float* sx,  const float* sy,  const float* sz,
                       const float* txy, const float* tyz, const float* tzx,
                       float* vm, int N);
