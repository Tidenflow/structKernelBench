#pragma once

#include <string>

// ============================================================
// BenchmarkResult — 单次 benchmark 运行的全部结果数据
//
// 设计原则: runner 只负责计算和填充此 struct，
// 不负责展示（GUI）或持久化（CSV）。调用者自行决定如何使用。
// ============================================================
struct BenchmarkResult {
    std::string kernel      = "spmv";
    std::string backend;
    int         rows        = 0;
    int         nnz         = 0;
    int         nnz_per_row = 0;
    int         threads     = 1;
    double      time_ms     = 0.0;
    double      time_min    = 0.0;
    double      time_max    = 0.0;
    double      gflops      = 0.0;
    double      max_error   = 0.0;
    bool        validated   = false;
    std::string timestamp;
};

// ============================================================
// 运行一次 CSR-SpMV benchmark
//
// 参数:
//   rows         - 矩阵行数
//   nnz_per_row  - 每行非零元数（近似）
//   backend      - "baseline" | "openmp" | ...（目前仅 baseline）
//   result_out   - 输出：填充完整结果
//
// 返回值: 0 = 成功, 非 0 = 验证失败
//
// 线程安全: 本函数可在线程中调用，不操作 GUI。
// 注意:    内部使用 rand()，多线程同时调用需注意。
// ============================================================
int run_spmv_benchmark(int rows, int nnz_per_row,
                       const std::string& backend,
                       BenchmarkResult& result_out);
