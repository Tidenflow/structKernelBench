#pragma once

// ============================================================
// Von Mises SIMD / 编译器自动向量化版本
//
// 纯计算无分支无间接访存，编译器可轻松生成 AVX2/AVX-512。
// ============================================================

void vonmises_simd(const float* sx,  const float* sy,  const float* sz,
                   const float* txy, const float* tyz, const float* tzx,
                   float* vm, int N);
