# USB Camera C++ Interface

这是一个基于Rust实现的USB相机管理库，提供了C++接口。该库支持自动相机模式检测、多线程并行处理，并通过回调机制高效地推送帧数据给C++应用。

## 特性

- 🎥 **自动相机模式检测**: 自动识别无相机、单相机、立体相机模式
- 🚀 **多线程架构**: 每个相机在独立线程中运行，最大化FPS性能
- 📡 **回调机制**: C++端只需注册回调函数，Rust端负责主动推送数据
- 🔄 **动态模式切换**: 支持运行时相机插拔，自动调整工作模式
- 📊 **高性能**: 基于V4L2直接访问和零拷贝技术
- 🛡️ **类型安全**: 完整的C接口封装，支持.so和.a库

## 架构设计

基于成功的多线程示例 `threaded_adaptive_stream_example.rs`，实现了：

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   C++ 应用      │    │  Rust 相机管理   │    │   硬件相机      │
│                 │    │                  │    │                 │
│ 注册回调函数 ────┼───►│ 独立线程架构     │◄───┼─ cameraL       │
│                 │    │                  │    │                 │
│ 接收帧数据   ◄───┼────│ 主动推送数据     │◄───┼─ cameraR       │
│                 │    │                  │    │                 │
│ 处理业务逻辑    │    │ 自动模式切换     │    │ V4L2设备        │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

## 快速开始

### 1. 构建Rust库

```bash
# 构建发布版本
cargo build --release

# 检查构建是否成功
ls target/release/libusb_camera.*
```

### 2. 构建C++示例

使用Makefile（推荐）：
```bash
# 构建C++示例
make

# 构建并运行
make run

# 快速测试（10秒）
make test
```

或使用CMake：
```bash
mkdir build && cd build
cmake ..
make
```

### 3. 运行示例

```bash
# 确保有USB相机连接
./cpp_callback_test
```

## C++ API 使用示例

### 基础用法

```cpp
#include "usb_camera.h"

// 回调函数
void on_frame_received(const CFrameData* frame, void* user_data) {
    printf("收到帧: %dx%d, %d KB\n", 
           frame->width, frame->height, frame->size / 1024);
}

int main() {
    // 创建相机管理器
    CameraStreamHandle handle = camera_stream_create();
    
    // 注册回调
    camera_stream_register_left_callback(handle, on_frame_received, nullptr);
    camera_stream_register_right_callback(handle, on_frame_received, nullptr);
    
    // 启动流
    camera_stream_start(handle);
    
    // 等待并处理帧数据（通过回调自动接收）
    sleep(10);
    
    // 停止并清理
    camera_stream_stop(handle);
    camera_stream_destroy(handle);
    
    return 0;
}
```

### 完整回调示例

```cpp
struct CameraContext {
    std::string name;
    std::atomic<int> frame_count{0};
};

void camera_callback(const CFrameData* frame, void* user_data) {
    CameraContext* ctx = static_cast<CameraContext*>(user_data);
    ctx->frame_count++;
    
    // 处理帧数据
    // 注意：frame->data指向的内存在回调结束后可能失效
    // 如需保存，请立即复制数据
    std::vector<uint8_t> frame_copy(frame->data, frame->data + frame->size);
    
    // 你的图像处理逻辑...
}

int main() {
    CameraContext left_ctx{"LEFT"};
    CameraContext right_ctx{"RIGHT"};
    
    CameraStreamHandle handle = camera_stream_create();
    
    // 注册不同相机的回调
    camera_stream_register_left_callback(handle, camera_callback, &left_ctx);
    camera_stream_register_right_callback(handle, camera_callback, &right_ctx);
    
    camera_stream_start(handle);
    
    // 监控统计
    while (running) {
        printf("左相机: %d 帧, 右相机: %d 帧\n", 
               left_ctx.frame_count.load(), 
               right_ctx.frame_count.load());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    camera_stream_stop(handle);
    camera_stream_destroy(handle);
    return 0;
}
```

## API 参考

### 核心函数

- `camera_stream_create()`: 创建相机管理器实例
- `camera_stream_start()`: 启动相机流管理
- `camera_stream_stop()`: 停止相机流管理  
- `camera_stream_destroy()`: 销毁管理器实例

### 回调注册

- `camera_stream_register_left_callback()`: 注册左相机回调
- `camera_stream_register_right_callback()`: 注册右相机回调
- `camera_stream_register_single_callback()`: 注册单相机回调

### 状态查询

- `camera_stream_get_mode()`: 获取当前相机模式
- `camera_stream_is_running()`: 检查管理器是否运行
- `camera_stream_update_mode()`: 强制更新模式检测

### 数据结构

```c
typedef struct {
    const uint8_t* data;        // 帧数据指针
    uint32_t size;              // 数据大小
    uint32_t width;             // 帧宽度
    uint32_t height;            // 帧高度
    uint32_t format;            // 像素格式
    uint64_t frame_id;          // 帧ID
    int32_t camera_type;        // 相机类型 (0=左, 1=右)
    uint64_t timestamp_ms;      // 时间戳
} CFrameData;
```

## 相机模式

- `CAMERA_MODE_NO_CAMERA (0)`: 无相机连接
- `CAMERA_MODE_SINGLE_CAMERA (1)`: 单相机模式
- `CAMERA_MODE_STEREO_CAMERA (2)`: 立体相机模式（左右双相机）

## 性能特点

- **独立线程**: 每个相机在独立线程中运行，避免相互干扰
- **最大FPS**: 基于测试，双相机可达到24.3 FPS的正确性能
- **零拷贝**: 帧数据直接从V4L2内存映射传递到回调
- **低延迟**: 回调机制避免了轮询开销

## 注意事项

1. **帧数据生命周期**: 回调函数中的`frame->data`指针仅在回调期间有效
2. **线程安全**: 回调函数在Rust线程中执行，注意C++端的线程安全
3. **相机命名**: 系统会自动识别`cameraL`/`cameraR`或按设备路径分配
4. **权限要求**: 需要访问`/dev/video*`设备的权限

## 故障排除

### 编译问题

```bash
# 安装依赖
sudo apt-get install build-essential pkg-config libv4l-dev v4l-utils

# 重新构建
make clean
make rebuild
```

### 运行时问题

```bash
# 检查相机设备
v4l2-ctl --list-devices

# 检查权限
ls -la /dev/video*

# 添加用户到video组
sudo usermod -a -G video $USER
```

### 调试信息

设置环境变量启用详细日志：
```bash
export RUST_LOG=debug
./cpp_callback_test
```

## 系统需求

- Linux系统（支持V4L2）
- USB相机设备
- C++14或更高版本
- Rust 1.70+（构建时）

## 许可证

与主项目SmartScope保持一致。