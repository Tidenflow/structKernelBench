#pragma once

#include <string>

// ============================================================
// VonMisesResult — 单次 benchmark 运行结果
// ============================================================
struct VonMisesResult {
    std::string kernel       = "vonmises";
    std::string backend;
    int         num_elements = 0;
    int         threads      = 1;
    double      time_ms      = 0.0;
    double      time_min     = 0.0;
    double      time_max     = 0.0;
    double      throughput   = 0.0;     // 百万元素/秒
    double      max_error    = 0.0;
    bool        validated    = false;
    std::string timestamp;
};

// ============================================================
// 运行一次 Von Mises benchmark
// ============================================================
int run_vonmises_benchmark(int num_elements,
                           const std::string& backend,
                           VonMisesResult& result_out);
