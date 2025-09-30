#!/bin/bash

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# 确保在smart_scope_qt目录下
cd /root/projects/qtscope_project/smart_scope_qt

# 创建测试构建目录
mkdir -p build_tests
cd build_tests

# 配置CMake
echo -e "${GREEN}配置CMake...${NC}"
cmake -DCMAKE_BUILD_TYPE=Debug ..

if [ $? -eq 0 ]; then
    # 编译测试程序
    echo -e "${GREEN}编译测试程序...${NC}"
    make test_camera_sync
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}编译成功！运行测试程序...${NC}"
        # 运行测试程序
        ./test_camera_sync --left-camera /dev/video1 --right-camera /dev/video3 --duration 10
    else
        echo -e "${RED}编译失败！${NC}"
        exit 1
    fi
else
    echo -e "${RED}CMake配置失败！${NC}"
    exit 1
fi 