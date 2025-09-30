cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(YOLOv8RknnIntegration)

# 查找OpenCV包
find_package(OpenCV REQUIRED)

# 设置库路径
set(RKNN_API_LIB ${CMAKE_SOURCE_DIR}/lib/librknn_api.so)
set(RKNN_RT_LIB ${CMAKE_SOURCE_DIR}/lib/librknnrt.so)
set(RGA_LIB ${CMAKE_SOURCE_DIR}/lib/librga.so)

# 输出库路径信息
message(STATUS "RKNN API library: ${RKNN_API_LIB}")
message(STATUS "RKNN Runtime library: ${RKNN_RT_LIB}")
message(STATUS "RGA library: ${RGA_LIB}")

# YOLOv8 RKNN推理模块目录
set(YOLOV8_RKNN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rknn_inference)

# 包含头文件目录
include_directories(
    ${YOLOV8_RKNN_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    # 添加RGA头文件路径
    /usr/include/rga
)

# 添加库路径
link_directories(
    ${CMAKE_SOURCE_DIR}/lib
)

# 源文件
set(YOLOV8_RKNN_SOURCES
    ${YOLOV8_RKNN_DIR}/src/yolov8.cc
    ${YOLOV8_RKNN_DIR}/src/yolov8_inference.cc
    ${YOLOV8_RKNN_DIR}/src/postprocess.cc
    ${YOLOV8_RKNN_DIR}/src/image_drawing.c
    ${YOLOV8_RKNN_DIR}/src/image_utils.c
    ${YOLOV8_RKNN_DIR}/src/file_utils.c
)

# YOLOv8检测器接口源文件
set(YOLOV8_DETECTOR_SOURCES
    yolov8_detector.cpp
)

# 避免目标重复定义
if(NOT TARGET yolov8_rknn)
    # 创建静态库
    add_library(yolov8_rknn STATIC ${YOLOV8_RKNN_SOURCES})

    # 链接依赖库
    target_link_libraries(yolov8_rknn
        ${OpenCV_LIBS}
        # 使用指定路径的库文件
        ${RKNN_API_LIB}
        ${RKNN_RT_LIB}
        ${RGA_LIB}
        dl
        pthread
        m
    )
endif()

if(NOT TARGET yolov8_detector)
    # 创建检测器接口库
    add_library(yolov8_detector STATIC ${YOLOV8_DETECTOR_SOURCES})

    # 链接检测器接口库
    target_link_libraries(yolov8_detector
        yolov8_rknn
        ${OpenCV_LIBS}
    )
endif()

# 只在单独构建时创建测试程序
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # 添加测试程序
    add_executable(yolov8_test yolov8_test.cpp)

    # 链接到我们刚创建的静态库
    target_link_libraries(yolov8_test
        yolov8_detector
        ${OpenCV_LIBS}
    )
    
    # 安装目标
    install(TARGETS yolov8_rknn yolov8_detector DESTINATION lib)
    install(TARGETS yolov8_test DESTINATION bin)
    install(DIRECTORY ${YOLOV8_RKNN_DIR}/include/ DESTINATION include/yolov8_rknn)
    install(FILES ${CMAKE_SOURCE_DIR}/include/yolov8_detector.h DESTINATION include)
endif() 