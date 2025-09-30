# 相机校正管理器使用指南

## 概述

`CameraCorrectionManager` 是一个统一的相机校正接口，整合了畸变校正、立体校正、深度校准和图像变换功能，提供了便捷的调用方式。

## 主要特性

- **统一接口**: 一个管理器处理所有校正功能
- **模块化设计**: 可选择启用/禁用特定校正功能
- **高性能**: 支持硬件加速和缓存优化
- **易于使用**: 提供工厂类简化创建过程
- **信号支持**: 异步操作和进度通知
- **统计信息**: 内置性能监控和统计

## 快速开始

### 1. 基本使用

```cpp
#include "core/camera/camera_correction_manager.h"
#include "core/camera/camera_correction_factory.h"

// 创建标准校正管理器
auto manager = CameraCorrectionFactory::createStandardCorrectionManager();

// 执行完整校正
cv::Mat leftImage, rightImage;
CorrectionResult result = manager->correctImages(leftImage, rightImage, CorrectionType::ALL);

if (result.success) {
    // 使用校正后的图像
    cv::Mat correctedLeft = result.correctedLeftImage;
    cv::Mat correctedRight = result.correctedRightImage;
}
```

### 2. 快速校正（仅畸变校正）

```cpp
// 创建快速校正管理器
auto manager = CameraCorrectionFactory::createFastCorrectionManager();

// 仅执行畸变校正
CorrectionResult result = manager->correctImages(
    leftImage, rightImage, 
    CorrectionType::DISTORTION | CorrectionType::STEREO_RECTIFICATION
);
```

### 3. 自定义配置

```cpp
// 创建自定义配置
CorrectionConfig config;
config.cameraParametersPath = "./camera_parameters";
config.imageSize = cv::Size(1280, 720);
config.enableDistortionCorrection = true;
config.enableDepthCalibration = false;  // 禁用深度校准
config.rotationDegrees = 90;            // 设置旋转角度
config.flipHorizontal = true;           // 启用水平翻转

// 创建自定义校正管理器
auto manager = CameraCorrectionFactory::createCustomCorrectionManager(config);
```

## 校正类型

### CorrectionType 枚举

```cpp
enum class CorrectionType {
    NONE = 0,                    // 无校正
    DISTORTION = 1,              // 畸变校正
    STEREO_RECTIFICATION = 2,    // 立体校正
    DEPTH_CALIBRATION = 4,       // 深度校准
    IMAGE_TRANSFORM = 8,         // 图像变换
    ALL = DISTORTION | STEREO_RECTIFICATION | DEPTH_CALIBRATION | IMAGE_TRANSFORM
};
```

### 组合使用

```cpp
// 仅畸变校正
CorrectionType::DISTORTION

// 畸变校正 + 立体校正
CorrectionType::DISTORTION | CorrectionType::STEREO_RECTIFICATION

// 所有校正
CorrectionType::ALL
```

## 图像变换功能

### 支持的变换类型

```cpp
enum class RGATransform {
    None = 0,           // 无变换
    Rotate90 = 1,       // 顺时针旋转90度
    Rotate180 = 2,      // 旋转180度
    Rotate270 = 3,      // 顺时针旋转270度
    FlipHorizontal = 4, // 水平翻转
    FlipVertical = 5,   // 垂直翻转
    Scale2X = 6,        // 放大2倍
    ScaleHalf = 7,      // 缩小一半
    Invert = 8          // 反色
};
```

### 使用示例

```cpp
// 应用单个变换
cv::Mat transformed = manager->applyImageTransform(image, RGATransform::Rotate90);

// 设置变换参数
manager->setImageTransformParams(
    90,        // 旋转角度
    true,      // 水平翻转
    false,     // 垂直翻转
    false,     // 颜色反转
    1.2f       // 缩放比例
);

// 应用所有变换
CorrectionResult result = manager->applyImageTransforms(leftImage, rightImage);
```

## 深度校准

### 基本使用

```cpp
// 执行深度校准
cv::Mat monoDepth, stereoDepth, disparity, validMask;
auto depthResult = manager->calibrateDepth(monoDepth, stereoDepth, disparity, validMask);

if (depthResult.success) {
    double scale = depthResult.scale_factor;
    double bias = depthResult.bias;
    double rmsError = depthResult.rms_error;
}
```

### 校准配置

```cpp
// 获取深度校准器
auto calibrator = manager->getDepthCalibrator();

// 设置校准选项
stereo_depth::ImprovedCalibrationOptions options;
options.ransac_threshold = 5.0f;
options.min_samples = 500;
options.min_inliers_ratio = 30;
options.max_iterations = 100;
options.enable_outlier_detection = true;
options.outlier_threshold = 2.0f;

calibrator->setOptions(options);
```

