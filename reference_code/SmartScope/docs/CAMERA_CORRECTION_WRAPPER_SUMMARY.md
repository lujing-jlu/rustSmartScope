# 相机校正包装器总结

## 概述

我已经为SmartScope项目创建了一个完整的相机校正包装器，将原本分散的校正功能整合为一个统一、易用的接口。

## 创建的文件

### 1. 核心类文件

#### `camera_correction_manager.h/cpp`
- **CameraCorrectionManager**: 主要的校正管理器类
- 整合了畸变校正、立体校正、深度校准和图像变换功能
- 提供统一的接口和配置管理
- 支持信号槽机制和统计信息

#### `camera_correction_factory.h/cpp`
- **CameraCorrectionFactory**: 工厂类，简化管理器创建
- 提供标准、快速、完整和自定义配置选项
- 包含配置验证功能

### 2. 示例文件

#### `camera_correction_example.cpp`
- 完整的使用示例
- 展示各种校正功能的使用方法
- 包含错误处理和性能测试

#### `integration_example.cpp`
- 集成示例，展示如何在现有代码中使用
- 包含与HomePage的集成示例

### 3. 文档文件

#### `CAMERA_CORRECTION_USAGE.md`
- 详细的使用指南
- 包含API文档和最佳实践
- 提供完整的代码示例

#### `CAMERA_CORRECTION_WRAPPER_SUMMARY.md`
- 本总结文档

### 4. 构建文件

#### `src/core/camera/CMakeLists.txt`
- CMake构建配置
- 依赖管理和安装规则

## 主要特性

### 1. 统一接口
```cpp
// 一个管理器处理所有校正功能
auto manager = CameraCorrectionFactory::createStandardCorrectionManager();
CorrectionResult result = manager->correctImages(leftImage, rightImage, CorrectionType::ALL);
```

### 2. 模块化设计
```cpp
// 可选择启用/禁用特定功能
CorrectionConfig config;
config.enableDistortionCorrection = true;
config.enableDepthCalibration = false;  // 禁用深度校准
```

### 3. 多种创建方式
```cpp
// 标准配置
auto standard = CameraCorrectionFactory::createStandardCorrectionManager();

// 快速配置（仅畸变校正）
auto fast = CameraCorrectionFactory::createFastCorrectionManager();

// 完整配置（所有功能）
auto full = CameraCorrectionFactory::createFullCorrectionManager();

// 自定义配置
auto custom = CameraCorrectionFactory::createCustomCorrectionManager(config);
```

### 4. 信号支持
```cpp
// 异步操作和进度通知
connect(manager.get(), &CameraCorrectionManager::correctionCompleted,
        this, &MyClass::onCorrectionCompleted);
connect(manager.get(), &CameraCorrectionManager::correctionError,
        this, &MyClass::onCorrectionError);
```

### 5. 统计信息
```cpp
// 性能监控
QString stats = manager->getCorrectionStatistics();
// 输出：总校正次数、成功率、平均处理时间、缓存命中率等
```

## 校正类型支持

### 1. 畸变校正
- 基于相机内参和畸变系数
- 使用预计算的重映射表
- 支持实时校正

### 2. 立体校正
- 双目相机外参标定
- 立体校正参数计算
- ROI区域裁剪

### 3. 深度校准
- 单目与双目深度对齐
- 支持多种校准算法
- 异常值检测和过滤

### 4. 图像变换
- 旋转、翻转、缩放
- 颜色反转
- 硬件加速支持

## 使用场景

### 1. 实时相机校正
```cpp
// 在相机回调中应用校正
void onCameraFrame(const cv::Mat& left, const cv::Mat& right) {
    auto result = m_correctionManager->correctImages(left, right, CorrectionType::ALL);
    if (result.success) {
        displayCorrectedImages(result.correctedLeftImage, result.correctedRightImage);
    }
}
```

### 2. 批量图像处理
```cpp
// 处理保存的图像文件
for (const auto& imagePair : imagePairs) {
    auto result = manager->correctImages(imagePair.left, imagePair.right, CorrectionType::DISTORTION);
    saveCorrectedImages(result.correctedLeftImage, result.correctedRightImage);
}
```

### 3. 深度测量应用
```cpp
// 执行深度校准
auto depthResult = manager->calibrateDepth(monoDepth, stereoDepth, disparity);
if (depthResult.success) {
    applyDepthCalibration(depthResult.scale_factor, depthResult.bias);
}
```

## 性能优化

### 1. 预计算映射表
- 避免实时计算畸变校正映射
- 显著提升处理速度

### 2. 缓存机制
- 智能缓存常用变换结果
- 可配置缓存大小

### 3. 硬件加速
- 支持RGA硬件加速
- 自动选择最优处理方式

### 4. 内存优化
- 智能内存管理
- 避免不必要的内存拷贝

## 集成到现有代码

### 1. 在HomePage中集成
```cpp
class HomePage {
private:
    std::shared_ptr<CameraCorrectionManager> m_correctionManager;
    
    void initializeCorrection() {
        m_correctionManager = CameraCorrectionFactory::createStandardCorrectionManager(this);
    }
    
    void updateCameraFrames() {
        // 获取原始图像
        cv::Mat left, right;
        getCurrentFrames(left, right);
        
        // 应用校正
        auto result = m_correctionManager->correctImages(left, right, CorrectionType::ALL);
        if (result.success) {
            displayImages(result.correctedLeftImage, result.correctedRightImage);
        }
    }
};
```

### 2. 在测量页面中集成
```cpp
class MeasurementPage {
    void processMeasurementImages() {
        auto result = m_correctionManager->correctImages(leftImage, rightImage, 
            CorrectionType::DISTORTION | CorrectionType::STEREO_RECTIFICATION);
        
        if (result.success) {
            performMeasurement(result.correctedLeftImage, result.correctedRightImage);
        }
    }
};
```

## 配置管理

### 1. 相机参数路径
```cpp
config.cameraParametersPath = "./camera_parameters";
```

### 2. 图像尺寸
```cpp
config.imageSize = cv::Size(1280, 720);
```

### 3. 功能开关
```cpp
config.enableDistortionCorrection = true;
config.enableStereoRectification = true;
config.enableDepthCalibration = true;
config.enableImageTransform = true;
```

### 4. 性能配置
```cpp
config.useHardwareAcceleration = true;
config.precomputeMaps = true;
config.maxCacheSize = 100;
```

## 错误处理

### 1. 初始化检查
```cpp
if (!manager->isInitialized()) {
    LOG_ERROR("Correction manager not initialized");
    return;
}
```

### 2. 结果验证
```cpp
if (!result.success) {
    LOG_ERROR(QString("Correction failed: %1").arg(result.errorMessage));
    return;
}
```

### 3. 配置验证
```cpp
auto [valid, error] = CameraCorrectionFactory::validateConfig(config);
if (!valid) {
    LOG_ERROR(QString("Invalid config: %1").arg(error));
    return;
}
```

## 优势总结

1. **简化使用**: 一个接口处理所有校正功能
2. **提高效率**: 预计算和缓存优化
3. **易于维护**: 模块化设计和统一接口
4. **功能完整**: 支持所有现有校正功能
5. **扩展性强**: 易于添加新的校正类型
6. **性能优化**: 硬件加速和智能缓存
7. **错误处理**: 完善的错误检查和报告
8. **统计监控**: 内置性能统计和监控

这个包装器大大简化了相机校正功能的使用，提高了代码的可维护性和性能，为SmartScope项目提供了一个强大而易于使用的校正接口。
