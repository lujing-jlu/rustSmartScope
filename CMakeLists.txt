cmake_minimum_required(VERSION 3.16)

project(RustSmartScope VERSION 0.1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译优化设置
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 启用并行编译
include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${N})
    message(STATUS "Using ${N} parallel compilation jobs")
endif()

# 启用自动MOC, UIC, RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 启用增量编译和缓存
# set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
if(CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

# 启用预编译头
set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Quick Qml Svg)
message(STATUS "Using Qt5.${Qt5Core_VERSION}")

# 查找线程库
find_package(Threads REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# 设置Rust库路径
set(RUST_TARGET_DIR "${CMAKE_SOURCE_DIR}/target/release")
set(RUST_SMARTSCOPE_LIB "${RUST_TARGET_DIR}/libsmartscope.a")

# 检查Rust库是否存在
if(NOT EXISTS ${RUST_SMARTSCOPE_LIB})
    message(WARNING "Rust library not found at ${RUST_SMARTSCOPE_LIB}")
    message(WARNING "Please run: cargo build --release -p smartscope-c-ffi")
endif()

# 添加源文件
set(SOURCES
    src/main.cpp
)

# QML资源文件（如果存在）
if(EXISTS "${CMAKE_SOURCE_DIR}/qml/qml.qrc")
    qt5_add_resources(QML_SOURCES qml/qml.qrc)
endif()

# 字体资源文件（分离编译）
if(EXISTS "${CMAKE_SOURCE_DIR}/qml/fonts.qrc")
    qt5_add_resources(FONT_SOURCES qml/fonts.qrc)
endif()

# 图标资源已包含在qml.qrc中，无需单独处理

# 创建可执行文件
add_executable(rustsmartscope ${SOURCES} ${QML_SOURCES} ${FONT_SOURCES})

# 链接库
target_link_libraries(rustsmartscope
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Quick
    Qt5::Qml
    Qt5::Svg
    Threads::Threads
    ${RUST_SMARTSCOPE_LIB}
    ${CMAKE_DL_LIBS}
    -lpthread
)

# 输出构建信息
message(STATUS "Rust library: ${RUST_SMARTSCOPE_LIB}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 安装规则
install(TARGETS rustsmartscope
    RUNTIME DESTINATION bin
)