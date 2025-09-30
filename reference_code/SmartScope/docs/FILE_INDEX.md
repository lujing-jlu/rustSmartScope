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
