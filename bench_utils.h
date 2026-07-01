#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

// ============================================================
// 计时器 — 基于 std::chrono::high_resolution_clock
// ============================================================
struct Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point t0;

    void start();
    double stop_ms();   // 返回自 start() 到现在的毫秒数
};

// ============================================================
// CSV 追加写入 — 自动建目录、写表头
// ============================================================
struct CsvWriter {
    std::string path;
    bool header_written = false;

    explicit CsvWriter(const std::string& filepath);
    void write_row(const std::map<std::string, std::string>& fields);
};

// ============================================================
// 小工具
// ============================================================
std::string now_str();                            // "2026-07-01 12:00:00"
double      median(std::vector<double>& v);        // 中位数（会排序）
