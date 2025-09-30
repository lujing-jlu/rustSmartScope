#!/bin/bash

echo "=== SmartScope 第一阶段重构：基础设施重构（安全版本） ==="
echo "开始执行..."

# 1. 备份当前状态
echo "1. 创建重构备份..."
cp -r . ../SmartScope_backup_$(date +%Y%m%d_%H%M%S)

# 2. 重命名中文目录
echo "2. 重命名中文目录..."
if [ -d "软件文档" ]; then
    echo "重命名 软件文档/ -> docs/zh/"
    mkdir -p docs/zh
    mv "软件文档"/* docs/zh/ 2>/dev/null || true
    rmdir "软件文档" 2>/dev/null || true
fi

# 3. 创建新的目录结构
echo "3. 创建新的目录结构..."
mkdir -p scripts
mkdir -p docs/en
mkdir -p docs/zh

# 4. 整理脚本文件
echo "4. 整理脚本文件..."
# 移动脚本文件到scripts目录（保留根目录的重要脚本）
find . -name "*.sh" -not -path "./scripts/*" -not -path "./build/*" | while read file; do
    dirname=$(dirname "$file")
    if [ "$dirname" != "." ]; then
        echo "移动脚本文件: $file -> scripts/"
        mv "$file" "scripts/"
    fi
done

# 5. 整理文档文件
echo "5. 整理文档文件..."
# 移动英文文档到docs/en（保留根目录的重要文档）
find . -name "*.md" -not -path "./docs/*" -not -path "./build/*" | while read file; do
    dirname=$(dirname "$file")
    if [ "$dirname" != "." ]; then
        echo "移动文档: $file -> docs/en/"
        mv "$file" "docs/en/"
    fi
done

# 6. 整理模型文件（只复制，不删除原文件）
echo "6. 整理模型文件..."
# 确保models目录存在
mkdir -p models

# 复制重复的模型文件到主models目录（如果不存在）
find . -name "*.rknn" -not -path "./models/*" -not -path "./build/*" -not -path "./deploy/*" | while read file; do
    filename=$(basename "$file")
    if [ ! -f "models/$filename" ]; then
        echo "复制模型文件: $file -> models/$filename"
        cp "$file" "models/$filename"
    fi
done

# 7. 整理库文件（只复制，不删除原文件）
echo "7. 整理库文件..."
# 确保lib目录存在
mkdir -p lib

# 复制重复的库文件到主lib目录（如果不存在）
find . -name "*.so" -not -path "./lib/*" -not -path "./build/*" -not -path "./deploy/*" | while read file; do
    filename=$(basename "$file")
    if [ ! -f "lib/$filename" ]; then
        echo "复制库文件: $file -> lib/$filename"
        cp "$file" "lib/$filename"
    fi
done

# 8. 删除空的yolov8目录
echo "8. 清理空目录..."
if [ -d yolov8/ ] && [ -z "$(ls -A yolov8/)" ]; then
    echo "删除空的 yolov8/ 目录"
    rm -rf yolov8/
fi

echo "=== 第一阶段重构完成（安全版本） ==="
echo "重构结果："
echo "- 保留了所有YOLOv8实现（避免破坏现有功能）"
echo "- 整理了目录结构"
echo "- 统一了文档和脚本组织"
echo "- 复制了重复的模型和库文件到统一位置"
echo ""
echo "下一步建议："
echo "1. 测试项目功能是否正常"
echo "2. 检查CMakeLists.txt中的路径引用"
echo "3. 准备第二阶段重构：模块重构" 