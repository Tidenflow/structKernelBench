cmake_minimum_required(VERSION 3.16)
project(CSRSpMV VERSION 0.2 LANGUAGES CXX)

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

qt_add_executable(CSRSpMV
    main.cpp
    bench_utils.cpp
    ui/BenchmarkPanel.cpp
    ui/ResultChartView.cpp
    ui/MainWindow.cpp
    CSR-SpMV/CSR-SpMV/baseline.cpp
    CSR-SpMV/openmp/openmp.cpp
    CSR-SpMV/simd/simd.cpp
    CSR-SpMV/spmv_runner.cpp
)

target_include_directories(CSRSpMV PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}/CSR-SpMV
    ${CMAKE_SOURCE_DIR}/CSR-SpMV/CSR-SpMV
    ${CMAKE_SOURCE_DIR}/CSR-SpMV/openmp
    ${CMAKE_SOURCE_DIR}/CSR-SpMV/simd
    ${CMAKE_SOURCE_DIR}/CSR-SpMV/cuda
)

target_link_libraries(CSRSpMV PRIVATE
    Qt6::Widgets
    Qt6::Charts
)

# ---------- 可选: CUDA ----------
find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_FLAGS_RELEASE "-O3")
    target_sources(CSRSpMV PRIVATE
        CSR-SpMV/cuda/cuda_kernel.cu
    )
    target_compile_definitions(CSRSpMV PRIVATE HAS_CUDA)
    message(STATUS "CUDA Toolkit found: ${CUDAToolkit_VERSION}")
else()
    message(STATUS "CUDA Toolkit not found — GPU backend disabled")
endif()

# ---------- 可选: OpenMP ----------
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(CSRSpMV PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(CSRSpMV PRIVATE HAS_OPENMP)
    message(STATUS "OpenMP found")
else()
    message(STATUS "OpenMP not found — multi-threading disabled")
endif()
