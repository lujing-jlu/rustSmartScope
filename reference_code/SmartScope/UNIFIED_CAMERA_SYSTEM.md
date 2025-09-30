# SmartScope 统一相机管理系统

## 🎯 系统概述

已成功为SmartScope项目实现了**统一的智能相机管理系统**，支持**无相机/单相机/双相机**三种模式的自动检测和切换。

## ✅ 实现的功能

### 🔧 核心特性

1. **智能三模式管理**
   - 无相机模式：自动检测，显示提示信息
   - 单相机模式：启用基础功能，禁用双目功能
   - 双相机模式：启用全功能，支持立体视觉

2. **自动设备检测**
   - V4L2设备扫描
   - 左右相机自动识别和配对
   - 热插拔检测支持
   - 智能重连机制

3. **自动读流管理**
   - 根据模式自动启动对应读流
   - 单相机：独立帧回调
   - 双相机：同步帧回调
   - 错误恢复和重连

4. **状态通知系统**
   - 实时模式变化通知
   - 设备连接/断开事件
   - Toast消息提示
   - 日志记录

## 📁 新增的文件

### 核心类文件
```
include/core/camera/unified_camera_manager.h     # 统一相机管理器头文件
src/core/camera/unified_camera_manager.cpp       # 统一相机管理器实现
```

### 测试和示例
```
src/tests/test_unified_camera_manager.cpp        # 功能测试程序
examples/unified_camera_example.cpp              # 使用示例
test_unified_camera.sh                          # 测试脚本
```

### 文档
```
UNIFIED_CAMERA_SYSTEM.md                        # 本文档
```

## 🔧 配置说明

### config.toml 更新

```toml
[camera]
# 统一相机管理器配置
auto_detection = true           # 启用自动检测
detection_interval = 2000       # 检测间隔(毫秒)
streaming_buffer_size = 4       # 读流缓冲区大小

[camera.left]
name = ["cameral", "left", "video3", "1.3", "ytxb.*1.3", "YTXB.*usb-.*1.3"]
device_priority = ["/dev/video3", "/dev/video1", "/dev/video0"]

[camera.right]
name = ["camerar", "right", "video1", "1.4.3", "ytxb.*1.4.3", "YTXB.*usb-.*1.4.3"]
device_priority = ["/dev/video1", "/dev/video3", "/dev/video2"]
```

## 💻 使用方法

### 基本使用

```cpp
#include "core/camera/unified_camera_manager.h"

using namespace SmartScope::Core::CameraUtils;

// 获取管理器实例
UnifiedCameraManager& manager = UnifiedCameraManager::instance();

// 连接信号
connect(&manager, &UnifiedCameraManager::operationModeChanged,
        this, &MyClass::onModeChanged);

connect(&manager, &UnifiedCameraManager::newSyncFrames,
        this, &MyClass::onSyncFrames);

// 初始化
if (!manager.initialize()) {
    qDebug() << "初始化失败";
    return;
}

// 查询状态
CameraOperationMode mode = manager.getCurrentMode();
QString leftCameraId = manager.getLeftCameraId();
QString rightCameraId = manager.getRightCameraId();
```

### 模式处理示例

```cpp
void MyClass::onModeChanged(CameraOperationMode newMode, CameraOperationMode oldMode)
{
    switch (newMode) {
        case CameraOperationMode::NO_CAMERA:
            // 显示"请连接相机"提示
            showNoCameraPrompt();
            disableAllFeatures();
            break;

        case CameraOperationMode::SINGLE_CAMERA:
            // 启用基础功能，禁用双目功能
            enableBasicFeatures();
            disableStereoFeatures();
            break;

        case CameraOperationMode::DUAL_CAMERA:
            // 启用全部功能
            enableAllFeatures();
            break;
    }
}
```

## 🔄 架构集成

### MainWindow 集成

已将统一相机管理器完全集成到MainWindow中：

1. **初始化**: 在MainWindow构造函数中自动初始化
2. **状态通知**: 通过Toast显示模式变化
3. **生命周期管理**: 在窗口关闭时正确释放资源
4. **页面通知**: 自动通知各页面更新UI状态

### 与现有系统的关系

```
应用层 (MainWindow, HomePage, MeasurementPage...)
    ↓ 接收模式变化信号
统一相机管理器 (UnifiedCameraManager)
    ↓ 管理底层读流
多相机管理器 (MultiCameraManager)
    ↓ OpenCV读流操作
V4L2设备 (真实相机硬件)
```

## 🧪 测试方法

### 1. 编译项目
```bash
cmake --build build -j4
```

### 2. 运行测试脚本
```bash
./test_unified_camera.sh
```

### 3. 手动测试步骤
1. 启动程序（无相机状态）
2. 插入单个相机（观察切换到单相机模式）
3. 插入第二个相机（观察切换到双相机模式）
4. 拔出一个相机（观察回到单相机模式）
5. 拔出所有相机（观察回到无相机模式）

### 4. 观察点
- Toast通知显示模式变化
- 日志输出显示详细状态
- 页面功能根据模式动态调整
- 相机读流自动启停

## 🔍 日志示例

```
[INFO] 初始化统一相机管理器
[INFO] 加载相机识别配置 - 左相机: cameral, left, video3, 1.3, ytxb.*1.3
[INFO] 加载相机识别配置 - 右相机: camerar, right, video1, 1.4.3, ytxb.*1.4.3
[INFO] 检测到新设备: /dev/video1 (YTXB: YTXB (usb-fc880000.usb-1.4.3))
[INFO] 识别为右相机: YTXB: YTXB (usb-fc880000.usb-1.4.3) (匹配规则: 1.4.3)
[INFO] 检测状态变化: 0 -> 2
[INFO] 操作模式变化: 0 -> 1
[INFO] 进入单相机模式
[INFO] 启动单相机读流: /dev/video1
```

## 🎛️ 优势特性

1. **无缝切换**: 三种模式间平滑过渡
2. **智能识别**: 基于名称和总线信息自动配对
3. **可配置**: 识别规则完全可配置
4. **健壮性**: 全面的错误处理和恢复机制
5. **性能优化**: 避免不必要的资源占用
6. **事件驱动**: 响应式架构，实时状态更新

## 🚀 后续扩展

系统设计为高度可扩展，未来可以轻松添加：

1. **更多相机类型**: 网络相机、文件相机等
2. **高级同步模式**: 更精确的时间同步
3. **相机参数管理**: 自动曝光、白平衡等
4. **性能监控**: 帧率、延迟统计
5. **用户配置界面**: 图形化配置工具

## 📝 注意事项

1. **权限要求**: 需要访问/dev/video*设备的权限
2. **依赖关系**: 需要V4L2库支持
3. **配置文件**: 确保config.toml配置正确
4. **设备识别**: 根据实际设备调整识别规则

---

**统一相机管理系统已完全集成并可投入使用！** 🎉