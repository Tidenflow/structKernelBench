#pragma once

// ============================================================
// 多工况最大应力包络 Thrust 工业级参考实现
//
// 使用 NVIDIA Thrust 库的 thrust::for_each，
// 作为手写 CUDA kernel 的性能参考。
// ============================================================

int envelope_thrust(const float* h_sx,  const float* h_sy,  const float* h_sz,
                     const float* h_txy, const float* h_tyz, const float* h_tzx,
                     float* h_envelope, int M, int N,
                     double* kernel_time_ms, double* transfer_time_ms);
// 返回 0 成功, 非 0 失败
