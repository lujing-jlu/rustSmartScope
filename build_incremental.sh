#!/bin/bash

echo "⚡ SmartScope 增量编译脚本"
echo "==============================="

# 设置编译参数
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
export MAKEFLAGS="-j$(nproc)"

echo "📊 使用 $CMAKE_BUILD_PARALLEL_LEVEL 个并行编译进程"

# 只有明确指定clean才完全清理
if [[ "$1" == "clean" ]]; then
    echo "🧹 完全清理构建目录..."
    rm -rf build
fi

# 创建构建目录
mkdir -p build
cd build

# 检查是否需要重新配置CMake
if [[ ! -f Makefile ]] || [[ ../CMakeLists.txt -nt Makefile ]]; then
    echo "⚙️ 配置CMake..."
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG" \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
          ..
else
    echo "✅ CMake配置未变化，跳过配置步骤"
fi

echo "🔨 开始增量编译..."
time make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "✅ 编译成功完成！"
    echo "🎯 可执行文件位置: $(pwd)/bin/rustsmartscope"
    ls -lh bin/rustsmartscope 2>/dev/null || echo "⚠️ 可执行文件未找到"
else
    echo "❌ 编译失败"
    exit 1
fi