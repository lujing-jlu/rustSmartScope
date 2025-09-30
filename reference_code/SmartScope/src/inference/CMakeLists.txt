set(INFERENCE_SOURCES
    inference_service.cpp
    stereo_depth_inference.cpp
    yolov8_service.cpp
)

set(INFERENCE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/inference/inference_service.hpp
    ${CMAKE_SOURCE_DIR}/include/inference/stereo_depth_inference.hpp
    ${CMAKE_SOURCE_DIR}/include/inference/yolov8_service.hpp
)

add_library(inference STATIC ${INFERENCE_SOURCES} ${INFERENCE_HEADERS})

# 添加Qt库依赖
find_package(Qt5 COMPONENTS Core REQUIRED)

target_link_libraries(inference
    PRIVATE
    ${OpenCV_LIBS}
    ${PCL_LIBRARIES}
    Qt5::Core
    stereo_depth
)

target_include_directories(inference
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
    ${STEREO_DEPTH_DIR}/include # stereo_depth包含目录
    ${Qt5Core_INCLUDE_DIRS}
)

# 添加YOLOv8检测器依赖
# 查找YOLOv8库的路径
find_path(YOLOV8_DETECTOR_PATH
    NAMES yolov8_detector.h
    PATHS ${CMAKE_SOURCE_DIR}/src/app/yolov8
    NO_DEFAULT_PATH
)

if(NOT YOLOV8_DETECTOR_PATH)
    message(FATAL_ERROR "未找到YOLOv8检测器头文件路径")
endif()

# 检查librknn库
find_library(RKNN_LIBRARY
    NAMES rknn_api
    PATHS /usr/lib /usr/local/lib ${CMAKE_SOURCE_DIR}/lib
)

if(NOT RKNN_LIBRARY)
    message(WARNING "未找到RKNN库，将使用YOLOv8检测器提供的静态库")
endif()

# 添加YOLOv8检测器的头文件路径
target_include_directories(inference
    PRIVATE
    ${YOLOV8_DETECTOR_PATH}
    ${YOLOV8_DETECTOR_PATH}/rknn_inference/include
)

# 在主项目中构建YOLOv8检测器库
add_subdirectory(${CMAKE_SOURCE_DIR}/src/app/yolov8 ${CMAKE_BINARY_DIR}/yolov8)

# 链接YOLOv8检测器库
target_link_libraries(inference
    PRIVATE
    yolov8_detector
    yolov8_rknn
)

# 确保模型文件被复制到输出目录
set(YOLOV8_MODEL_DIR ${CMAKE_SOURCE_DIR}/models)
file(GLOB YOLOV8_MODEL_FILES ${YOLOV8_MODEL_DIR}/*.rknn ${YOLOV8_MODEL_DIR}/*.txt)

foreach(MODEL_FILE ${YOLOV8_MODEL_FILES})
    get_filename_component(FILE_NAME ${MODEL_FILE} NAME)
    configure_file(${MODEL_FILE} ${CMAKE_BINARY_DIR}/models/${FILE_NAME} COPYONLY)
endforeach() 