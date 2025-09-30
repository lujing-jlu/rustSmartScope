cmake_minimum_required(VERSION 3.10)
project(yolov8_rknn_inference VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加系统库选项
option(USE_SYSTEM_LIBS "Use system installed RKNN libraries" OFF)

# 设置库路径
if(USE_SYSTEM_LIBS)
    # 使用系统库
    find_library(RKNN_RT_LIB rknn_runtime PATHS /usr/lib /usr/local/lib)
    find_library(RKNN_API_LIB rknn_api PATHS /usr/lib /usr/local/lib)
    find_library(RGA_LIB rga PATHS /usr/lib /usr/local/lib)
    
    # 检查是否找到系统库
    if(NOT RKNN_RT_LIB OR NOT RKNN_API_LIB OR NOT RGA_LIB)
        message(FATAL_ERROR "System libraries not found. Please install RKNN SDK or set USE_SYSTEM_LIBS=OFF")
    endif()
else()
    # 使用本地库文件
    set(RKNN_RT_LIB ${CMAKE_SOURCE_DIR}/lib/librknnrt.so)
    set(RKNN_API_LIB ${CMAKE_SOURCE_DIR}/lib/librknn_api.so)
    set(RGA_LIB ${CMAKE_SOURCE_DIR}/lib/librga.so)
endif()

# 输出库路径信息
message(STATUS "RKNN Runtime library: ${RKNN_RT_LIB}")
message(STATUS "RKNN API library: ${RKNN_API_LIB}")
message(STATUS "RGA library: ${RGA_LIB}")

# 寻找OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# 添加包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(../3rdparty/rknpu2/include)
include_directories(../utils)
include_directories(${CMAKE_SOURCE_DIR}/include/rga)

# 添加源文件
set(SOURCES
    src/yolov8_inference.cc
    src/yolov8.cc
    src/postprocess.cc
    src/file_utils.c
    src/image_utils.c
    src/image_drawing.c
)

# 创建共享库
add_library(yolov8_rknn SHARED ${SOURCES})

# 链接库
target_link_libraries(yolov8_rknn
    ${RKNN_RT_LIB}
    ${RKNN_API_LIB}
    ${RGA_LIB}
    ${OpenCV_LIBS}
    dl
    pthread
    m
)

# 测试程序
add_executable(test_inference test/test_inference.cc)
target_link_libraries(test_inference
    yolov8_rknn
    ${OpenCV_LIBS}
)

# 安装规则
install(TARGETS yolov8_rknn
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(TARGETS test_inference
    RUNTIME DESTINATION bin
)

install(FILES
    include/yolov8_inference.h
    DESTINATION include/yolov8_rknn
)

# 安装并重命名依赖库
if(NOT USE_SYSTEM_LIBS)
    install(FILES ${RKNN_RT_LIB}
        DESTINATION lib
        RENAME librknn_runtime.so
    )
    
    install(FILES ${RKNN_API_LIB}
        DESTINATION lib
    )
    
    install(FILES ${RGA_LIB}
        DESTINATION lib
    )
endif()

# 复制模型和标签文件
install(DIRECTORY model/
    DESTINATION share/yolov8_rknn/model
    FILES_MATCHING PATTERN "*.rknn" PATTERN "*.txt" PATTERN "*.jpg"
) 