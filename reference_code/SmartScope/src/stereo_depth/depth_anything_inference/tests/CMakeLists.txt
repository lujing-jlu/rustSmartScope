# 创建测试可执行文件
add_executable(test_basic test_basic.cpp)

# 链接依赖库
target_link_libraries(test_basic
    depth_anything_inference
    gtest
    gtest_main
    ${OpenCV_LIBS}
)

# 设置包含目录
target_include_directories(test_basic PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# 添加到CTest
add_test(NAME test_basic COMMAND test_basic)
