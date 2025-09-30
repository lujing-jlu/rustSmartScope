# 新的智能相机监控系统

## 概述

基于用户需求，我们重构了相机监控系统，实现了以下核心特性：

1. **单独的监控线程**：使用独立线程监控可用相机
2. **智能相机识别**：自动识别左相机和右相机
3. **智能模式切换**：只有左右相机都插入时才切换为双目模式
4. **无相机模式**：默认保持无相机模式继续检测

## 系统架构

### 核心组件

1. **CameraMonitorThread** - 相机监控线程
   - 独立线程运行
   - 监控系统中的相机设备变化
   - 识别左右相机类型
   - 发射状态变化信号

2. **SmartCameraManager** - 智能相机管理器
   - 基于监控线程的高级管理器
   - 实现智能模式切换逻辑
   - 提供简化的API接口

3. **MultiCameraManager** - 多相机管理器（底层）
   - 实际的相机操作和数据处理
   - 线程管理和帧缓冲

## 工作流程

### 1. 系统启动
```
启动应用 → 初始化SmartCameraManager → 启动CameraMonitorThread → 开始监控
```

### 2. 相机检测流程
```
扫描设备 → V4L2验证 → 相机类型识别 → 状态更新 → 信号发射
```

### 3. 模式切换逻辑
```
无相机模式 ←→ 左相机检测 ←→ 右相机检测 → 双目模式就绪 → 启动双目模式
     ↑                                                    ↓
     ←←←←←←←←←←←← 任一相机断开 ←←←←←←←←←←←←←←←←←←←←←←←←←←
```

## 关键特性

### 🎯 智能模式切换

- **等待策略**：只有左右相机都存在时才切换为双目模式
- **保持检测**：其他情况下保持无相机模式继续检测
- **自动恢复**：相机断开后自动回到无相机模式

### 🔍 相机识别机制

```cpp
// 左相机识别名称（可配置）
QStringList leftCameraNames = {
    "YTXB: YTXB (usb-fc800000.usb-1.3)",
    "cameraL",
    "Web Camera 2Ks",
    "Left Camera"
};

// 右相机识别名称（可配置）
QStringList rightCameraNames = {
    "YTXB: YTXB (usb-fc880000.usb-1.4.3)",
    "cameraR", 
    "USB Camera",
    "Right Camera"
};
```

### 🔄 状态监控

- **MonitorState**：监控线程状态
  - `NO_CAMERA` - 无相机
  - `LEFT_ONLY` - 仅左相机
  - `RIGHT_ONLY` - 仅右相机
  - `BINOCULAR_READY` - 双目就绪

- **CameraWorkMode**：工作模式
  - `NO_CAMERA` - 无相机模式
  - `BINOCULAR` - 双目模式

## 使用方法

### 1. 基本使用

```cpp
#include "core/camera/smart_camera_manager.h"

using namespace SmartScope::Core::CameraUtils;

// 获取管理器实例
SmartCameraManager& manager = SmartCameraManager::instance();

// 初始化
manager.initialize();

// 连接信号
connect(&manager, &SmartCameraManager::workModeChanged,
        this, &MyClass::onWorkModeChanged);
connect(&manager, &SmartCameraManager::binocularModeReady,
        this, &MyClass::onBinocularReady);
```

### 2. 状态查询

```cpp
// 获取当前工作模式
CameraWorkMode mode = manager.getCurrentWorkMode();

// 检查是否支持3D测量
bool can3D = manager.is3DMeasurementAvailable();

// 获取相机状态
CameraStatus leftStatus = manager.getLeftCameraStatus();
CameraStatus rightStatus = manager.getRightCameraStatus();
```

### 3. 配置相机识别

```cpp
// 设置左相机识别名称
QStringList leftNames = {"My Left Camera", "Camera L"};
manager.setLeftCameraNames(leftNames);

// 设置右相机识别名称  
QStringList rightNames = {"My Right Camera", "Camera R"};
manager.setRightCameraNames(rightNames);
```

## 信号说明

### SmartCameraManager 信号

- `workModeChanged(CameraWorkMode oldMode, CameraWorkMode newMode)`
  - 工作模式改变时发射

- `leftCameraStatusChanged(const CameraStatus& status)`
  - 左相机状态改变时发射

- `rightCameraStatusChanged(const CameraStatus& status)`
  - 右相机状态改变时发射

- `binocularModeReady()`
  - 双目模式就绪时发射

- `measurement3DAvailabilityChanged(bool available)`
  - 3D测量可用性改变时发射

- `cameraListUpdated(const QList<CameraDeviceInfo>& cameras)`
  - 相机列表更新时发射

## 配置文件支持

系统支持通过配置文件设置相机识别名称：

```toml
[camera.left]
name = ["YTXB: YTXB (usb-fc800000.usb-1.3)", "cameraL", "Web Camera 2Ks"]

[camera.right]  
name = ["YTXB: YTXB (usb-fc880000.usb-1.4.3)", "cameraR", "USB Camera"]

[camera]
width = 1280
height = 720
fps = 30
```

## 优势对比

### 新系统 vs 旧系统

| 特性 | 旧系统 | 新系统 |
|------|--------|--------|
| 监控方式 | 多个定时器 + 文件监视器 | 单独监控线程 |
| 模式切换 | 复杂的状态机 | 简化的智能逻辑 |
| 相机识别 | 分散在多个类中 | 集中在监控线程 |
| 错误恢复 | 多次重连尝试 | 持续监控自动恢复 |
| API复杂度 | 高（多个管理器） | 低（单一入口） |
| 线程安全 | 多个互斥锁 | 简化的锁机制 |

## 示例程序

参考 `src/core/camera/camera_usage_example.cpp` 获取完整的使用示例。

## 编译说明

新的源文件已添加到 `src/core/CMakeLists.txt` 中，重新编译项目即可使用新系统。

## 迁移指南

如果要从旧的相机系统迁移到新系统：

1. 将 `CameraStateManager` 替换为 `SmartCameraManager`
2. 更新信号连接代码
3. 简化状态查询逻辑
4. 移除手动重连代码（新系统自动处理）

新系统提供了更简洁、更可靠的相机管理方案，特别适合需要智能双目模式切换的应用场景。
