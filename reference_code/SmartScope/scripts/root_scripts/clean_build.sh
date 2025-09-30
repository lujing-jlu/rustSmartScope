#!/bin/bash

# 清理 CMake 和 Make 生成的构建文件脚本

# 颜色定义 (可选，用于美化输出)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# 获取脚本所在目录，确保在项目根目录执行清理
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR" || exit 1

echo -e "${YELLOW}开始清理构建文件...${NC}"
echo "当前目录: $(pwd)"

# 1. 删除根目录下的 CMake 缓存和主要构建文件
echo "删除根目录 CMake 缓存和主要构建文件..."
rm -f CMakeCache.txt
rm -f cmake_install.cmake
rm -f Makefile
rm -f CMakeRuleHashes.txt  # 明确删除，以防万一
rm -f TargetDirectories.txt # 明确删除，以防万一

# 2. 递归删除所有 CMakeFiles 目录
echo "递归删除所有 CMakeFiles/ 目录..."
find . -type d -name "CMakeFiles" -exec rm -rf {} +

# 3. 递归删除所有 *_autogen 目录 (Qt MOC/UIC/RCC 生成)
echo "递归删除所有 *_autogen/ 目录..."
find . -type d -name "*_autogen" -exec rm -rf {} +

# 4. 删除最终生成的可执行文件 (根据项目配置可能需要调整名称)
echo "删除可执行文件 SmartScopeQt..."
rm -f SmartScopeQt

# 5. (可选) 删除特定类型的编译产物，find 会比较慢，但更彻底
# echo "删除对象文件 (*.o)..."
# find . -type f -name "*.o" -delete
# echo "删除静态库文件 (*.a)..."
# find . -type f -name "*.a" -delete # 注意：如果你想保留自己编译的静态库，注释掉这行

echo -e "${GREEN}清理完成！${NC}"

exit 0