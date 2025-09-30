cmake_minimum_required(VERSION 3.16)

project(RustSmartScope VERSION 0.1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用自动MOC, UIC, RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Quick Qml)
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

# 图标资源文件
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
    qt5_add_resources(ICON_SOURCES resources/resources.qrc)
endif()

# 创建可执行文件
add_executable(rustsmartscope ${SOURCES} ${QML_SOURCES} ${ICON_SOURCES})

# 链接库
target_link_libraries(rustsmartscope
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Quick
    Qt5::Qml
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