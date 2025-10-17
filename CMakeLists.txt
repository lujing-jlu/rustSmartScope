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

# 查找Qt5（Svg 设为可选）
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Quick Qml)
message(STATUS "Using Qt5.${Qt5Core_VERSION}")
# Svg 可选（用于加载SVG图标）；若缺失则继续构建但可能无法渲染SVG
find_package(Qt5Svg QUIET)

# 查找OpenCV
find_package(OpenCV REQUIRED COMPONENTS core imgproc calib3d highgui imgcodecs)
message(STATUS "Using OpenCV ${OpenCV_VERSION}")

# 查找线程库
find_package(Threads REQUIRED)
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(UDEV IMPORTED_TARGET libudev)
    if(TARGET PkgConfig::UDEV)
        message(STATUS "Found libudev via pkg-config")
    else()
        message(WARNING "pkg-config found, but libudev not detected; will try linking with -ludev")
    endif()
else()
    message(WARNING "pkg-config not found; will try linking with -ludev")
endif()

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# 设置Rust库路径
set(RUST_TARGET_DIR "${CMAKE_SOURCE_DIR}/target/release")
set(RUST_SMARTSCOPE_LIB "${RUST_TARGET_DIR}/libsmartscope.a")

# RKNN runtime library directory (from rknn-inference crate)
set(RKNN_LIB_DIR "${CMAKE_SOURCE_DIR}/crates/rknn-inference/lib")

# 检查Rust库是否存在
if(NOT EXISTS ${RUST_SMARTSCOPE_LIB})
    message(WARNING "Rust library not found at ${RUST_SMARTSCOPE_LIB}")
    message(WARNING "Please run: cargo build --release -p smartscope-c-ffi")
endif()

# Ensure linker can find rknnrt and rga
if(EXISTS "${RKNN_LIB_DIR}")
    link_directories(${RKNN_LIB_DIR})
    message(STATUS "RKNN lib dir: ${RKNN_LIB_DIR}")
else()
    message(WARNING "RKNN lib dir not found: ${RKNN_LIB_DIR}")
endif()

# 添加源文件
set(SOURCES
    src/main.cpp
    src/qml_logger.cpp
    src/camera_manager.cpp
    src/camera_parameter_manager.cpp
    src/video_widget.cpp
    src/qml_video_item.cpp
    src/video_transform_manager.cpp
    src/ai_detection_manager.cpp
    src/storage_manager.cpp
    src/screen_recorder_manager.cpp
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
    $<$<TARGET_EXISTS:Qt5::Svg>:Qt5::Svg>
    Threads::Threads
    ${RUST_SMARTSCOPE_LIB}
    ${OpenCV_LIBS}
    ${CMAKE_DL_LIBS}
    -lpthread
    -lturbojpeg
    -lrga
    -lrknnrt

    # libudev (required by Rust udev crate)
    $<$<TARGET_EXISTS:PkgConfig::UDEV>:PkgConfig::UDEV>
    $<$<NOT:$<TARGET_EXISTS:PkgConfig::UDEV>>:-ludev>
)

# 输出构建信息
message(STATUS "Rust library: ${RUST_SMARTSCOPE_LIB}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Linking with RKNN libs from: ${RKNN_LIB_DIR}")

# 安装规则
install(TARGETS rustsmartscope
    RUNTIME DESTINATION bin
)
