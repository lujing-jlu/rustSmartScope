# RustSmartScope 架构设计

## 整体架构概述

RustSmartScope 采用 **Rust + C++ + QML** 混合架构，充分发挥各语言的优势：

```
┌─────────────────────────────────────────────────────────┐
│                    QML Frontend                         │
│        (用户界面 + 页面路由 + 交互逻辑)                   │
└─────────────────┬───────────────────────────────────────┘
                  │ Qt Signals/Slots
┌─────────────────▼───────────────────────────────────────┐
│                 C++ Bridge Layer                       │
│         (Qt集成 + QML桥接 + 系统集成)                    │
└─────────────────┬───────────────────────────────────────┘
                  │ C FFI
┌─────────────────▼───────────────────────────────────────┐
│                  Rust Core Engine                      │
│    (业务逻辑 + 算法处理 + 硬件控制 + 数据管理)             │
└─────────────────────────────────────────────────────────┘
```

## 功能分工详解

### 🦀 Rust 核心引擎 (smartscope-core)

**职责范围：**
- **相机硬件控制** - V4L2设备管理，双相机同步捕获
- **图像处理算法** - OpenCV图像处理，立体视觉算法
- **AI推理引擎** - RKNN模型加载，YOLOv8目标检测
- **3D测量计算** - 立体匹配，深度图生成，点云处理
- **配置管理** - TOML配置文件读写，参数验证
- **数据持久化** - 测量结果存储，报告生成
- **错误处理** - 统一错误类型，日志记录

**技术栈：**
- `opencv` - 计算机视觉
- `v4l2` - Linux相机控制
- `rknn-runtime` - AI推理加速
- `serde` + `toml` - 配置序列化
- `thiserror` - 错误处理
- `log` - 日志系统

**示例功能：**
```rust
// 相机控制
pub fn capture_stereo_images() -> Result<(Mat, Mat), CameraError>

// AI检测
pub fn detect_objects(image: &Mat) -> Result<Vec<Detection>, AIError>

// 3D测量
pub fn calculate_depth_map(left: &Mat, right: &Mat) -> Result<Mat, StereoError>

// 配置管理
pub fn load_config(path: &str) -> Result<AppConfig, ConfigError>
```

### 🔗 C FFI 桥接层 (c-ffi)

**职责范围：**
- **C兼容接口** - 将Rust函数暴露为C函数
- **内存管理** - 跨语言内存安全
- **错误码转换** - Rust错误到C错误码映射
- **数据类型转换** - 结构体和数组在C/Rust间传递

**示例接口：**
```rust
#[no_mangle]
pub extern "C" fn smartscope_init() -> i32

#[no_mangle]
pub extern "C" fn smartscope_capture_image(
    camera_id: i32,
    width: *mut i32,
    height: *mut i32,
    data: *mut *mut u8
) -> i32

#[no_mangle]
pub extern "C" fn smartscope_detect_objects(
    image_data: *const u8,
    width: i32,
    height: i32,
    results: *mut DetectionResult,
    count: *mut i32
) -> i32
```

### 🎯 C++ 桥接层 (src/main.cpp)

**职责范围：**
- **Qt应用框架** - QApplication生命周期管理
- **QML引擎集成** - QQmlApplicationEngine，QML组件注册
- **信号槽机制** - Qt信号槽与QML事件绑定
- **系统集成** - 操作系统特定功能，窗口管理
- **资源管理** - QRC资源系统，图标字体加载

**关键组件：**
```cpp
class SmartScopeController : public QObject {
    Q_OBJECT
public slots:
    void captureImage();
    void startAIDetection(bool enabled);
    void performMeasurement(const QVariantMap& params);

signals:
    void imageReady(const QString& imagePath);
    void detectionComplete(const QVariantList& results);
    void measurementFinished(const QVariantMap& data);
};
```

### 🎨 QML 前端 (qml/)

