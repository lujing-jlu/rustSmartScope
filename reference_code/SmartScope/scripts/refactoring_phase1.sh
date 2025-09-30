#!/bin/bash

echo "=== SmartScope 第一阶段重构：基础设施重构 ==="
echo "开始执行..."

# 1. 备份当前状态
echo "1. 创建重构备份..."
cp -r . ../SmartScope_backup_$(date +%Y%m%d_%H%M%S)

# 2. 清理重复的YOLOv8实现
echo "2. 清理重复的YOLOv8实现..."

# 检查yolov8_rknn_inference和src/app/yolov8的差异
echo "检查YOLOv8实现的差异..."
diff -r yolov8_rknn_inference/ src/app/yolov8/ > yolov8_diff.txt 2>&1

if [ -s yolov8_diff.txt ]; then
    echo "发现差异，请检查 yolov8_diff.txt"
    echo "建议：保留 src/app/yolov8/，删除 yolov8_rknn_inference/"
else
    echo "两个目录内容相同，删除重复的 yolov8_rknn_inference/"
    rm -rf yolov8_rknn_inference/
fi

# 删除空的yolov8目录
if [ -d yolov8/ ] && [ -z "$(ls -A yolov8/)" ]; then
    echo "删除空的 yolov8/ 目录"
    rm -rf yolov8/
fi

# 3. 重命名中文目录
echo "3. 重命名中文目录..."
if [ -d "软件文档" ]; then
    echo "重命名 软件文档/ -> docs/zh/"
    mkdir -p docs/zh
    mv "软件文档"/* docs/zh/ 2>/dev/null || true
    rmdir "软件文档" 2>/dev/null || true
fi

# 4. 整理模型文件
echo "4. 整理模型文件..."
# 确保models目录存在
mkdir -p models

# 移动重复的模型文件到主models目录
find . -name "*.rknn" -not -path "./models/*" -not -path "./build/*" -not -path "./deploy/*" | while read file; do
    filename=$(basename "$file")
    if [ ! -f "models/$filename" ]; then
        echo "移动模型文件: $file -> models/$filename"
        cp "$file" "models/$filename"
    fi
done

# 5. 整理库文件
echo "5. 整理库文件..."
# 确保lib目录存在
mkdir -p lib

# 移动重复的库文件到主lib目录
find . -name "*.so" -not -path "./lib/*" -not -path "./build/*" -not -path "./deploy/*" | while read file; do
    filename=$(basename "$file")
    if [ ! -f "lib/$filename" ]; then
        echo "移动库文件: $file -> lib/$filename"
        cp "$file" "lib/$filename"
    fi
done

# 6. 创建新的目录结构
echo "6. 创建新的目录结构..."
mkdir -p scripts
mkdir -p docs/en
mkdir -p docs/zh

# 移动脚本文件到scripts目录
find . -name "*.sh" -not -path "./scripts/*" -not -path "./build/*" | while read file; do
    if [ "$(dirname "$file")" != "." ]; then
        echo "移动脚本文件: $file -> scripts/"
        mv "$file" "scripts/"
    fi
done

# 7. 整理文档
echo "7. 整理文档..."
# 移动英文文档到docs/en
find . -name "*.md" -not -path "./docs/*" -not -path "./build/*" | while read file; do
    if [ "$(dirname "$file")" != "." ]; then
        echo "移动文档: $file -> docs/en/"
        mv "$file" "docs/en/"
    fi
done

echo "=== 第一阶段重构完成 ==="
echo "请检查重构结果，然后运行测试确保功能正常" 