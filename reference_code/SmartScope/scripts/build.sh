#!/bin/bash

# Depth Anything Inference 构建脚本

set -e

echo "=== Depth Anything Inference 构建脚本 ==="

# 检查依赖
echo "检查依赖..."

# 检查CMake
if ! command -v cmake &> /dev/null; then
    echo "错误: 未找到CMake，请先安装CMake"
    exit 1
fi

# 检查make
if ! command -v make &> /dev/null; then
    echo "错误: 未找到make，请先安装make"
    exit 1
fi

# 检查OpenCV
if ! pkg-config --exists opencv4; then
    echo "警告: 未找到OpenCV4，尝试检查OpenCV..."
    if ! pkg-config --exists opencv; then
        echo "错误: 未找到OpenCV，请先安装OpenCV"
        echo "Ubuntu/Debian: sudo apt-get install libopencv-dev"
        exit 1
    fi
fi

# 检查Eigen3
if ! pkg-config --exists eigen3; then
    echo "错误: 未找到Eigen3，请先安装Eigen3"
    echo "Ubuntu/Debian: sudo apt-get install libeigen3-dev"
    exit 1
fi

# 检查glog
if ! pkg-config --exists libglog; then
    echo "错误: 未找到glog，请先安装glog"
    echo "Ubuntu/Debian: sudo apt-get install libgoogle-glog-dev"
    exit 1
fi

echo "依赖检查完成"

# 创建构建目录
echo "创建构建目录..."
mkdir -p build
cd build

# 配置项目
echo "配置项目..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译项目
echo "编译项目..."
make -j$(nproc)

echo "构建完成！"
echo ""
echo "可执行文件位置:"
echo "  - 示例程序: build/bin/depth_inference_example"
echo "  - 测试程序: build/bin/test_basic"
echo ""
echo "运行示例:"
echo "  ./build/bin/depth_inference_example test_data/input.jpg"
echo ""
echo "运行测试:"
echo "  ./build/bin/test_basic" 