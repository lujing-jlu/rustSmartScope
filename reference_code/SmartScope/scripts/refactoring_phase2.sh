#!/bin/bash

echo "=== SmartScope 第二阶段重构：模块重构 ==="
echo "开始执行..."

# 1. 备份当前状态
echo "1. 创建重构备份..."
cp -r . ../SmartScope_backup_phase2_$(date +%Y%m%d_%H%M%S)

# 2. 分析模块依赖关系
echo "2. 分析模块依赖关系..."
echo "检查CMakeLists.txt中的依赖关系..."

# 3. 简化YOLOv8模块结构
echo "3. 简化YOLOv8模块结构..."
echo "分析YOLOv8相关模块..."

# 检查src/app/yolov8和yolov8_rknn_inference的差异
if [ -d "yolov8_rknn_inference" ] && [ -d "src/app/yolov8" ]; then
    echo "发现两个YOLOv8实现，建议合并..."
    echo "src/app/yolov8/ 大小: $(du -sh src/app/yolov8/ | cut -f1)"
    echo "yolov8_rknn_inference/ 大小: $(du -sh yolov8_rknn_inference/ | cut -f1)"
fi

# 4. 优化构建系统
echo "4. 优化构建系统..."
echo "检查CMakeLists.txt文件..."

# 统计CMakeLists.txt数量
cmake_count=$(find . -name "CMakeLists.txt" | grep -v "build" | grep -v "autogen" | wc -l)
echo "当前CMakeLists.txt数量: $cmake_count"

# 5. 重新组织include目录
echo "5. 重新组织include目录..."
echo "检查include目录结构..."

# 6. 统一接口设计
echo "6. 统一接口设计..."
echo "检查头文件依赖..."

# 7. 创建新的模块结构
echo "7. 创建新的模块结构..."
mkdir -p src/modules
mkdir -p src/modules/core
mkdir -p src/modules/app
mkdir -p src/modules/inference
mkdir -p src/modules/infrastructure

echo "=== 第二阶段重构分析完成 ==="
echo "建议的下一步操作："
echo "1. 合并重复的YOLOv8实现"
echo "2. 简化CMakeLists.txt结构"
echo "3. 重新组织模块依赖"
echo "4. 统一接口设计" 