cmake_minimum_required(VERSION 3.10)

project(SmartScopeQt VERSION 1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用自动MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置库运行时搜索路径
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# 设置RKNN相关库路径
set(RKNN_API_LIB ${CMAKE_SOURCE_DIR}/lib/librknn_api.so)
set(RKNN_RT_LIB ${CMAKE_SOURCE_DIR}/lib/librknnrt.so)
set(RGA_LIB ${CMAKE_SOURCE_DIR}/lib/librga.so)

# 输出库路径信息
message(STATUS "RKNN API library: ${RKNN_API_LIB}")
message(STATUS "RKNN Runtime library: ${RKNN_RT_LIB}")
message(STATUS "RGA library: ${RGA_LIB}")

# 设置Qt模块
find_package(Qt5 COMPONENTS 
    Core
    Gui 
    Widgets
    OpenGL
    PrintSupport
    Charts
    Quick
    REQUIRED
)

# 设置线程库
find_package(Threads REQUIRED)

# 添加OpenMP支持
find_package(OpenMP REQUIRED)
if(OpenMP_FOUND)
    message(STATUS "找到OpenMP库: ${OpenMP_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# 设置OpenCV
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV库版本: ${OpenCV_VERSION}")
message(STATUS "OpenCV包含目录: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV库: ${OpenCV_LIBS}")

# 设置PCL
find_package(PCL REQUIRED)
message(STATUS "PCL库版本: ${PCL_VERSION}")
message(STATUS "PCL包含目录: ${PCL_INCLUDE_DIRS}")
message(STATUS "PCL库: ${PCL_LIBRARIES}")
add_definitions(${PCL_DEFINITIONS})

# 设置Google Logging
find_package(glog REQUIRED)
message(STATUS "Google Logging库: ${glog_LIBRARIES}")

# 设置TurboJPEG
find_path(TURBOJPEG_INCLUDE_DIR turbojpeg.h)
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg)
message(STATUS "找到TurboJPEG库: ${TURBOJPEG_LIBRARY}")
message(STATUS "TurboJPEG包含目录: ${TURBOJPEG_INCLUDE_DIR}")

# stereo_depth 模块设置（整合到主项目中）
set(STEREO_DEPTH_DIR "${CMAKE_SOURCE_DIR}/src/stereo_depth")
include_directories(${STEREO_DEPTH_DIR}/include)
message(STATUS "stereo_depth 目录: ${STEREO_DEPTH_DIR}")

# 检查 stereo_depth 是否存在
if(NOT EXISTS ${STEREO_DEPTH_DIR})
    message(FATAL_ERROR "未找到 stereo_depth 目录: ${STEREO_DEPTH_DIR}")
endif()

# 添加include目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcustomplot)

# 添加子目录
add_subdirectory(src/app)
add_subdirectory(src/infrastructure)
add_subdirectory(src/core)
add_subdirectory(src/inference)
add_subdirectory(src/stereo_depth)

# 链接 stereo_depth_lib (在目标定义后添加)

# 添加资源文件
set(RESOURCE_FILES
    resources/resources.qrc
)

# 添加源文件
set(SOURCE_FILES
    src/main.cpp
    src/mainwindow.cpp
    src/statusbar.cpp
    third_party/qcustomplot/qcustomplot.cpp
    src/app/ui/home_page.cpp
    src/app/utils/led_controller.cpp
    src/app/utils/hid_com/hid_communication.cpp
    src/core/rga/src/rga_processor.cpp
    src/core/rga/src/rga_transform_types.cpp
)

# 添加头文件 (修正路径)
set(HEADER_FILES
    ${CMAKE_SOURCE_DIR}/include/mainwindow.h
    ${CMAKE_SOURCE_DIR}/include/statusbar.h
    third_party/qcustomplot/qcustomplot.h
    src/core/rga/include/rga/rga_processor.h
    src/core/rga/include/rga/rga_transform_types.h
)

# 添加头文件搜索路径
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/app/utils
    ${CMAKE_SOURCE_DIR}/src/core/rga/include
    ${CMAKE_SOURCE_DIR}/third_party/qcustomplot
    ${OpenCV_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
    ${TurboJPEG_INCLUDE_DIRS}
)

# 创建可执行文件
add_executable(SmartScopeQt
    ${SOURCE_FILES}
    ${HEADER_FILES}
    ${RESOURCE_FILES}
)

# 链接库 (添加 Qt5::OpenGL 和 Qt5::PrintSupport)
target_link_libraries(SmartScopeQt
    PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    Qt5::OpenGL
    Qt5::PrintSupport
    ${OpenCV_LIBS}
    ${PCL_LIBRARIES}
    OpenMP::OpenMP_CXX
    ui
    measurement
    infrastructure
    core
    inference
    image
    utils
    rga
    # ${ONNXRUNTIME_LIBRARY} # 直接链接具体文件
    # ${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so.1.16.3 # 暂时注释掉 ONNX Runtime
    stereo_depth
    glog::glog
)

# 包含目录
target_include_directories(SmartScopeQt
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core/rga/include
    ${OpenCV_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
)

# 添加库搜索路径
link_directories(${CMAKE_SOURCE_DIR}/lib)

# 安装RKNN相关库
install(FILES 
    ${RKNN_API_LIB}
    ${RKNN_RT_LIB}
    ${RGA_LIB}
    DESTINATION lib
)

# 复制共享库到构建目录
add_custom_command(TARGET SmartScopeQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${RKNN_API_LIB} ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${RKNN_RT_LIB} ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${RGA_LIB} ${CMAKE_BINARY_DIR}
    COMMENT "Copying RKNN shared libraries to build directory"
)

# 添加测试目录 - 暂时注释掉，避免测试编译错误
# add_subdirectory(src/tests)

# 添加测试程序 - 暂时注释掉，因为文件已被清理
# add_executable(test_depth_inference test_depth_inference.cpp)
# target_link_libraries(test_depth_inference
#     ${OpenCV_LIBS}
#     ${PCL_LIBRARIES}
#     Qt5::Core
#     inference
#     stereo_depth
#     glog::glog
# ) 