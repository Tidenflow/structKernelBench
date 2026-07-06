#pragma once

// ============================================================
// 多工况最大应力包络 CUDA GPU 版本
//
// 单元素单线程，每线程串行遍历 N 个工况取最大 VM。
// 全无分支、连续访存、有 coalescing。
// ============================================================

int envelope_cuda(const float* h_sx,  const float* h_sy,  const float* h_sz,
                   const float* h_txy, const float* h_tyz, const float* h_tzx,
                   float* h_envelope, int M, int N,
                   double* kernel_time_ms, double* transfer_time_ms);
// 返回 0 成功, 非 0 失败
