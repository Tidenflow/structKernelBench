# StructKernelBench

面向结构仿真的 CPU/GPU 核心算子性能优化实验平台。

**非完整 CAE 软件，不做完整结构求解器**——从工业结构仿真流程中抽取典型计算热点，针对 CPU 单线程、OpenMP 多线程、SIMD/编译优化和 CUDA GPU 四种并行策略进行性能对比与底层分析。

---

## 项目概述

在船舶、航空航天、汽车、机械结构等工业仿真场景中，CAE 软件通常包含前处理、求解、后处理三个阶段。完整 CAE 软件极其复杂，但其中存在大量可独立优化的**计算核心（算子）**。

本项目选取结构仿真中的 **1 个求解阶段算子** 和 **2 个后处理阶段算子**，分别实现多种并行后端，对比不同并行策略、数据布局和编译优化选项对性能的影响。

### 什么是算子

> 一段输入输出明确、可重复执行、适合独立优化的计算步骤。

在 AI 编译器中，常见算子包括 MatMul、Conv、Softmax；在结构仿真中，同样存在类似的计算核心——稀疏矩阵向量乘、von Mises 应力计算、多工况最大值包络等。

---

## 三个核心算子

| 编号 | 算子名称 | 所属阶段 | 业务意义 | 技术重点 |
|---|---|---|---|---|
| 1 | CSR-SpMV | 求解阶段 | 有限元迭代求解的基础计算热点 | 稀疏矩阵存储、访存优化、OpenMP/CUDA |
| 2 | von Mises 等效应力 | 后处理阶段 | 将多分量应力合成为危险程度指标 | 数据并行、SIMD、AoS/SoA、CUDA |
| 3 | 多工况最大应力包络 | 后处理阶段 | 多载荷工况中筛选最危险结果 | 并行归约、GPU reduction |

---

## 算子一：CSR-SpMV 稀疏矩阵向量乘

### 业务背景

在结构有限元分析中，形成大型线性方程组 **K·u = F**，其中 K 为结构刚度矩阵。由于每个节点通常只与附近节点有关，K 非常大但大部分元素为 0，属于稀疏矩阵。

迭代求解过程中反复执行 **y = A × x**，即 SpMV（Sparse Matrix-Vector Multiplication）。

### 优化挑战

- 矩阵大但非零元素分布不规则
- 访存不连续，CPU cache 不友好
- GPU 上 memory coalescing 可能不佳
- 通常是内存带宽受限而非计算受限

### 实现版本

1. CPU baseline：单线程 CSR-SpMV
2. OpenMP：按行并行的 CSR-SpMV
3. CUDA：每行一个线程或一个 warp 的 SpMV
4. 后续扩展：ELL / COO / HYB 格式对比

---

## 算子二：von Mises 等效应力计算

### 业务背景

求解器输出每个单元的 6 个应力分量（σx, σy, σz, τxy, τyz, τxz）。工程师需要综合指标判断该位置是否危险——von Mises 等效应力将 6 个分量合成为一个标量。

### 优化特点

单元之间互不依赖，天然适合并行：

- CPU 多线程：多个线程分别处理不同区间的单元
- GPU 并行：一个 GPU 线程负责一个或多个单元
- SIMD：一条向量指令同时处理多个单元

### 数据布局优化

**AoS（Array of Structs）**：

```cpp
struct Stress { float sx, sy, sz, txy, tyz, txz; };
Stress data[N];  // xyz xyz xyz ... 交叉存储
```

**SoA（Struct of Arrays）**：

```cpp
struct StressField { float *sx, *sy, *sz, *txy, *tyz, *txz; };
// xxx... yyy... zzz... 分量连续存储
```

本项目对比两种布局在 CPU / OpenMP / CUDA 下的性能差异。

### 实现版本

1. CPU baseline：普通 for 循环
2. OpenMP：多线程并行
3. SIMD / 编译器自动向量化
4. CUDA：GPU kernel 并行计算
5. AoS 与 SoA 数据布局对比

---

## 算子三：多工况最大应力包络

### 业务背景

真实仿真通常有多个载荷工况（case_001 ~ case_050）。工程师关心每个单元在所有工况中的最大应力、最危险工况、全局最大值以及超阈值单元。

### 优化特点

100 万单元 × 50 工况 = 5000 万数值需要处理。每个单元的最大值计算相互独立，非常适合 OpenMP 和 CUDA 并行。

### 实现版本

