# Smart Scope Qt - 模块化双目相机3D重建系统

基于Qt和OpenCV的模块化、面向对象的双目相机3D重建系统，实现实时视差图计算和3D点云重建可视化。

## 系统架构

Smart Scope Qt采用模块化、分层架构设计，遵循SOLID原则，主要分为以下几层：

1. **基础设施层** - 提供底层功能支持
2. **核心层** - 实现业务核心功能
3. **应用层** - 提供用户界面和交互
4. **工具层** - 提供通用工具和辅助功能

## 模块划分

### 1. 基础设施模块 (Infrastructure)

#### 1.1 配置管理模块 (Configuration)
- **职责**：管理系统配置，提供配置读写接口
- **主要类**：
  - `ConfigManager` - 配置管理器，单例模式
  - `ConfigLoader` - 配置加载器，支持多种格式(TOML/JSON/YAML)
  - `ConfigValidator` - 配置验证器

#### 1.2 日志模块 (Logging)
- **职责**：提供统一的日志记录功能
- **主要类**：
  - `Logger` - 日志记录器
  - `LogFormatter` - 日志格式化器
  - `LogAppender` - 日志输出目标(控制台/文件/网络)

#### 1.3 异常处理模块 (Exception)
- **职责**：统一的异常处理机制
- **主要类**：
  - `AppException` - 应用异常基类
  - `ConfigException` - 配置异常
  - `CameraException` - 相机异常
  - `ProcessingException` - 处理异常

### 2. 核心模块 (Core)

#### 2.1 相机模块 (Camera)
- **职责**：USB相机设备管理、图像采集
- **主要类**：
  - `CameraManager` - 相机管理器，单例模式，负责管理所有相机设备
  - `Camera` - 相机抽象基类，定义相机的通用接口
  - `USBCamera` - USB相机实现，基于V4L2接口
  - `FrameBuffer` - 帧缓冲区，用于存储采集的图像帧
  - `CaptureWorker` - 图像采集工作线程
  - `DecodeWorker` - 图像解码工作线程
  - `CameraException` - 相机异常类，用于处理相机操作中的异常
- **主要功能**：
  - 相机设备发现和枚举
  - 相机连接和断开
  - 相机参数设置和获取
  - 图像采集和处理
  - 左右相机管理（用于立体视觉）
  - 相机状态监控和错误处理
  - 相机异常恢复

#### 2.2 立体视觉模块 (Stereo)
- **职责**：立体匹配、视差计算、深度估计
- **主要类**：
  - `StereoProcessor` - 立体处理器
  - `StereoMatcher` - 立体匹配器接口
  - `BMStereoMatcher` - BM算法实现
  - `SGBMStereoMatcher` - SGBM算法实现
  - `NeuralStereoMatcher` - 神经网络立体匹配
  - `DisparityFilter` - 视差过滤器
  - `DisparityPostProcessor` - 视差后处理器

#### 2.3 3D重建模块 (Reconstruction)
- **职责**：点云生成、3D模型重建
- **主要类**：
  - `PointCloudGenerator` - 点云生成器
  - `PointCloudFilter` - 点云过滤器
  - `SurfaceReconstructor` - 表面重建器
  - `MeshGenerator` - 网格生成器

#### 2.4 推理模块 (Inference)
- **职责**：深度学习模型推理
- **主要类**：
  - `InferenceEngine` - 推理引擎接口
  - `ONNXInferenceEngine` - ONNX推理引擎
  - `ModelLoader` - 模型加载器
  - `InferenceSession` - 推理会话

### 3. 应用模块 (Application)

#### 3.1 UI模块 (UI)
- **职责**：用户界面
- **主要类**：
  - `MainWindow` - 主窗口
  - `ControlPanel` - 控制面板
  - `StatusBar` - 状态栏
  - `PageManager` - 页面管理器

#### 3.2 可视化模块 (Visualization)
- **职责**：数据可视化
- **主要类**：
  - `DisparityVisualizer` - 视差图可视化器
  - `PointCloudVisualizer` - 点云可视化器
  - `DepthMapVisualizer` - 深度图可视化器
  - `ColorMapper` - 颜色映射器

#### 3.3 交互模块 (Interaction)
- **职责**：用户交互
- **主要类**：
  - `InputHandler` - 输入处理器
  - `GestureRecognizer` - 手势识别器
  - `CommandManager` - 命令管理器

#### 3.4 图片注释模块 (Annotation)
- **职责**：为图像添加注释和标记
- **主要类**：
  - `AnnotationManager` - 注释管理器
  - `TextAnnotation` - 文本注释
  - `ArrowAnnotation` - 箭头标记
  - `HighlightAnnotation` - 区域高亮
  - `MeasurementAnnotation` - 测量标记

#### 3.5 报告生成模块 (Reporting)
- **职责**：生成分析报告
- **主要类**：
  - `ReportGenerator` - 报告生成器
  - `ReportTemplate` - 报告模板
  - `ReportItem` - 报告项目
  - `ReportExporter` - 报告导出器

#### 3.6 文件浏览模块 (FileBrowser)
- **职责**：管理和浏览保存的图像和视频文件
- **主要类**：
  - `FileBrowserWidget` - 文件浏览器控件
  - `ThumbnailGenerator` - 缩略图生成器
  - `FileMetadata` - 文件元数据
  - `FileOperations` - 文件操作

### 4. 工具模块 (Utils)

#### 4.1 图像处理工具 (ImageUtils)
- **职责**：图像处理通用功能
- **主要类**：
  - `ImageConverter` - 图像格式转换
  - `ImageFilter` - 图像滤波器
  - `ImageEnhancer` - 图像增强器

#### 4.2 数学工具 (MathUtils)
- **职责**：数学计算
- **主要类**：
  - `Geometry` - 几何计算
  - `Transformation` - 坐标变换
  - `Statistics` - 统计计算

#### 4.3 并发工具 (ConcurrencyUtils)
- **职责**：多线程和并发
- **主要类**：
  - `ThreadPool` - 线程池
  - `WorkerThread` - 工作线程
  - `TaskQueue`# SmartScope
