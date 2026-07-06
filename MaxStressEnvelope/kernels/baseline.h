#pragma once

// ============================================================
// 多工况最大应力包络 — CPU 单线程 baseline
//
// 对每个单元, 计算 N 个工况的 Von Mises 应力, 取最大包络。
//
// 数据布局: 元素优先 (element-major)
//   sx[i*N + j] = 第 i 单元在第 j 工况的 σx
//   其余分量同。
//
// 输入: sx/sy/sz/txy/tyz/tzx 各 M*N 个 float
// 输出: envelope, 长度 M
// ============================================================

void envelope_baseline(const float* sx,  const float* sy,  const float* sz,
                        const float* txy, const float* tyz, const float* tzx,
                        float* envelope, int M, int N);
