cmake_minimum_required(VERSION 3.16)
project(StructKernelBench VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(BASE_FLAGS "-march=native -ffast-math")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${BASE_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

# ---------- CUDA arch (需要在 find_package 之前) ----------
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES "75-real;80-real;86-real;89-real")
endif()

# ---------- Qt6 ----------
find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

# ============================================================
# 可执行目标: StructKernelBench
# ============================================================
qt_add_executable(StructKernelBench
    # ---- common ----
    common/main.cpp
    common/bench_utils.cpp
    # ---- CSR-SpMV kernels ----
    CSR-SpMV/kernels/baseline.cpp
    CSR-SpMV/kernels/openmp.cpp
    CSR-SpMV/kernels/simd.cpp
    # ---- CSR-SpMV runner ----
    CSR-SpMV/spmv_runner.cpp
    # ---- CSR-SpMV UI ----
    CSR-SpMV/ui/BenchmarkPanel.cpp
    CSR-SpMV/ui/ResultChartView.cpp
    CSR-SpMV/ui/SpmvMainWidget.cpp
)

target_include_directories(StructKernelBench PRIVATE
    ${CMAKE_SOURCE_DIR}                    # common/bench_utils.h, CSR-SpMV/spmv_runner.h 等
    ${CMAKE_SOURCE_DIR}/CSR-SpMV/kernels   # baseline.h, openmp.h, simd.h, cuda_kernel.h
)

target_link_libraries(StructKernelBench PRIVATE
    Qt6::Widgets
    Qt6::Charts
)

# ---------- 可选: CUDA ----------
# 显式指定 /opt/cuda 搜索路径，即使 nvcc 不在 PATH 也能找到
if(NOT DEFINED CUDAToolkit_ROOT AND EXISTS "/opt/cuda/bin/nvcc")
    set(CUDAToolkit_ROOT "/opt/cuda")
endif()
find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_FLAGS_RELEASE "-O3")
    target_sources(StructKernelBench PRIVATE
        CSR-SpMV/kernels/cuda_kernel.cu
        CSR-SpMV/kernels/cusparse_kernel.cu
    )
    target_link_libraries(StructKernelBench PRIVATE
        CUDA::cusparse
    )
    target_compile_definitions(StructKernelBench PRIVATE HAS_CUDA)
    message(STATUS "CUDA Toolkit found: ${CUDAToolkit_VERSION}")
else()
    message(STATUS "CUDA Toolkit not found — GPU backend disabled")
endif()

# ---------- 可选: OpenMP ----------
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(StructKernelBench PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(StructKernelBench PRIVATE HAS_OPENMP)
    message(STATUS "OpenMP found")
else()
    message(STATUS "OpenMP not found — multi-threading disabled")
endif()
