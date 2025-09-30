#!/bin/bash

echo "=== SmartScope 根目录文件整理脚本 ==="
echo "开始整理根目录下的文件..."

# 1. 创建必要的目录
echo "1. 创建整理目录..."
mkdir -p scripts/root_scripts
mkdir -p scripts/tools
mkdir -p docs/analysis
mkdir -p docs/integration
mkdir -p docs/debug
mkdir -p docs/protocols
mkdir -p images/debug_output
mkdir -p images/test_results
mkdir -p temp_files

# 2. 整理脚本文件
echo "2. 整理脚本文件..."

# 移动构建和运行脚本到scripts/root_scripts
echo "移动构建和运行脚本..."
mv auto_build_run.sh scripts/root_scripts/ 2>/dev/null || echo "auto_build_run.sh 已存在或移动失败"
mv clean_build.sh scripts/root_scripts/ 2>/dev/null || echo "clean_build.sh 已存在或移动失败"
mv deploy_minimal_smartscope.sh scripts/root_scripts/ 2>/dev/null || echo "deploy_minimal_smartscope.sh 已存在或移动失败"
mv restart.sh scripts/root_scripts/ 2>/dev/null || echo "restart.sh 已存在或移动失败"
mv run_smartscope.sh scripts/root_scripts/ 2>/dev/null || echo "run_smartscope.sh 已存在或移动失败"

# 移动重构脚本到scripts/
echo "移动重构脚本..."
mv refactoring_phase1.sh scripts/ 2>/dev/null || echo "refactoring_phase1.sh 已存在或移动失败"
mv refactoring_phase1_safe.sh scripts/ 2>/dev/null || echo "refactoring_phase1_safe.sh 已存在或移动失败"
mv refactoring_phase2.sh scripts/ 2>/dev/null || echo "refactoring_phase2.sh 已存在或移动失败"
mv cleanup_script.sh scripts/ 2>/dev/null || echo "cleanup_script.sh 已存在或移动失败"

# 移动工具脚本到scripts/tools
echo "移动工具脚本..."
mv calib.py scripts/tools/ 2>/dev/null || echo "calib.py 已存在或移动失败"
mv camera_diagnosis.py scripts/tools/ 2>/dev/null || echo "camera_diagnosis.py 已存在或移动失败"
mv camera_test_fixed.py scripts/tools/ 2>/dev/null || echo "camera_test_fixed.py 已存在或移动失败"
mv depth_analysis.py scripts/tools/ 2>/dev/null || echo "depth_analysis.py 已存在或移动失败"
mv md_to_word.py scripts/tools/ 2>/dev/null || echo "md_to_word.py 已存在或移动失败"
mv send_hid_report.py scripts/tools/ 2>/dev/null || echo "send_hid_report.py 已存在或移动失败"

# 3. 整理文档文件
echo "3. 整理文档文件..."

# 移动分析文档到docs/analysis
echo "移动分析文档..."
mv CLEANUP_ANALYSIS.md docs/analysis/ 2>/dev/null || echo "CLEANUP_ANALYSIS.md 已存在或移动失败"
mv REFACTORING_ANALYSIS.md docs/analysis/ 2>/dev/null || echo "REFACTORING_ANALYSIS.md 已存在或移动失败"
mv REFACTORING_SUMMARY.md docs/analysis/ 2>/dev/null || echo "REFACTORING_SUMMARY.md 已存在或移动失败"

# 移动集成文档到docs/integration
echo "移动集成文档..."
mv DEPTH_ANYTHING_INTEGRATION_ANALYSIS.md docs/integration/ 2>/dev/null || echo "DEPTH_ANYTHING_INTEGRATION_ANALYSIS.md 已存在或移动失败"
mv DEPTH_ANYTHING_INTEGRATION_COMPLETE.md docs/integration/ 2>/dev/null || echo "DEPTH_ANYTHING_INTEGRATION_COMPLETE.md 已存在或移动失败"
mv DEPTH_MODE_SWITCH_ENHANCEMENT.md docs/integration/ 2>/dev/null || echo "DEPTH_MODE_SWITCH_ENHANCEMENT.md 已存在或移动失败"
mv STEREO_DEPTH_INTEGRATION.md docs/integration/ 2>/dev/null || echo "STEREO_DEPTH_INTEGRATION.md 已存在或移动失败"

# 移动调试文档到docs/debug
echo "移动调试文档..."
mv DEBUG_FEATURE_README.md docs/debug/ 2>/dev/null || echo "DEBUG_FEATURE_README.md 已存在或移动失败"
mv LAYOUT_FIX_SUMMARY.md docs/debug/ 2>/dev/null || echo "LAYOUT_FIX_SUMMARY.md 已存在或移动失败"
mv POINT_CLOUD_DEPTH_FIX.md docs/debug/ 2>/dev/null || echo "POINT_CLOUD_DEPTH_FIX.md 已存在或移动失败"

# 移动协议文档到docs/protocols
echo "移动协议文档..."
mv "Rk3588 Stm32 hid raw 通讯协议.md" docs/protocols/ 2>/dev/null || echo "通讯协议文档 已存在或移动失败"

# 移动其他文档到docs/
echo "移动其他文档..."
mv KEYBOARD_SHORTCUTS.md docs/ 2>/dev/null || echo "KEYBOARD_SHORTCUTS.md 已存在或移动失败"
mv test_polyline_measurement.md docs/ 2>/dev/null || echo "test_polyline_measurement.md 已存在或移动失败"

# 4. 整理图片文件
echo "4. 整理图片文件..."

