# Smart Scope Qt - 项目结构

## 目录结构

```
smart_scope_qt/
├── CMakeLists.txt                 # 主CMake构建文件
├── .gitignore                     # Git忽略文件
├── README.md                      # 项目说明文档
├── LICENSE                        # 许可证文件
├── docs/                          # 文档目录
│   ├── architecture.md            # 架构文档
│   ├── user_manual.md             # 用户手册
│   └── developer_guide.md         # 开发者指南
├── src/                           # 源代码目录
│   ├── CMakeLists.txt             # 源代码CMake文件
│   ├── main.cpp                   # 程序入口
│   ├── infrastructure/            # 基础设施层
│   │   ├── config/                # 配置管理模块
│   │   │   ├── config_manager.h
│   │   │   └── config_manager.cpp
│   │   ├── logging/               # 日志模块
│   │   │   ├── logger.h
│   │   │   ├── logger.cpp
│   │   │   ├── log_formatter.h
│   │   │   ├── log_formatter.cpp
│   │   │   ├── log_appender.h
│   │   │   └── log_appender.cpp
│   │   └── exception/             # 异常处理模块
│   │       ├── app_exception.h
│   │       ├── app_exception.cpp
│   │       ├── camera_exception.h
│   │       └── camera_exception.cpp
│   ├── core/                      # 核心层
│   │   ├── camera/                # 相机模块
│   │   │   ├── camera.h           # 相机接口
│   │   │   ├── camera_manager.h
│   │   │   ├── camera_manager.cpp
│   │   │   ├── usb_camera.h
│   │   │   ├── usb_camera.cpp
│   │   │   ├── camera_calibration.h
│   │   │   ├── camera_calibration.cpp
│   │   │   ├── frame_buffer.h
│   │   │   └── frame_buffer.cpp
│   │   ├── camera_control/        # 相机参数调节模块
│   │   │   ├── camera_controller.h
│   │   │   ├── camera_controller.cpp
│   │   │   ├── parameter_preset.h
│   │   │   ├── parameter_preset.cpp
│   │   │   ├── parameter_history.h
│   │   │   └── parameter_history.cpp
│   │   ├── stereo/                # 立体视觉模块
│   │   │   ├── stereo_processor.h
│   │   │   ├── stereo_processor.cpp
│   │   │   ├── stereo_matcher.h
│   │   │   ├── bm_stereo_matcher.h
│   │   │   ├── bm_stereo_matcher.cpp
│   │   │   ├── sgbm_stereo_matcher.h
│   │   │   ├── sgbm_stereo_matcher.cpp
│   │   │   ├── neural_stereo_matcher.h
│   │   │   ├── neural_stereo_matcher.cpp
│   │   │   ├── disparity_filter.h
│   │   │   └── disparity_filter.cpp
│   │   ├── reconstruction/        # 3D重建模块
│   │   │   ├── point_cloud_generator.h
│   │   │   ├── point_cloud_generator.cpp
│   │   │   ├── point_cloud_filter.h
│   │   │   ├── point_cloud_filter.cpp
│   │   │   ├── surface_reconstructor.h
│   │   │   └── surface_reconstructor.cpp
│   │   └── inference/             # 推理模块
│   │       ├── inference_engine.h
│   │       ├── onnx_inference_engine.h
│   │       ├── onnx_inference_engine.cpp
│   │       ├── model_loader.h
│   │       └── model_loader.cpp
│   ├── app/                       # 应用层
│   │   ├── ui/                    # UI模块
│   │   │   ├── main_window.h
│   │   │   ├── main_window.cpp
│   │   │   ├── control_panel.h
│   │   │   ├── control_panel.cpp
│   │   │   ├── status_bar.h
│   │   │   ├── status_bar.cpp
│   │   │   ├── page_manager.h
│   │   │   └── page_manager.cpp
│   │   ├── visualization/         # 可视化模块
│   │   │   ├── disparity_visualizer.h
│   │   │   ├── disparity_visualizer.cpp
│   │   │   ├── point_cloud_visualizer.h
│   │   │   ├── point_cloud_visualizer.cpp
│   │   │   ├── depth_map_visualizer.h
│   │   │   └── depth_map_visualizer.cpp
│   │   ├── annotation/              # 图片注释模块
│   │   │   ├── annotation_manager.h
│   │   │   ├── annotation_manager.cpp
│   │   │   ├── text_annotation.h
│   │   │   ├── text_annotation.cpp
│   │   │   ├── arrow_annotation.h
│   │   │   ├── arrow_annotation.cpp
│   │   │   ├── highlight_annotation.h
│   │   │   ├── highlight_annotation.cpp
│   │   │   ├── measurement_annotation.h
│   │   │   └── measurement_annotation.cpp
│   │   ├── reporting/              # 报告生成模块
│   │   │   ├── report_generator.h
│   │   │   ├── report_generator.cpp
│   │   │   ├── report_template.h
│   │   │   ├── report_template.cpp
│   │   │   ├── report_item.h
│   │   │   ├── report_item.cpp
│   │   │   ├── report_exporter.h
│   │   │   └── report_exporter.cpp
│   │   ├── file_browser/           # 文件浏览模块
│   │   │   ├── file_browser_widget.h
│   │   │   ├── file_browser_widget.cpp
│   │   │   ├── thumbnail_generator.h
│   │   │   ├── thumbnail_generator.cpp
│   │   │   ├── file_metadata.h
│   │   │   ├── file_metadata.cpp
│   │   │   ├── file_operations.h
│   │   │   └── file_operations.cpp
│   │   └── interaction/           # 交互模块
│   │       ├── input_handler.h
│   │       ├── input_handler.cpp
│   │       ├── gesture_recognizer.h
│   │       └── gesture_recognizer.cpp
│   └── utils/                     # 工具层
│       ├── image_utils/           # 图像处理工具
│       │   ├── image_converter.h
│       │   ├── image_converter.cpp
│       │   ├── image_filter.h
│       │   └── image_filter.cpp
│       ├── math_utils/            # 数学工具
│       │   ├── geometry.h
│       │   ├── geometry.cpp
│       │   ├── transformation.h
│       │   └── transformation.cpp
│       └── concurrency_utils/     # 并发工具
│           ├── thread_pool.h
│           ├── thread_pool.cpp
│           ├── worker_thread.h
│           └── worker_thread.cpp
├── include/                       # 公共头文件目录
│   └── smart_scope/               # 公共头文件
│       ├── smart_scope.h          # 主头文件
│       ├── camera.h               # 相机接口
│       ├── stereo.h               # 立体视觉接口
│       └── reconstruction.h       # 重建接口
├── tests/                         # 测试目录
│   ├── CMakeLists.txt             # 测试CMake文件
│   ├── unit/                      # 单元测试
│   │   ├── camera_tests.cpp
│   │   ├── stereo_tests.cpp
│   │   └── reconstruction_tests.cpp
│   └── integration/               # 集成测试
│       ├── camera_stereo_tests.cpp
│       └── stereo_reconstruction_tests.cpp
├── resources/                     # 资源文件
│   ├── icons/                     # 图标
│   ├── shaders/                   # 着色器
│   ├── styles/                    # 样式表
│   └── models/                    # 模型文件
├── scripts/                       # 脚本文件
│   ├── build.sh                   # 构建脚本
│   ├── install_dependencies.sh    # 依赖安装脚本
│   └── camera_calibration.py      # 相机标定脚本
├── examples/                      # 示例代码
│   ├── basic_camera/              # 基本相机示例
│   ├── stereo_matching/           # 立体匹配示例
│   └── point_cloud/               # 点云示例
└── third_party/                   # 第三方库
    ├── toml11/                    # TOML解析库
    └── spdlog/                    # 日志库
└── data/                      # 数据模块
    ├── storage/               # 数据存储模块
    │   ├── data_storage.h
    │   ├── data_storage.cpp
    │   ├── image_storage.h
    │   ├── image_storage.cpp
    │   ├── video_storage.h
    │   ├── video_storage.cpp
    │   ├── point_cloud_storage.h
    │   └── point_cloud_storage.cpp
    └── data_io/               # 数据导入导出模块
        ├── data_importer.h
        ├── data_importer.cpp
        ├── data_exporter.h
        ├── data_exporter.cpp
        ├── format_converter.h
        └── format_converter.cpp
```

