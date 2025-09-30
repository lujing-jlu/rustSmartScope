#!/bin/bash

echo "=== SmartScope 项目深层清理脚本 ==="
echo "开始清理..."

# 1. 清理构建产物
echo "1. 清理构建产物..."
rm -rf build/
rm -rf src/*/CMakeFiles/
rm -rf src/*/cmake_install.cmake
rm -rf src/*/Makefile
rm -rf src/*/*_autogen/
rm -rf SmartScopeQt_autogen/
rm -rf test_depth_inference_autogen/

# 2. 清理临时和日志文件
echo "2. 清理临时和日志文件..."
rm -f *.log
rm -f *.txt
rm -f core
rm -f core.*
rm -f test_depth_inference

# 3. 清理测试图片（保留resources中的）
echo "3. 清理测试图片..."
rm -f *.png
rm -f *.jpg
rm -f *.jpeg
rm -f bus.jpg

# 4. 清理重复的库文件（保留lib/目录下的）
echo "4. 清理重复的库文件..."
rm -f librga.so
rm -f librknn_api.so
rm -f librknnrt.so

# 5. 清理yolov8_rknn_inference构建产物
echo "5. 清理yolov8_rknn_inference构建产物..."
rm -rf yolov8_rknn_inference/build/
rm -f yolov8_rknn_inference/output.jpg

# 6. 清理src/app/yolov8构建产物
echo "6. 清理src/app/yolov8构建产物..."
rm -rf src/app/yolov8/build/

# 7. 清理stereo_depth构建产物
echo "7. 清理stereo_depth构建产物..."
rm -rf src/stereo_depth/depth_anything_inference/build/

# 8. 清理测试文件（保留src/tests/）
echo "8. 清理测试文件..."
rm -f test_*.py
rm -f test_*.cpp
rm -f camera_test.*
rm -f Makefile_camera_test
rm -f run_camera_test.sh

# 9. 清理重复的模型文件（保留models/目录下的）
echo "9. 清理重复的模型文件..."
rm -f src/app/yolov8/models/*.rknn
rm -f yolov8_rknn_inference/model/*.rknn
rm -f src/stereo_depth/depth_anything_inference/models/*.rknn

# 10. 清理重复的标签文件
echo "10. 清理重复的标签文件..."
rm -f yolov8_rknn_inference/model/*.txt
rm -f src/app/yolov8/models/*.txt

echo "=== 清理完成 ==="
echo "建议执行以下命令查看清理效果："
echo "du -sh * | sort -hr" 