# 相机模块使用指南

本文档详细介绍了Smart Scope Qt相机模块的功能、接口和使用方法，帮助开发者快速上手和集成相机功能。

## 目录

1. [模块概述](#模块概述)
2. [主要类](#主要类)
3. [相机管理器](#相机管理器)
4. [多相机同步](#多相机同步)
5. [非阻塞操作和超时机制](#非阻塞操作和超时机制)
6. [设备类型检测](#设备类型检测)
7. [相机接口](#相机接口)
8. [USB相机实现](#usb相机实现)
9. [帧缓冲区](#帧缓冲区)
10. [异常处理](#异常处理)
11. [使用示例](#使用示例)
12. [常见问题](#常见问题)

## 模块概述

相机模块是Smart Scope Qt的核心模块之一，负责管理系统中的相机设备，提供相机的发现、连接、控制和图像采集功能。该模块采用面向对象的设计，支持多种相机类型，并提供统一的接口。

相机模块主要功能包括：

- 相机设备发现和枚举
- 相机连接和断开
- 相机参数设置和获取
- 图像采集和处理
- 左右相机管理（用于立体视觉）
- 相机状态监控和错误处理
- 相机异常恢复
- 多相机同步
- 非阻塞操作和超时机制
- 设备类型检测

## 主要类

相机模块包含以下主要类：

- `CameraManager`: 相机管理器，单例模式，负责管理所有相机设备
- `Camera`: 相机抽象基类，定义相机的通用接口
- `USBCamera`: USB相机实现，基于V4L2接口
- `FrameBuffer`: 帧缓冲区，用于存储采集的图像帧
- `CaptureWorker`: 图像采集工作线程
- `DecodeWorker`: 图像解码工作线程
- `CameraException`: 相机异常类，用于处理相机操作中的异常

## 相机管理器

`CameraManager`是相机模块的核心类，采用单例模式，负责管理系统中的所有相机设备。

### 主要功能

- 初始化和关闭相机系统
- 发现和枚举可用相机
- 打开和关闭相机
- 管理左右相机（用于立体视觉）
- 开始和停止图像采集
- 处理相机事件和错误
- 相机状态监控和异常恢复
- 多相机同步
- 非阻塞操作和超时机制
- 设备类型检测

### 接口说明

```cpp
// 获取单例实例
static CameraManager& instance();

// 初始化相机管理器
bool initialize();

// 关闭相机管理器
void shutdown();

// 获取可用相机列表
QStringList getAvailableCameras();

// 刷新相机列表
bool refreshCameraList();

// 打开相机
bool openCamera(const QString& cameraId);

// 关闭相机
bool closeCamera(const QString& cameraId);

// 获取相机
Camera* getCamera(const QString& cameraId);

// 获取左相机
Camera* getLeftCamera();

// 获取右相机
Camera* getRightCamera();

// 设置左相机
bool setLeftCamera(const QString& cameraId);

// 设置右相机
bool setRightCamera(const QString& cameraId);

// 获取左相机ID
QString getLeftCameraId() const;

// 获取右相机ID
QString getRightCameraId() const;

// 开始采集
bool startCapture();

// 停止采集
void stopCapture();

// 检查相机是否可访问
bool isCameraAccessible(const QString& cameraId);

// 获取相机总线信息
QString getCameraBusInfo(const QString& cameraId);

// 恢复相机异常状态
bool recoverCamera(const QString& cameraId);

// 检查相机是否正在使用中
bool isCameraInUse(const QString& cameraId);

// 获取相机状态信息
QString getCameraStatus(const QString& cameraId);

// 检查相机初始化是否完成
bool isInitialized() const;

// 检查相机采集是否正在进行
bool isCapturing() const;
```

### 信号

```cpp
// 相机列表变更信号
void cameraListChanged(const QStringList& cameras);

// 新帧信号
void newFrame(const QString& cameraId, const unsigned char* frameData, int width, int height, qint64 timestamp);

// 相机错误信号
void cameraError(const QString& cameraId, const QString& errorMessage);
```

## 多相机同步

相机模块支持多相机同步功能，主要用于立体视觉系统中左右相机的同步采集。同步功能包括以下几个方面：

### 同步策略

相机管理器支持多种同步策略：

1. **无同步(None)**：不进行同步，左右相机独立采集
2. **软件触发同步(SoftwareTrigger)**：通过软件同时触发左右相机采集
3. **时间戳匹配同步(TimestampMatch)**：根据帧时间戳匹配左右相机的帧
4. **组合策略(Combined)**：结合软件触发和时间戳匹配的优点

### 同步接口

```cpp
// 启用/禁用同步
void enableSync(bool enabled);

// 设置同步策略
void setSyncStrategy(SyncStrategy strategy);

// 设置同步时间阈值（毫秒）
void setSyncTimeThreshold(int threshold);

// 设置帧缓冲区大小
void setFrameBufferSize(int size);

// 触发同步采集
bool triggerSyncCapture();

// 同步状态查询
bool isSyncEnabled() const;
SyncStrategy getSyncStrategy() const;
int getSyncTimeThreshold() const;
int getFrameBufferSize() const;
```

### 同步信号

```cpp
// 新同步帧信号
void newSyncFrame(const SyncFrame& syncFrame);

// 同步状态变化信号
void syncStatusChanged(bool enabled);

// 同步策略变化信号
void syncStrategyChanged(SyncStrategy strategy);
```

### 同步帧结构

```cpp
struct SyncFrame {
    QString leftCameraId;       // 左相机ID
    QString rightCameraId;      // 右相机ID
    const unsigned char* leftData;  // 左相机帧数据
    const unsigned char* rightData; // 右相机帧数据
    int leftWidth;              // 左相机帧宽度
    int leftHeight;             // 左相机帧高度
    int rightWidth;             // 右相机帧宽度
    int rightHeight;            // 右相机帧高度
    qint64 leftTimestamp;       // 左相机帧时间戳
    qint64 rightTimestamp;      // 右相机帧时间戳
    qint64 syncTimestamp;       // 同步时间戳
};
```

### 同步工作流程

1. 启用同步功能：`cameraManager.enableSync(true);`
2. 设置同步策略：`cameraManager.setSyncStrategy(SyncStrategy::Combined);`
3. 设置同步参数：
   - 时间阈值：`cameraManager.setSyncTimeThreshold(33);` // 33毫秒，约30fps
   - 缓冲区大小：`cameraManager.setFrameBufferSize(10);` // 缓存10帧
4. 连接同步帧信号：`connect(&cameraManager, &CameraManager::newSyncFrame, ...);`
5. 开始采集：`cameraManager.startCapture();`
6. 接收同步帧并处理

## 非阻塞操作和超时机制

相机模块实现了非阻塞操作和超时机制，避免在相机操作中出现长时间阻塞，提高系统的稳定性和响应性。

### 非阻塞操作

相机管理器的关键操作（如设置左右相机、打开相机等）都实现了非阻塞模式，避免在操作过程中阻塞主线程。

#### 实现原理

1. **设备可访问性检查**：在执行操作前，先检查设备是否可访问，避免尝试访问不可用设备导致阻塞
2. **状态检查**：在执行操作前，检查相机的当前状态，避免重复操作
3. **异步操作**：将耗时操作放在工作线程中执行，避免阻塞主线程
4. **信号通知**：使用信号-槽机制通知操作结果，而不是同步等待

#### 关键接口

```cpp
// 检查相机是否可访问
bool isCameraAccessible(const QString& cameraId);

// 设置左相机（非阻塞）
bool setLeftCamera(const QString& cameraId);

// 设置右相机（非阻塞）
bool setRightCamera(const QString& cameraId);

// 打开相机（非阻塞）
bool openCamera(const QString& cameraId);
```

### 超时机制

相机模块实现了超时机制，避免操作长时间无响应。

#### 实现原理

1. **操作超时检测**：为关键操作设置超时时间，超时后自动取消操作
2. **定时器监控**：使用定时器监控操作执行时间，超时后触发超时处理
3. **超时处理**：超时后自动取消操作，释放资源，并发送错误信号

#### 使用示例

```cpp
// 设置操作超时时间（毫秒）
cameraManager.setOperationTimeout(3000); // 3秒超时

// 执行操作
bool result = cameraManager.setLeftCamera(cameraId);
if (!result) {
    // 操作失败或已超时
    LOG_ERROR("设置左相机失败或超时");
}

// 连接超时信号
connect(&cameraManager, &CameraManager::operationTimeout, 
        [](const QString& operation) {
    LOG_ERROR("操作超时: " + operation);
});
```

## 设备类型检测

设备类型检测是相机模块的重要功能之一，用于识别设备是否支持视频捕获功能，避免使用不支持视频捕获的设备（如HDMI接收器）作为相机。

### 设备类型检测接口

```cpp
/**
 * @brief 检查相机是否支持视频捕获
 * @param cameraId 相机ID
 * @return 是否支持视频捕获
 */
bool isVideoCaptureSupported(const QString& cameraId);
```

### 实现原理

设备类型检测基于V4L2接口，通过查询设备的功能来判断是否支持视频捕获：

1. 打开设备文件
2. 使用`VIDIOC_QUERYCAP`命令查询设备功能
3. 检查设备功能标志中是否包含`V4L2_CAP_VIDEO_CAPTURE`
4. 如果包含，则表示设备支持视频捕获；否则，表示设备不支持视频捕获

### 使用方法

在设置相机之前，应该先检查设备是否支持视频捕获：

```cpp
// 获取相机管理器实例
CameraManager& cameraManager = CameraManager::instance();

// 检查设备是否支持视频捕获
QString cameraId = "/dev/video0";
if (!cameraManager.isVideoCaptureSupported(cameraId)) {
    qDebug() << "设备不支持视频捕获:" << cameraId;
    return;
}

// 设置相机
if (!cameraManager.setLeftCamera(cameraId)) {
    qDebug() << "设置左相机失败";
    return;
}
```

### 设备类型检测的优势

1. **避免使用不支持的设备**：防止程序尝试使用不支持视频捕获的设备，如HDMI接收器
2. **提高系统稳定性**：避免因使用不支持的设备而导致的程序崩溃或阻塞
3. **提供更好的用户体验**：在设备不支持时提供明确的错误信息，而不是无响应
4. **减少调试时间**：快速识别设备类型问题，减少调试时间

### 设备类型检测的实现示例

```cpp
bool CameraManager::isVideoCaptureSupported(const QString& cameraId)
{
    LOG_DEBUG(QString("检查相机是否支持视频捕获: %1").arg(cameraId));
    
    try {
        if (cameraId.isEmpty()) {
            LOG_WARNING("相机ID为空，无法检查视频捕获支持");
            return false;
        }
        
        // 构建完整的设备路径
        QString devicePath = cameraId;
        if (!devicePath.startsWith("/dev/")) {
            devicePath = "/dev/" + devicePath;
        }
        
        // 尝试打开设备文件
        int fd = open(devicePath.toStdString().c_str(), O_RDWR);
        if (fd < 0) {
            LOG_WARNING(QString("相机 %1 不可访问: %2").arg(cameraId).arg(strerror(errno)));
            return false;
        }
        
        // 查询设备功能
        struct v4l2_capability cap = {};
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
            LOG_WARNING(QString("无法查询设备功能，设备ID: %1, 错误: %2").arg(cameraId).arg(strerror(errno)));
            close(fd);
            return false;
        }
        
        // 关闭设备文件
        close(fd);
        
        // 检查是否支持视频捕获
        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            LOG_WARNING(QString("设备不支持视频捕获，设备ID: %1").arg(cameraId));
            return false;
        }
        
        LOG_DEBUG(QString("相机 %1 支持视频捕获").arg(cameraId));
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("检查相机视频捕获支持时发生异常: %1").arg(e.what()));
        return false;
    }
}
```

### 常见设备类型

在Linux系统中，常见的视频设备类型包括：

1. **USB摄像头**：通常支持视频捕获，设备路径如`/dev/video0`、`/dev/video1`等
2. **HDMI接收器**：通常不支持视频捕获，设备路径如`/dev/video0`
3. **视频采集卡**：通常支持视频捕获，设备路径如`/dev/video0`、`/dev/video1`等
4. **虚拟摄像头**：通常支持视频捕获，设备路径如`/dev/video0`、`/dev/video1`等

### 设备类型检测的注意事项

1. **设备路径可能变化**：设备路径可能会因为设备插拔顺序而变化，不应该硬编码设备路径
2. **设备权限**：确保程序有足够的权限访问设备文件
3. **设备占用**：设备可能被其他程序占用，导致无法访问
4. **设备功能变化**：设备功能可能会因为驱动程序更新而变化

## 相机接口

`Camera`是相机的抽象基类，定义了相机的通用接口。所有具体的相机实现都应该继承自这个类。

### 主要功能

- 打开和关闭相机
- 设置和获取相机参数
- 获取图像帧
- 设置分辨率和帧率
- 处理相机事件和错误

### 接口说明

```cpp
// 相机类型枚举
enum class Type {
    USB,        // USB相机
    GigE,       // GigE相机
    HDMI,       // HDMI相机
    CSI,        // CSI相机
    Unknown     // 未知类型
};

// 相机属性枚举
enum class Property {
    Brightness,     // 亮度
    Contrast,       // 对比度
    Saturation,     // 饱和度
    Hue,            // 色调
    Gain,           // 增益
    Exposure,       // 曝光
    Focus,          // 焦距
    WhiteBalance,   // 白平衡
    Gamma,          // 伽马
    Sharpness,      // 锐度
    BacklightComp,  // 背光补偿
    PowerLineFreq,  // 电源频率
    AutoExposure,   // 自动曝光
    AutoWhiteBalance // 自动白平衡
};

// 构造函数
explicit Camera(const QString& deviceId, QObject* parent = nullptr);

// 析构函数
virtual ~Camera();

// 获取设备ID
QString getDeviceId() const;

// 获取相机类型
virtual Type getType() const = 0;

// 打开相机
virtual bool open() = 0;

// 关闭相机
virtual void close() = 0;

// 检查相机是否已打开
virtual bool isOpened() const = 0;

// 获取图像帧
virtual bool getFrame(QImage& image) = 0;

// 设置相机属性
virtual bool setProperty(Property property, double value) = 0;

// 获取相机属性
virtual double getProperty(Property property) const = 0;

// 获取相机名称
virtual QString getName() const = 0;

// 设置分辨率
virtual bool setResolution(int width, int height) = 0;

// 获取当前分辨率
virtual QSize getResolution() const = 0;

// 获取支持的分辨率列表
virtual QList<QSize> getSupportedResolutions() const = 0;

// 设置帧率
virtual bool setFrameRate(double fps) = 0;

// 获取当前帧率
virtual double getFrameRate() const = 0;
```

### 信号

```cpp
// 新帧信号
void newFrame(const QImage& frame);

// 错误信号
void error(const QString& message);
```

## USB相机实现

`USBCamera`是`Camera`的具体实现，基于V4L2接口，用于控制USB相机设备。

### 主要功能

- 基于V4L2接口控制USB相机
- 支持多种图像格式（MJPEG, YUYV等）
- 支持设置相机参数（亮度、对比度、曝光等）
- 支持设置分辨率和帧率
- 使用多线程进行图像采集和解码

### 使用示例

```cpp
// 创建USB相机实例
USBCamera camera("/dev/video0");

// 打开相机
if (!camera.open()) {
    LOG_ERROR("打开相机失败");
    return;
}

// 设置分辨率
camera.setResolution(640, 480);

// 设置帧率
camera.setFrameRate(30.0);

// 设置相机参数
camera.setProperty(Camera::Property::Brightness, 128);
camera.setProperty(Camera::Property::Contrast, 128);
camera.setProperty(Camera::Property::Exposure, -5); // 自动曝光

// 获取图像帧
QImage frame;
if (camera.getFrame(frame)) {
    // 处理图像帧
    processFrame(frame);
}

// 关闭相机
camera.close();
```

## 帧缓冲区

`FrameBuffer`是一个线程安全的缓冲区，用于存储采集的图像帧。它采用生产者-消费者模式，支持多线程环境下的帧数据传输。

### 主要功能

- 线程安全的帧数据存储
- 支持帧数据的入队和出队
- 支持缓冲区大小限制
- 支持缓冲区清空

### 接口说明

```cpp
// 构造函数
explicit FrameBuffer(int maxSize = 1);

// 析构函数
~FrameBuffer();

// 入队帧数据
bool push(void* data, size_t size, QElapsedTimer* timestamp = nullptr);

// 出队帧数据
bool pop(FrameData& frame);

// 清空缓冲区
void clear();

// 获取缓冲区大小
int size() const;

// 检查缓冲区是否为空
bool isEmpty() const;

// 检查缓冲区是否已满
bool isFull() const;
```

## 异常处理

相机模块使用`CameraException`类处理相机操作中的异常。该类继承自`AppException`，提供了相机特定的异常处理功能。

### 主要功能

- 处理相机操作中的异常
- 提供详细的异常信息（文件、行号、函数名）
- 支持异常的格式化输出

### 使用示例

```cpp
#include "infrastructure/exception/camera_exception.h"

try {
    // 可能抛出异常的相机操作
    if (!camera.open()) {
        THROW_CAMERA_EXCEPTION("打开相机失败");
    }
} catch (const CameraException& e) {
    LOG_ERROR(QString("相机异常: %1").arg(e.getFormattedMessage()));
    // 处理异常
}
```

## 使用示例

### 基本使用

```cpp
// 创建相机管理器
CameraManager cameraManager;

// 初始化
if (cameraManager.initialize()) {
    // 刷新相机列表
    cameraManager.refreshCameraList();
    
    // 获取相机列表
    QStringList cameras = cameraManager.getCameraList();
    
    if (cameras.size() >= 2) {
        // 设置左右相机
        cameraManager.setLeftCamera(cameras[0]);
        cameraManager.setRightCamera(cameras[1]);
        
        // 开始采集
        cameraManager.startCapture();
        
        // ... 处理图像 ...
        
        // 停止采集
        cameraManager.stopCapture();
    }
}
```

### 处理相机事件

```cpp
// 连接信号
connect(&cameraManager, &CameraManager::newFrame, 
        [](const QString& cameraId, const unsigned char* frameData, int width, int height, qint64 timestamp) {
    // 处理新帧
});

connect(&cameraManager, &CameraManager::cameraError,
        [](const QString& cameraId, int errorCode, const QString& errorMessage) {
    // 处理错误
});

connect(&cameraManager, &CameraManager::cameraStatusChanged,
        [](const QString& cameraId, const QString& status) {
    // 处理状态变化
});
```

### 使用同步功能

```cpp
// 启用同步
cameraManager.enableSync(true);

// 设置同步策略
cameraManager.setSyncStrategy(SyncStrategy::Combined);

// 设置同步参数
cameraManager.setSyncTimeThreshold(33);  // 33毫秒
cameraManager.setFrameBufferSize(10);    // 缓存10帧

// 连接同步帧信号
connect(&cameraManager, &CameraManager::newSyncFrame,
        [](const SyncFrame& syncFrame) {
    // 处理同步帧
    // 例如：立体匹配、3D重建等
});

// 开始采集
cameraManager.startCapture();

// ... 处理同步图像 ...

// 停止采集
cameraManager.stopCapture();

// 禁用同步
cameraManager.enableSync(false);
```

### 恢复相机错误

```cpp
try {
    // 尝试操作相机
    cameraManager.startCapture();
} catch (const CameraException& e) {
    // 处理异常
    qDebug() << "相机异常: " << e.what();
    
    // 尝试恢复相机
    QString errorCameraId = e.getContext().value("cameraId").toString();
    if (!errorCameraId.isEmpty()) {
        if (cameraManager.recoverCamera(errorCameraId)) {
            qDebug() << "相机恢复成功";
        } else {
            qDebug() << "相机恢复失败";
        }
    }
}
```

## 常见问题

### 1. 相机无法打开

**Q: 为什么相机无法打开？**

A: 相机无法打开可能有以下原因：

1. 相机设备不存在或已断开连接
2. 相机设备权限不足（需要sudo权限）
3. 相机已被其他进程占用
4. 相机驱动程序不兼容或未安装

解决方法：

```cpp
// 检查相机是否可访问
if (!cameraManager.isCameraAccessible(cameraId)) {
    LOG_ERROR(QString("相机 %1 不可访问").arg(cameraId));
    
    // 检查设备是否存在
    QFile deviceFile("/dev/" + cameraId);
    if (!deviceFile.exists()) {
        LOG_ERROR(QString("相机设备 %1 不存在").arg(cameraId));
        return;
    }
    
    // 检查设备权限
    QFileInfo fileInfo(deviceFile);
    if (!fileInfo.isReadable() || !fileInfo.isWritable()) {
        LOG_ERROR(QString("相机设备 %1 权限不足").arg(cameraId));
        return;
    }
    
    // 检查设备是否被占用
    if (cameraManager.isCameraInUse(cameraId)) {
        LOG_ERROR(QString("相机设备 %1 已被其他进程占用").arg(cameraId));
        return;
    }
}
```

### 2. 图像质量问题

**Q: 如何提高图像质量？**

A: 可以通过调整相机参数来提高图像质量：

```cpp
// 获取相机
Camera* camera = cameraManager.getCamera(cameraId);
if (camera) {
    // 调整亮度
    camera->setProperty(Camera::Property::Brightness, 128);
    
    // 调整对比度
    camera->setProperty(Camera::Property::Contrast, 128);
    
    // 调整饱和度
    camera->setProperty(Camera::Property::Saturation, 128);
    
    // 调整曝光
    camera->setProperty(Camera::Property::Exposure, -5); // 自动曝光
    
    // 调整白平衡
    camera->setProperty(Camera::Property::WhiteBalance, 4600); // 色温4600K
    
    // 调整锐度
    camera->setProperty(Camera::Property::Sharpness, 128);
}
```

### 3. 性能问题

**Q: 如何提高相机采集性能？**

A: 可以通过以下方法提高相机采集性能：

1. 降低分辨率
2. 降低帧率
3. 使用硬件加速的图像格式（如MJPEG）
4. 优化图像处理算法
5. 使用多线程处理

```cpp
// 设置较低的分辨率
camera->setResolution(640, 480);

// 设置较低的帧率
camera->setFrameRate(15.0);

// 使用MJPEG格式（如果相机支持）
// 这通常在USBCamera的实现中自动处理

// 使用多线程处理图像
// 这在CameraManager中已经实现
```

### 4. 多相机同步问题

**Q: 如何同步多个相机的采集？**

A: 多相机同步是一个复杂的问题，可以通过以下方法实现：

1. 软件触发同步：在软件层面同时启动多个相机的采集
2. 硬件触发同步：使用外部触发信号同步多个相机
3. 时间戳同步：根据时间戳匹配来自不同相机的帧

```cpp
// 软件触发同步示例
void synchronizeCameras(const QStringList& cameraIds)
{
    CameraManager& cameraManager = CameraManager::instance();
    
    // 准备所有相机
    for (const QString& cameraId : cameraIds) {
        if (!cameraManager.openCamera(cameraId)) {
            LOG_ERROR(QString("打开相机 %1 失败").arg(cameraId));
            return;
        }
    }
    
    // 同时启动所有相机的采集
    for (const QString& cameraId : cameraIds) {
        Camera* camera = cameraManager.getCamera(cameraId);
        if (camera) {
            // 这里可以使用一些低级API来实现更精确的同步
            // 例如，先准备好所有相机，然后同时触发
        }
    }
}
```

### 5. 相机断开连接处理

**Q: 如何处理相机断开连接的情况？**

A: 可以通过监听相机错误信号来处理相机断开连接的情况：

```cpp
// 连接相机错误信号
connect(&cameraManager, &CameraManager::cameraError,
        [](const QString& cameraId, const QString& errorMessage) {
    // 检查是否是断开连接错误
    if (errorMessage.contains("disconnected") || errorMessage.contains("no such device")) {
        LOG_ERROR(QString("相机 %1 已断开连接").arg(cameraId));
        
        // 尝试重新连接
        QTimer::singleShot(1000, [cameraId]() {
            CameraManager& cameraManager = CameraManager::instance();
            
            // 刷新相机列表
            cameraManager.refreshCameraList();
            
            // 检查相机是否重新连接
            if (cameraManager.getAvailableCameras().contains(cameraId)) {
                LOG_INFO(QString("相机 %1 已重新连接").arg(cameraId));
                
                // 重新打开相机
                if (cameraManager.openCamera(cameraId)) {
                    LOG_INFO(QString("重新打开相机 %1 成功").arg(cameraId));
                } else {
                    LOG_ERROR(QString("重新打开相机 %1 失败").arg(cameraId));
                }
            } else {
                LOG_ERROR(QString("相机 %1 未重新连接").arg(cameraId));
            }
        });
    }
});
``` 