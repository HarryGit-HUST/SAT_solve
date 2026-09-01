cmake_minimum_required(VERSION 3.10)
project(SAT_Solver LANGUAGES C CXX)

# 设置 C++ 标准为 C++14 (兼容 std::chrono、fstream 等)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 默认编译类型为 Release (带有 -O3 优化，测耗时必须开优化)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译器优化与告警选项配置
if(MSVC)
    add_compile_options(/W3 /O2)
else()
    # GCC / Clang 开启 -O3 高度优化与全面警告
    add_compile_options(-Wall -Wextra -O3)
endif()

# 包含头文件目录
include_directories(include)

# 自动收集 src 目录下的所有 .cpp 和 .c 源码文件
# （后续新增 dpll_optimized.cpp、sudoku.cpp 时无需手动修改 CMakeLists）
file(GLOB SOURCES 
    "src/*.cpp" 
    "src/*.c"
)

# 生成可执行文件
add_executable(sat_solver ${SOURCES})

# 打印编译配置信息
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Source Files: ${SOURCES}")