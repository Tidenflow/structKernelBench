#pragma once

// ============================================================
// Von Mises CUDA GPU 版本
//
// 单元素单线程，完美并行。内存合并访问，无 bank conflict。
// ============================================================

int vonmises_cuda(const float* h_sx,  const float* h_sy,  const float* h_sz,
                  const float* h_txy, const float* h_tyz, const float* h_tzx,
                  float* h_vm, int N,
                  double* kernel_time_ms, double* transfer_time_ms);
// 返回 0 成功, 非 0 失败