1. CPU baseline：双重循环
2. OpenMP：按单元并行
3. CUDA：GPU 并行 reduction
4. 后续扩展：Top-K 危险单元筛选

---

## 技术路线

### 计算后端

```
CPU baseline → OpenMP 多线程 → SIMD / 编译器自动向量化 → CUDA GPU
```

### 编译优化分析

对比不同编译选项：

| 选项 | 作用 |
|---|---|
| `-O0` / `-O2` / `-O3` / `-Ofast` | 优化级别 |
| `-march=native` | 针对本机 CPU 指令集 |
| `-ffast-math` | 激进浮点优化 |
| `-fopenmp` | 启用 OpenMP |

分析内容：不同优化级别下的运行时间、编译器自动向量化情况、循环展开、SIMD 指令生成。

### 底层分析方向

**CPU 侧**：cache 命中率、内存带宽、SIMD 指令、自动向量化、线程调度、负载均衡

**GPU 侧**：thread/block 映射、global memory 访问、memory coalescing、shared memory、warp divergence、occupancy、Host-Device 数据传输开销

---

## 项目结构

项目采用**每个算子独立文件夹**的结构——优先保证清晰可读，共享部分仅限工具函数。

```
StructKernelBench/
├── README.md
├── CMakeLists.txt
├── include/
│   └── bench_utils.h            # 共享：计时器、CSV 输出、参数解析
├── src/
│   ├── bench_utils.cpp
│   └── main.cpp                 # 统一入口
├── CSR-SpMV/                    # 算子一：稀疏矩阵向量乘
│   ├── baseline.cpp
│   ├── openmp.cpp
│   ├── simd.cpp
│   ├── cuda_kernel.cu
│   └── spmv_runner.cpp
├── VonMises/                    # 算子二：von Mises 等效应力
│   ├── baseline.cpp
│   ├── openmp.cpp
│   ├── simd.cpp
│   ├── cuda_kernel.cu
│   └── vonmises_runner.cpp
├── MaxStressEnvelope/           # 算子三：多工况最大应力包络
│   ├── baseline.cpp
│   ├── openmp.cpp
│   ├── simd.cpp
│   ├── cuda_kernel.cu
│   └── envelope_runner.cpp
├── scripts/
│   ├── run_benchmark.sh
│   └── plot_results.py          # 读取 CSV，输出性能折线图
└── results/
    ├── benchmark.csv
    └── figures/
```

---

## Benchmark 设计

### 数据规模

| 算子 | 规模 |
|---|---|
| von Mises | 10⁵ / 10⁶ / 10⁷ 单元 |
| 多工况包络 | 10⁵ / 10⁶ / 10⁷ 单元 × 10 / 50 / 100 工况 |
| SpMV | 10⁴ / 10⁵ / 10⁶ 行，每行 5 / 10 / 20 非零元 |

### 输出指标

`time_ms` · `throughput` · `speedup` · `cpu_threads` · `gpu_kernel_time` · `host_device_transfer_time` · `max_error` · `compiler_flags` · `data_layout`

### benchmark.csv 示例

```csv
kernel,backend,data_size,case_count,layout,threads,time_ms,speedup,compiler_flags
von_mises,cpu,1000000,1,aos,1,120.5,1.0,-O3
von_mises,openmp,1000000,1,aos,8,24.3,4.96,-O3 -fopenmp
von_mises,cuda,1000000,1,soa,0,3.8,31.7,nvcc -O3
envelope,cpu,1000000,50,soa,1,850.2,1.0,-O3
envelope,openmp,1000000,50,soa,8,140.6,6.05,-O3 -fopenmp
spmv,cpu,1000000,0,csr,1,65.1,1.0,-O3
```

---

## 开发路线

推荐按以下顺序推进（从易到难）：

| 阶段 | 内容 |
|---|---|
| v0.1 | CMake 工程搭建、计时器、CSV 输出、von Mises CPU baseline |
| v0.2 | von Mises OpenMP + 多工况包络 OpenMP |
| v0.3 | von Mises CUDA + 多工况包络 CUDA |
| v0.4 | AoS / SoA 数据布局对比 |
| v0.5 | CSR-SpMV CPU / OpenMP / CUDA |
| v0.6 | 编译优化分析、向量化报告、性能图表自动生成 |

---

## 构建与运行

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./StructKernelBench
```

CUDA 版本需安装 CUDA Toolkit，CMake 会自动检测。若未检测到 CUDA，仅编译 CPU 相关后端。

---

## 许可

MIT License
