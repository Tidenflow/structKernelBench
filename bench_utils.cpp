#include "bench_utils.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// ---------- Timer ----------

void Timer::start() {
    t0 = clock::now();
}

double Timer::stop_ms() {
    auto end = clock::now();
    return std::chrono::duration<double, std::milli>(end - t0).count();
}

// ---------- CsvWriter ----------

CsvWriter::CsvWriter(const std::string& filepath) : path(filepath) {
    namespace fs = std::filesystem;
    fs::path p(filepath);
    if (!p.parent_path().empty()) {
        fs::create_directories(p.parent_path());
    }
    // 检查是否已有内容
    std::ifstream f(path);
    header_written = f.good() && f.peek() != std::ifstream::traits_type::eof();
}

void CsvWriter::write_row(const std::map<std::string, std::string>& fields) {
    std::ofstream f(path, std::ios::app);
    if (!f) {
        std::cerr << "[ERROR] Cannot open CSV: " << path << std::endl;
        return;
    }

    // 第一次写入：先写表头
    if (!header_written) {
        bool first = true;
        for (auto& [key, _] : fields) {
            if (!first) f << ",";
            f << key;
            first = false;
        }
        f << "\n";
        header_written = true;
    }

    // 写数据行（按表头顺序）
    bool first = true;
    for (auto& [_, val] : fields) {
        if (!first) f << ",";
        f << val;
        first = false;
    }
    f << "\n";
}

// ---------- 小工具 ----------

std::string now_str() {
    auto t = std::time(nullptr);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

double median(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n % 2 == 1)
        return v[n / 2];
    else
        return (v[n / 2 - 1] + v[n / 2]) * 0.5;
}
