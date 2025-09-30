# 添加子目录
add_subdirectory(rknn_core)
add_subdirectory(deploy_core)
add_subdirectory(image_processing_utils)
add_subdirectory(mono_stereo_depth_anything)

# 创建主库
add_library(depth_anything_inference
    depth_anything_inference_impl.cpp
    rknn_core/src/rknn_core.cpp
    rknn_core/src/rknn_core_factory.cpp
    deploy_core/src/base_infer_core.cpp
    deploy_core/src/base_mono_stereo.cpp
    deploy_core/src/base_detection.cpp
    deploy_core/src/base_stereo.cpp
    image_processing_utils/src/detection_2d_preprocess_cpu.cpp
    image_processing_utils/src/detection_2d_postprocess_cpu.cpp
    mono_stereo_depth_anything/src/depth_anything.cpp
)

# 链接依赖库
target_link_libraries(depth_anything_inference
    rknn_core
    deploy_core
    image_processing_utils
    mono_stereo_depth_anything
    ${OpenCV_LIBS}
    glog::glog
    ${EIGEN3_LIBRARIES}
)

# 设置包含目录
target_include_directories(depth_anything_inference PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