**职责范围：**
- **用户界面** - 响应式布局，现代化设计
- **页面路由** - StackView页面切换，导航管理
- **用户交互** - 触摸手势，鼠标键盘事件
- **动画效果** - 页面转场，按钮反馈动画
- **状态管理** - UI状态同步，数据绑定

**页面结构：**
```
qml/
├── main.qml              # 主窗口 + 路由管理
├── pages/
│   ├── HomePage.qml      # 主页 - 相机预览
│   ├── PreviewPage.qml   # 预览页 - 图像浏览
│   ├── MeasurementPage.qml # 3D测量页
│   ├── ReportPage.qml    # 报告页 - 结果展示
│   └── SettingsPage.qml  # 设置页 - 参数配置
└── components/
    ├── CameraPreview.qml # 相机预览组件
    └── StatusIndicator.qml # 状态指示器
```

## 数据流向

### 1. 相机捕获流程
```
QML按钮点击 → C++信号 → Rust相机控制 → 硬件V4L2 →
图像数据 → Rust处理 → C FFI → C++ → QML显示
```

### 2. AI检测流程
```
QML启用开关 → C++配置 → Rust AI初始化 → RKNN模型加载 →
图像输入 → AI推理 → 检测结果 → C FFI → C++ → QML可视化
```

### 3. 3D测量流程
```
QML测量请求 → C++参数 → Rust立体匹配 → OpenCV处理 →
深度计算 → 3D坐标 → 测量结果 → C FFI → C++ → QML报告
```

## 构建系统集成

### Cargo Workspace (Rust)
```toml
[workspace]
members = ["crates/smartscope-core", "crates/c-ffi"]

[dependencies]
opencv = "0.90"
v4l2 = "0.14"
rknn-runtime = "1.6"
serde = { version = "1.0", features = ["derive"] }
toml = "0.8"
thiserror = "1.0"
```

### CMake 集成 (C++)
```cmake
# 链接Rust静态库
find_library(SMARTSCOPE_LIB
    NAMES smartscope
    PATHS ${CMAKE_SOURCE_DIR}/target/release)

target_link_libraries(rustsmartscope
    ${SMARTSCOPE_LIB}
    Qt5::Core Qt5::Widgets Qt5::Qml Qt5::Quick)
```

## 性能优化策略

1. **Rust并发** - tokio异步运行时，rayon并行计算
2. **内存零拷贝** - 跨语言内存映射，避免数据复制
3. **硬件加速** - RKNN专用芯片，OpenCV SIMD优化
4. **缓存机制** - 图像缓存，AI模型预加载
5. **QML优化** - 异步加载，组件复用

## 错误处理策略

### Rust层面
```rust
#[derive(thiserror::Error, Debug)]
pub enum SmartScopeError {
    #[error("Camera error: {0}")]
    Camera(#[from] CameraError),
    #[error("AI inference error: {0}")]
    AI(#[from] AIError),
    #[error("Configuration error: {0}")]
    Config(#[from] ConfigError),
}
```

### C FFI层面
```rust
#[repr(C)]
pub enum ErrorCode {
    Success = 0,
    CameraNotFound = -1001,
    AIModelLoadFailed = -2001,
    ConfigParseError = -3001,
}
```

### C++层面
```cpp
class SmartScopeException : public std::exception {
    ErrorCode errorCode;
    std::string message;
public:
    const char* what() const noexcept override;
    ErrorCode getErrorCode() const { return errorCode; }
};
```

## 开发工作流

1. **Rust开发** - `cargo build --release -p smartscope-c-ffi`
2. **C++集成** - CMake自动链接Rust库
3. **QML调试** - 热重载，实时预览
4. **集成测试** - `./build_and_run.sh`
5. **性能分析** - Rust profiler + Qt Creator分析器

这种架构设计确保了：
- **性能** - Rust处理核心算法，C++处理UI集成
- **安全** - Rust内存安全，类型安全的业务逻辑
- **可维护** - 清晰的层次分离，单一职责原则
- **跨平台** - Qt跨平台能力，Rust跨架构支持