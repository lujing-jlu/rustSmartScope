#!/bin/bash

echo "🚀 SmartScope 优化编译脚本"
echo "================================"

# 设置编译参数
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
export MAKEFLAGS="-j$(nproc)"

echo "📊 使用 $CMAKE_BUILD_PARALLEL_LEVEL 个并行编译进程"

# 清理构建目录（可选）
if [[ "$1" == "clean" ]]; then
    echo "🧹 清理构建目录..."
    rm -rf build
fi

# 创建构建目录
mkdir -p build
cd build

echo "⚙️ 配置CMake..."
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..

echo "🔨 开始编译..."
time make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "✅ 编译成功完成！"
    echo "🎯 可执行文件位置: $(pwd)/rustsmartscope"
else
    echo "❌ 编译失败"
    exit 1
fi