# ============================================================================
# CMake 最低版本要求
# 语法: cmake_minimum_required(VERSION <版本号>)
# 作用: 指定构建此项目所需的 CMake 最低版本
# ============================================================================
cmake_minimum_required(VERSION 3.14)

# ============================================================================
# 项目定义
# 语法: project(<项目名> VERSION <版本> LANGUAGES <语言>)
# 作用: 定义项目名称、版本号和使用的编程语言
# 会自动设置变量: PROJECT_NAME, PROJECT_VERSION, PROJECT_SOURCE_DIR 等
# ============================================================================
project(code_socket VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# 生成 compile_commands.json
# 作用: 生成编译数据库，供 IDE（如 clangd、VS Code）进行代码分析
# 文件位置: ${CMAKE_BINARY_DIR}/compile_commands.json (即 build/ 目录下)
# ============================================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# C++ 标准设置
# CMAKE_CXX_STANDARD: 指定 C++ 标准版本 (11, 14, 17, 20, 23)
# CMAKE_CXX_STANDARD_REQUIRED: ON 表示必须使用指定标准，否则报错
# CMAKE_CXX_EXTENSIONS: OFF 表示禁用编译器扩展，使用纯标准 C++
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# 编译选项
# -Wall: 开启所有常见警告
# -Wextra: 开启额外警告
# -pthread: 启用 POSIX 线程支持
# ============================================================================
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# ============================================================================
# 设置输出目录
# CMAKE_ARCHIVE_OUTPUT_DIRECTORY: 静态库(.a)输出目录
# CMAKE_LIBRARY_OUTPUT_DIRECTORY: 动态库(.so)输出目录
# CMAKE_RUNTIME_OUTPUT_DIRECTORY: 可执行文件输出目录
# CMAKE_BINARY_DIR: 构建目录的根路径 (即 build/ 目录)
# ============================================================================
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# ============================================================================
# 添加子模块（嵌套编译的核心）
# 语法: add_subdirectory(<子目录名>)
# 作用: 告诉 CMake 进入指定子目录，执行其中的 CMakeLists.txt
# 执行顺序: 按照书写顺序依次执行
# 注意: 子目录中必须存在 CMakeLists.txt 文件
# ============================================================================

# 内部库模块 (libs/) - 包含 common, tcp, udp
add_subdirectory(libs)

# 主程序/示例程序 (src/)
add_subdirectory(src)

# ============================================================================
# 输出构建信息
# message(STATUS "...") 在配置时输出信息
# ============================================================================
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")

