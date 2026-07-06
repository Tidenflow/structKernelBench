#pragma once

// ============================================================
// 多工况最大应力包络 CUB 工业级参考实现
//
// 使用 NVIDIA CUB 库的 DeviceSegmentedReduce::Max，
// 两阶段: (1) 逐元素计算 VM → (2) CUB 分段归约取最大。
//
// CUB 是 cuSPARSE/Thrust/cuBLAS 等库的底层原语库，
// 其分段归约经过极致优化（warp-level reduce, 共享内存等）。
// ============================================================

int envelope_cub(const float* h_sx,  const float* h_sy,  const float* h_sz,
                  const float* h_txy, const float* h_tyz, const float* h_tzx,
                  float* h_envelope, int M, int N,
                  double* kernel_time_ms, double* transfer_time_ms);
// 返回 0 成功, 非 0 失败
