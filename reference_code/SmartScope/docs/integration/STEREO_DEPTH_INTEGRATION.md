# SmartScope stereo_depth_lib 集成文档

## 概述

本文档描述了将 `stereo_depth_lib` 集成到 SmartScope 项目中的过程，完全替换原有的 ONNX IGEV 推理方案。

## 集成内容

### 1. 新增文件

#### 1.1 头文件
- `include/inference/stereo_depth_inference.hpp` - 立体深度推理类头文件

#### 1.2 源文件
- `src/inference/stereo_depth_inference.cpp` - 立体深度推理类实现

#### 1.3 测试文件
- `test_stereo_depth_integration.cpp` - 集成测试程序

### 2. 修改文件

#### 2.1 推理服务
- `include/inference/inference_service.hpp` - 更新接口引用
- `src/inference/inference_service.cpp` - 更新实现

#### 2.2 构建系统
- `CMakeLists.txt` - 添加 stereo_depth_lib 依赖
- `src/inference/CMakeLists.txt` - 更新源文件和链接

### 3. 删除文件

#### 3.1 旧文件
- `include/inference/stereo_inference.hpp` - 旧的 ONNX IGEV 头文件
- `src/inference/stereo_inference.cpp` - 旧的 ONNX IGEV 实现

## 技术实现

### 1. StereoDepthInference 类

#### 1.1 主要功能
- 封装 `stereo_depth_lib` 的 `ComprehensiveDepthProcessor`
- 提供与原有 `StereoInference` 兼容的接口
- 支持性能模式设置
- 提供视差图和点云保存功能

#### 1.2 性能模式
- **HIGH_QUALITY**: 高质量模式，视差范围 256，块大小 7
- **BALANCED**: 平衡模式，视差范围 128，块大小 5
- **FAST**: 快速模式，视差范围 64，块大小 3
- **ULTRA_FAST**: 极速模式，视差范围 32，块大小 3

### 2. 集成方式

#### 2.1 直接替换
- 完全移除 ONNX IGEV 相关代码
- 使用 `stereo_depth_lib` 作为唯一的立体视觉推理引擎
- 保持 SmartScope 的现有架构和接口

#### 2.2 配置管理
- 相机参数目录：`camera_parameters`
- 单目深度模型：`models/depth_anything_v2_vits.rknn`
- 默认性能模式：`ULTRA_FAST`

## 构建和运行

### 1. 前置条件
- `stereo_depth_lib` 已编译并位于 `../stereo_depth_lib/`
- 相机参数文件位于 `camera_parameters/` 目录
- 深度模型文件位于 `models/` 目录

### 2. 构建步骤
```bash
cd reference_code/SmartScope
mkdir build && cd build
cmake ..
make -j4
```

### 3. 运行测试
```bash
# 运行集成测试
./test_stereo_depth_integration

# 运行完整的 SmartScope 应用
./SmartScopeQt
```

## 功能特性

### 1. 立体视觉处理
- 双目图像校正
- SGBM 立体匹配
- 视差图计算
- 深度图生成

### 2. 单目深度估计
- 集成 Depth Anything 模型
- 单目深度预测
- 深度校准和融合

### 3. 点云生成
- 3D 点云重建
- PLY 格式保存
- 点云过滤和优化

### 4. 性能优化
- 多线程处理
- 内存优化
- 硬件加速支持

## 配置参数

### 1. SGBM 参数
```cpp
options_.min_disparity = 0;
options_.num_disparities = 16 * 8;  // 根据性能模式调整
options_.block_size = 5;            // 根据性能模式调整
options_.uniqueness_ratio = 10;
options_.speckle_window = 100;
options_.speckle_range = 32;
```

### 2. 深度校准参数
```cpp
options_.min_samples = 1000;
options_.ransac_max_iterations = 50;
options_.ransac_threshold = 30.0f;
options_.min_inliers_ratio = 10;
```

### 3. 预处理参数
```cpp
options_.scale = 1.0;
options_.bilateral_d = 9;
options_.bilateral_sigma = 75.0;
options_.gauss_kernel = 5;
options_.median_kernel = 3;
```

## 注意事项

### 1. 依赖关系
- 确保 `stereo_depth_lib` 已正确编译
- 检查相机参数文件是否存在
- 验证深度模型文件路径

### 2. 性能考虑
- 根据硬件性能选择合适的性能模式
- 监控内存使用情况
- 优化图像分辨率设置

### 3. 错误处理
- 添加适当的异常处理
- 提供详细的错误信息
- 实现优雅的降级机制

## 未来扩展

### 1. 功能增强
- 支持更多立体匹配算法
- 添加深度图后处理
- 集成更多深度学习模型

### 2. 性能优化
- GPU 加速支持
- 内存池优化
- 并行处理优化

### 3. 用户界面
- 参数调节界面
- 实时性能监控
- 结果可视化增强

## 总结

通过本次集成，SmartScope 成功替换了原有的 ONNX IGEV 推理方案，使用更先进的 `stereo_depth_lib` 进行立体视觉处理。新的实现提供了更好的性能、更丰富的功能和更稳定的运行环境。 