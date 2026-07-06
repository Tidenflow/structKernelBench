#pragma once

#include <string>

// ============================================================
// EnvelopeResult — 单次 benchmark 运行结果
// ============================================================
struct EnvelopeResult {
    std::string kernel         = "MaxStressEnvelope";
    std::string backend;
    int         num_elements   = 0;
    int         num_load_cases = 0;
    int         threads        = 1;
    double      time_ms        = 0.0;
    double      time_min       = 0.0;
    double      time_max       = 0.0;
    double      throughput     = 0.0;     // 百万元素/秒
    double      max_error      = 0.0;
    bool        validated      = false;
    std::string timestamp;
};

// ============================================================
// 运行一次 MaxStressEnvelope benchmark
// ============================================================
int run_envelope_benchmark(int num_elements,
                            int num_load_cases,
                            const std::string& backend,
                            EnvelopeResult& result_out);
