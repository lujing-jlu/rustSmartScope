# 创建示例可执行文件
add_executable(depth_inference_example
    depth_inference_example.cpp
)

# 链接库
target_link_libraries(depth_inference_example
    depth_anything_inference
    ${OpenCV_LIBS}
    glog::glog
)