## 关键文件说明

### 1. 配置文件

项目使用TOML格式的配置文件：

```toml
# 应用配置
[app]
name = "Smart Scope Qt"
version = "1.0.0"
log_level = "info"
log_file = "logs/smart_scope.log"

# 相机配置
[camera]
width = 1280
height = 720
fps = 30

[camera.left]
name = ["cameraL", "Web Camera 2Ks"]
parameters_path = "camera_parameters/camera0_intrinsics.dat"
rot_trans_path = "camera_parameters/camera0_rot_trans.dat"

[camera.right]
name = ["cameraR", "USB Camera"]
parameters_path = "camera_parameters/camera1_intrinsics.dat"
rot_trans_path = "camera_parameters/camera1_rot_trans.dat"

# 相机参数调节配置
[camera_control]
auto_exposure = true
exposure_time = 10000  # 微秒
gain = 1.0
auto_white_balance = true
brightness = 0
contrast = 0
saturation = 0
presets_path = "presets/camera_presets.json"
history_size = 10

# 立体匹配配置
[stereo]
matcher = "sgbm"  # 可选: "bm", "sgbm", "neural"
min_disparity = 0
num_disparities = 128
block_size = 9
p1 = 200
p2 = 800
disp12_max_diff = 1
pre_filter_cap = 31
uniqueness_ratio = 15
speckle_window_size = 100
speckle_range = 32

# 神经网络模型配置
[neural]
model_path = "models/stereo_model.onnx"
provider = "cuda"  # 可选: "cpu", "cuda", "tensorrt"

# 点云配置
[point_cloud]
max_points = 1000000
filter_outliers = true
voxel_size = 0.01

# 注释配置
[annotation]
default_font = "Arial"
default_font_size = 12
default_color = "#FF0000"
arrow_width = 2
highlight_opacity = 0.3
save_with_image = true
annotations_path = "annotations/"

# 报告配置
[report]
templates_path = "templates/"
default_template = "standard_report.html"
company_name = "Smart Scope Inc."
company_logo = "resources/logo.png"
output_path = "reports/"
default_format = "pdf"  # 可选: "pdf", "html", "docx"

# 文件浏览配置
[file_browser]
root_path = "data/"
thumbnail_size = 128
cache_thumbnails = true
thumbnails_path = "cache/thumbnails/"
supported_formats = [".jpg", ".png", ".bmp", ".tiff", ".ply", ".obj", ".mp4", ".avi"]
```