## 信号和槽

### 连接信号

```cpp
// 连接校正完成信号
connect(manager.get(), &CameraCorrectionManager::correctionCompleted,
        this, [](const CorrectionResult& result) {
    qDebug() << "Correction completed:" << result.success;
});

// 连接错误信号
connect(manager.get(), &CameraCorrectionManager::correctionError,
        this, [](const QString& error) {
    qDebug() << "Correction error:" << error;
});

// 连接进度信号
connect(manager.get(), &CameraCorrectionManager::correctionProgress,
        this, [](int progress) {
    qDebug() << "Progress:" << progress << "%";
});
```

## 配置选项

### CorrectionConfig 结构

```cpp
struct CorrectionConfig {
    // 基础配置
    QString cameraParametersPath = "./camera_parameters";
    cv::Size imageSize = cv::Size(1280, 720);
    
    // 校正功能配置
    bool enableDistortionCorrection = true;
    bool enableStereoRectification = true;
    bool enableDepthCalibration = true;
    bool enableImageTransform = true;
    
    // 图像变换参数
    int rotationDegrees = 0;
    bool flipHorizontal = false;
    bool flipVertical = false;
    bool invertColors = false;
    float zoomScale = 1.0f;
    
    // 性能配置
    bool useHardwareAcceleration = true;
    bool precomputeMaps = true;
    int maxCacheSize = 100;
};
```

## 性能优化

### 缓存配置

```cpp
CorrectionConfig config;
config.precomputeMaps = true;    // 预计算映射表
config.maxCacheSize = 200;       // 增加缓存大小
config.useHardwareAcceleration = true;  // 启用硬件加速
```

### 统计信息

```cpp
// 获取校正统计信息
QString stats = manager->getCorrectionStatistics();
qDebug() << stats;

// 输出示例:
// Correction Statistics:
// Total Corrections: 100
// Successful Corrections: 98
// Success Rate: 98%
// Average Processing Time: 15.2ms
// Cache Hits: 85
// Cache Misses: 15
// Cache Hit Rate: 85%
```

## 错误处理

### 常见错误

1. **初始化失败**
   ```cpp
   if (!manager->isInitialized()) {
       qDebug() << "Manager not initialized";
   }
   ```

2. **参数文件缺失**
   ```cpp
   // 检查相机参数文件是否存在
   QDir paramDir("./camera_parameters");
   if (!paramDir.exists()) {
       qDebug() << "Camera parameters directory not found";
   }
   ```

3. **图像格式错误**
   ```cpp
   if (leftImage.empty() || rightImage.empty()) {
       qDebug() << "Input images are empty";
   }
   ```

### 验证配置

```cpp
// 验证配置
auto [valid, error] = CameraCorrectionFactory::validateConfig(config);
if (!valid) {
    qDebug() << "Invalid config:" << error;
}
```

## 最佳实践

1. **使用工厂类**: 优先使用 `CameraCorrectionFactory` 创建管理器
2. **预初始化**: 在应用启动时初始化校正管理器
3. **缓存优化**: 启用预计算映射表和缓存
4. **错误处理**: 始终检查校正结果的成功状态
5. **性能监控**: 定期查看统计信息优化性能
6. **资源管理**: 使用智能指针管理内存

## 完整示例

```cpp
#include "core/camera/camera_correction_manager.h"
#include "core/camera/camera_correction_factory.h"

class MyApplication : public QObject {
    Q_OBJECT

public:
    void initializeCorrection() {
        // 创建校正管理器
        m_correctionManager = CameraCorrectionFactory::createStandardCorrectionManager(this);
        
        // 连接信号
        connect(m_correctionManager.get(), &CameraCorrectionManager::correctionCompleted,
                this, &MyApplication::onCorrectionCompleted);
    }
    
    void processImages(const cv::Mat& left, const cv::Mat& right) {
        // 执行校正
        auto result = m_correctionManager->correctImages(left, right, CorrectionType::ALL);
        
        if (result.success) {
            // 处理校正后的图像
            processCorrectedImages(result.correctedLeftImage, result.correctedRightImage);
        }
    }

private slots:
    void onCorrectionCompleted(const CorrectionResult& result) {
        qDebug() << "Correction completed in" << result.processingTimeMs << "ms";
    }

private:
    std::shared_ptr<CameraCorrectionManager> m_correctionManager;
};
```

这个包装器提供了统一、易用的相机校正接口，大大简化了校正功能的调用和使用。
