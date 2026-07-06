#pragma once

// ============================================================
// 多工况最大应力包络 SIMD / 编译器自动向量化版本
//
// 内层循环 (遍历工况 j) 连续访存，编译器可自动向量化。
// -O3 -march=native -ffast-math 下应生成 AVX2/AVX-512 指令。
// ============================================================

void envelope_simd(const float* sx,  const float* sy,  const float* sz,
                    const float* txy, const float* tyz, const float* tzx,
                    float* envelope, int M, int N);