### 2. CMake构建文件

主CMakeLists.txt文件：

```cmake
cmake_minimum_required(VERSION 3.14)
project(smart_scope_qt VERSION 1.0.0 LANGUAGES CXX)

# C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt设置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找依赖包
find_package(Qt5 COMPONENTS Core Gui Widgets DataVisualization REQUIRED)
find_package(OpenCV REQUIRED)
find_package(ONNXRuntime REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
)

# 添加子目录
add_subdirectory(src)
add_subdirectory(tests)

# 安装规则
install(DIRECTORY resources DESTINATION share/smart_scope_qt)
install(FILES LICENSE README.md DESTINATION share/doc/smart_scope_qt)
```

### 3. 主程序入口

main.cpp文件：

```cpp
#include <QApplication>
#include <QCommandLineParser>
#include "app/ui/main_window.h"
#include "infrastructure/config/config_manager.h"
#include "infrastructure/logging/logger.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Smart Scope Qt");
    app.setApplicationVersion("1.0.0");
    
    // 命令行参数解析
    QCommandLineParser parser;
    parser.setApplicationDescription("Smart Scope Qt - 双目相机3D重建系统");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption configOption(QStringList() << "c" << "config", 
                                   "指定配置文件路径", "config_file", "config.toml");
    parser.addOption(configOption);
    parser.process(app);
    
    // 初始化日志系统
    Logger::init();
    
    // 加载配置
    QString configPath = parser.value(configOption);
    if (!ConfigManager::instance().loadConfig(configPath)) {
        Logger::error("无法加载配置文件: {}", configPath.toStdString());
        return 1;
    }
    
    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
```

## 编译和构建

### 依赖项

- Qt 5.15或更高版本
- OpenCV 4.5或更高版本
- ONNX Runtime 1.12或更高版本
- CMake 3.14或更高版本
- C++17兼容编译器

### 构建步骤

1. 创建构建目录：
```bash
mkdir build
cd build
```

2. 配置项目：
```bash
cmake ..
```

3. 编译：
```bash
cmake --build . --config Release
```

4. 安装（可选）：
```bash
cmake --install . --prefix /usr/local
```

## 开发规范

1. **命名规范**：
   - 类名：PascalCase
   - 方法和函数：camelCase
   - 成员变量：m_camelCase
   - 常量：UPPER_CASE
   - 文件名：snake_case.h/cpp

2. **代码风格**：
   - 缩进：4个空格
   - 大括号：新行
   - 行宽：最大120字符

3. **文档规范**：
   - 所有公共API使用Doxygen风格注释
   - 复杂算法需要添加详细注释
   - 每个文件顶部添加版权和许可证信息

4. **版本控制**：
   - 功能分支：feature/feature-name
   - 修复分支：fix/bug-description
   - 发布分支：release/vX.Y.Z 