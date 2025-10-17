#!/bin/bash

echo "⚡ SmartScope 增量编译脚本"
echo "==============================="

# 设置编译参数
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
export MAKEFLAGS="-j$(nproc)"

echo "📊 使用 $CMAKE_BUILD_PARALLEL_LEVEL 个并行编译进程"

# 选择Qt路径（优先 /opt/qt5.15.15，其次系统Qt）
QT_DIR=""
QT_ROOT=""
if [ -d "/opt/qt5.15.15/lib/cmake/Qt5" ]; then
    QT_DIR="/opt/qt5.15.15/lib/cmake/Qt5"
    QT_ROOT="/opt/qt5.15.15"
    export CMAKE_PREFIX_PATH="${QT_ROOT}/lib/cmake:${CMAKE_PREFIX_PATH}"
    export LD_LIBRARY_PATH="${QT_ROOT}/lib:${LD_LIBRARY_PATH}"
    export QT_PLUGIN_PATH="${QT_ROOT}/plugins"
    echo "🔧 检测到自带Qt5: ${QT_ROOT}"
elif [ -d "/usr/lib/aarch64-linux-gnu/cmake/Qt5" ]; then
    QT_DIR="/usr/lib/aarch64-linux-gnu/cmake/Qt5"
    QT_ROOT="/usr"
    echo "🔧 使用系统Qt5: ${QT_DIR}"
else
    echo "⚠️ 未检测到Qt5配置目录，将依赖CMake自动查找"
fi

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
          ${QT_DIR:+-DQt5_DIR=${QT_DIR}} \
          ${QT_ROOT:+-DQT_ROOT=${QT_ROOT}} \
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