# 移动调试输出图片到images/debug_output
echo "移动调试输出图片..."
mv confidence_map.png images/debug_output/ 2>/dev/null || echo "confidence_map.png 已存在或移动失败"
mv depth_map.png images/debug_output/ 2>/dev/null || echo "depth_map.png 已存在或移动失败"
mv disparity_map.png images/debug_output/ 2>/dev/null || echo "disparity_map.png 已存在或移动失败"
mv mono_depth_calibrated.png images/debug_output/ 2>/dev/null || echo "mono_depth_calibrated.png 已存在或移动失败"
mv mono_depth_raw.png images/debug_output/ 2>/dev/null || echo "mono_depth_raw.png 已存在或移动失败"

# 5. 整理临时文件
echo "5. 整理临时文件..."
mv yolov8_diff.txt temp_files/ 2>/dev/null || echo "yolov8_diff.txt 已存在或移动失败"

# 6. 保留重要文件在根目录
echo "6. 保留重要文件在根目录..."
echo "保留以下重要文件在根目录："
echo "- CMakeLists.txt (项目构建配置)"
echo "- README.md (项目说明文档)"
echo "- SmartScope.desktop (桌面快捷方式)"

# 7. 创建索引文件
echo "7. 创建文件索引..."
cat > docs/FILE_INDEX.md << 'EOF'
# SmartScope 项目文件索引

## 📁 目录结构

### 📂 scripts/
- **root_scripts/**: 项目构建和运行脚本
  - `auto_build_run.sh`: 自动构建和运行脚本
  - `clean_build.sh`: 清理构建脚本
  - `deploy_minimal_smartscope.sh`: 最小化部署脚本
  - `restart.sh`: 重启脚本
  - `run_smartscope.sh`: 运行SmartScope脚本
- **tools/**: 工具脚本
  - `calib.py`: 相机校准工具
  - `camera_diagnosis.py`: 相机诊断工具
  - `camera_test_fixed.py`: 相机测试工具
  - `depth_analysis.py`: 深度分析工具
  - `md_to_word.py`: Markdown转Word工具
  - `send_hid_report.py`: HID报告发送工具
- **refactoring_phase1.sh**: 第一阶段重构脚本
- **refactoring_phase1_safe.sh**: 安全版本第一阶段重构脚本
- **refactoring_phase2.sh**: 第二阶段重构脚本
- **cleanup_script.sh**: 清理脚本

### 📂 docs/
- **analysis/**: 分析文档
  - `CLEANUP_ANALYSIS.md`: 清理分析报告
  - `REFACTORING_ANALYSIS.md`: 重构分析报告
  - `REFACTORING_SUMMARY.md`: 重构总结报告
- **integration/**: 集成文档
  - `DEPTH_ANYTHING_INTEGRATION_ANALYSIS.md`: Depth Anything集成分析
  - `DEPTH_ANYTHING_INTEGRATION_COMPLETE.md`: Depth Anything集成完成报告
  - `DEPTH_MODE_SWITCH_ENHANCEMENT.md`: 深度模式切换增强
  - `STEREO_DEPTH_INTEGRATION.md`: 立体深度集成
- **debug/**: 调试文档
  - `DEBUG_FEATURE_README.md`: 调试功能说明
  - `LAYOUT_FIX_SUMMARY.md`: 布局修复总结
  - `POINT_CLOUD_DEPTH_FIX.md`: 点云深度修复
- **protocols/**: 协议文档
  - `Rk3588 Stm32 hid raw 通讯协议.md`: RK3588 STM32 HID通信协议
- **en/**: 英文文档
- **zh/**: 中文文档
- `KEYBOARD_SHORTCUTS.md`: 键盘快捷键说明
- `test_polyline_measurement.md`: 测试折线测量

### 📂 images/
- **debug_output/**: 调试输出图片
  - `confidence_map.png`: 置信度图
  - `depth_map.png`: 深度图
  - `disparity_map.png`: 视差图
  - `mono_depth_calibrated.png`: 校准后单目深度图
  - `mono_depth_raw.png`: 原始单目深度图
- **test_results/**: 测试结果图片

### 📂 temp_files/
- `yolov8_diff.txt`: YOLOv8差异分析文件

## 📄 根目录重要文件

- `CMakeLists.txt`: 项目主构建配置文件
- `README.md`: 项目主要说明文档
- `SmartScope.desktop`: 桌面快捷方式文件

## 🔧 使用说明

### 运行脚本
```bash
# 构建和运行项目
./scripts/root_scripts/auto_build_run.sh

# 清理构建
./scripts/root_scripts/clean_build.sh

# 运行SmartScope
./scripts/root_scripts/run_smartscope.sh
```

### 使用工具
```bash
# 相机校准
python3 scripts/tools/calib.py

# 相机诊断
python3 scripts/tools/camera_diagnosis.py

# 深度分析
python3 scripts/tools/depth_analysis.py
```

### 查看文档
- 项目分析文档: `docs/analysis/`
- 集成文档: `docs/integration/`
- 调试文档: `docs/debug/`
- 协议文档: `docs/protocols/`
EOF

echo "=== 根目录文件整理完成 ==="
echo "整理结果："
echo "- 脚本文件已移动到 scripts/ 目录"
echo "- 文档文件已移动到 docs/ 目录"
echo "- 图片文件已移动到 images/ 目录"
echo "- 临时文件已移动到 temp_files/ 目录"
echo "- 重要文件保留在根目录"
echo "- 创建了文件索引: docs/FILE_INDEX.md" 