#!/bin/bash

# 定义颜色
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}正在重启SmartScope应用...${NC}"

# 获取当前目录
CURRENT_DIR=$(pwd)
echo -e "${GREEN}当前目录: ${CURRENT_DIR}${NC}"

# 停止当前运行的SmartScope进程
echo -e "${YELLOW}停止当前运行的SmartScope进程...${NC}"
pkill -f SmartScope || true
sleep 2

# 确保进程已经停止
if pgrep -f SmartScope > /dev/null; then
    echo -e "${RED}强制终止SmartScope进程...${NC}"
    pkill -9 -f SmartScope || true
    sleep 1
fi

# 启动SmartScope应用
echo -e "${YELLOW}启动SmartScope应用...${NC}"
./scripts/root_scripts/run_smartscope.sh

echo -e "${GREEN}重启完成!${NC}" 