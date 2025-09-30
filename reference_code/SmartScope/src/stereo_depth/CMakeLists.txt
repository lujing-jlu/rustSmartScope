cmake_minimum_required(VERSION 3.10)

# stereo_depth 模块
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc calib3d)

# 引入 depth_anything_inference 作为子目录
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/depth_anything_inference ${CMAKE_CURRENT_BINARY_DIR}/depth_anything_inference_build)

# 创建 stereo_depth 静态库
add_library(stereo_depth
    src/stereo_depth_pipeline.cpp
    src/comprehensive_depth_processor.cpp
    src/enhanced_postprocessing.cpp
    src/improved_depth_calibration.cpp
)

# 设置包含目录
target_include_directories(stereo_depth PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/depth_anything_inference/include
    ${OpenCV_INCLUDE_DIRS}
)

# 链接依赖库
target_link_libraries(stereo_depth PUBLIC 
    ${OpenCV_LIBS}
    depth_anything_inference
)

# 设置输出目录
set_target_properties(stereo_depth PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 创建示例程序（可选）
if(BUILD_EXAMPLES)
    add_executable(comprehensive_depth_example
        examples/use_comprehensive_lib.cpp
    )
    target_link_libraries(comprehensive_depth_example PRIVATE stereo_depth)
    
    add_executable(fine_grained_example
        examples/fine_grained_example.cpp
    )
    target_link_libraries(fine_grained_example PRIVATE stereo_depth)
endif() 