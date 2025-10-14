# Getting Started with RKNN Inference

这个快速入门指南将帮助你开始使用 `rknn-inference` crate 进行 YOLOv8 目标检测。

## 概述

`rknn-inference` 是一个 Rust crate,提供了对 RKNN (Rockchip Neural Network) 推理引擎的封装,专为 RK3588 NPU 硬件优化。它使用 C FFI 来调用底层的 RKNN API,并提供了一个安全、易用的 Rust 接口。

## 项目结构

```
crates/rknn-inference/
├── src/
│   ├── lib.rs          # 主库接口,提供 Yolov8Detector
│   ├── ffi.rs          # C FFI 绑定
│   ├── types.rs        # Rust 类型定义
│   └── error.rs        # 错误处理
├── csrc/               # C/C++ 源代码(从参考代码复制)
│   ├── yolov8.cc       # YOLOv8 模型初始化和推理
│   ├── postprocess.cc  # 后处理(NMS、解码等)
│   ├── image_utils.c   # 图像预处理工具
│   └── file_utils.c    # 文件 I/O 工具
├── include/            # C/C++ 头文件
│   ├── yolov8.h
│   ├── postprocess.h
│   ├── utils/          # 工具头文件
│   └── rga/            # RGA 硬件加速头文件
├── lib/                # 预编译的库文件
│   ├── librknnrt.so    # RKNN 运行时库
│   └── librga.so       # RGA 图像处理库
├── examples/           # 示例程序
│   └── yolov8_detect.rs
├── build.rs            # 构建脚本
├── Cargo.toml
├── README.md
└── GETTING_STARTED.md  # 本文件
```

## 快速开始

### 1. 准备模型文件

你需要一个 RKNN 格式的 YOLOv8 模型文件(`.rknn`)。通常这个文件是通过 RKNN-Toolkit 从 ONNX 或 PyTorch 模型转换而来。

示例模型路径:
```
reference_code/SmartScope/yolov8_rknn_inference/model/yolov8.rknn
```

### 2. 准备测试图片

准备一张用于测试的 RGB 图片。注意当前的示例代码需要你实现图片加载逻辑,可以使用 `image` crate:

```toml
[dependencies]
image = "0.25"
```

### 3. 使用示例

#### 基础用法

```rust
use rknn_inference::{ImageBuffer, ImageFormat, Yolov8Detector};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 1. 加载模型
    let mut detector = Yolov8Detector::new("path/to/model.rknn")?;

    // 2. 获取模型输入尺寸
    let (width, height) = detector.model_size();
    println!("Model size: {}x{}", width, height);

    // 3. 准备图像数据(RGB888 格式)
    let image = ImageBuffer::from_rgb888(
        width as i32,
        height as i32,
        rgb_data_vec
    );

    // 4. 运行推理
    let detections = detector.detect(&image)?;

    // 5. 处理检测结果
    for det in detections {
        println!(
            "Class: {}, Confidence: {:.2}%, BBox: [{},{},{},{}]",
            det.class_id,
            det.confidence * 100.0,
            det.bbox.left,
            det.bbox.top,
            det.bbox.right,
            det.bbox.bottom
        );
    }

    Ok(())
}
```

#### 运行示例程序

```bash
# 编译示例
cargo build --example yolov8_detect --release -p rknn-inference

# 运行示例
cargo run --example yolov8_detect --release -p rknn-inference -- \
    /path/to/model.rknn \
    /path/to/image.jpg \
    output_results.txt
```

### 4. 集成到你的项目

在你的 `Cargo.toml` 中添加依赖:

```toml
[dependencies]
rknn-inference = { path = "../crates/rknn-inference" }
# 或者如果发布到 crates.io:
# rknn-inference = "0.1.0"
```

## 图像预处理

当前的实现中,C++ 层会自动处理以下预处理步骤:

1. **Letterbox 填充**: 保持宽高比,添加灰色边框
2. **缩放**: 调整到模型输入尺寸
3. **格式转换**: 确保 RGB888 格式

你只需要提供原始的 RGB 图像数据即可。

### 使用 `image` crate 加载图片

```rust
use image::io::Reader as ImageReader;
use rknn_inference::{ImageBuffer, ImageFormat};

fn load_image_from_file(path: &str) -> Result<ImageBuffer, Box<dyn std::error::Error>> {
    let img = ImageReader::open(path)?.decode()?;
    let rgb = img.to_rgb8();

    let (width, height) = rgb.dimensions();
    let data = rgb.into_raw();

    Ok(ImageBuffer::from_rgb888(
        width as i32,
        height as i32,
        data
    ))
}
```

## API 参考

### `Yolov8Detector`

主要的检测器结构体。

```rust
impl Yolov8Detector {
    /// 从模型文件创建检测器
    pub fn new<P: AsRef<Path>>(model_path: P) -> Result<Self>;

    /// 对图像运行推理
    pub fn detect(&mut self, image: &ImageBuffer) -> Result<Vec<DetectionResult>>;

    /// 获取模型输入尺寸
    pub fn model_size(&self) -> (u32, u32);
}
```

### `ImageBuffer`

图像数据容器。

```rust
pub struct ImageBuffer {
    pub width: i32,
    pub height: i32,
    pub format: ImageFormat,
    pub data: Vec<u8>,
}

impl ImageBuffer {
    pub fn from_rgb888(width: i32, height: i32, data: Vec<u8>) -> Self;
}
```

### `DetectionResult`

检测结果。

```rust
pub struct DetectionResult {
    pub bbox: ImageRect,      // 边界框
    pub confidence: f32,       // 置信度 (0.0-1.0)
    pub class_id: i32,         // COCO 类别 ID
}
```

### `ImageRect`

边界框表示。

```rust
pub struct ImageRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl ImageRect {
    pub fn width(&self) -> i32;
    pub fn height(&self) -> i32;
}
```

### 辅助函数

```rust
/// 根据类别 ID 获取 COCO 类别名称
pub fn get_class_name(class_id: i32) -> Option<String>;
```

## COCO 类别

YOLOv8 使用 COCO 数据集的 80 个类别。类别 ID 范围从 0-79。

常见类别示例:
- 0: person (人)
- 1: bicycle (自行车)
- 2: car (汽车)
- 3: motorcycle (摩托车)
- 5: bus (公交车)
- 7: truck (卡车)
- 16: dog (狗)
- 17: cat (猫)
- ... (共 80 个类别)

完整列表请参考 `model/coco_80_labels_list.txt`。

## 性能优化建议

1. **使用 INT8 量化模型**: 在转换 RKNN 模型时启用量化可以显著提升性能
2. **预分配缓冲区**: 对于连续推理,可以重用图像缓冲区
3. **批处理**: 当前不支持批量推理,但可以在应用层面实现异步处理
4. **图像尺寸对齐**: 确保图像宽度是 16 的倍数(RK3588)或 4 的倍数(RV1106)

## 常见问题

### Q: 如何转换 YOLOv8 模型到 RKNN 格式?

A: 使用 RKNN-Toolkit2:

```python
from rknn.api import RKNN

rknn = RKNN()
rknn.config(target_platform='rk3588')
rknn.load_onnx(model='yolov8.onnx')
rknn.build(do_quantization=True)
rknn.export_rknn('yolov8.rknn')
```

### Q: 为什么检测结果为空?

A: 检查以下几点:
1. 模型文件是否正确加载
2. 输入图像格式是否为 RGB888
3. 置信度阈值是否太高(默认 0.25)
4. 图像内容是否包含可检测的对象

### Q: 如何调整置信度阈值?

A: 目前阈值硬编码在 C++ 代码中。你可以修改 `csrc/postprocess.cc` 中的 `BOX_THRESH` 和 `NMS_THRESH` 常量,然后重新编译。

### Q: 支持哪些图像格式?

A: 当前主要支持 RGB888 格式。其他格式(如 RGBA8888, YUV420SP)已在代码中定义,但可能需要额外测试。

## 下一步

- 查看 `examples/yolov8_detect.rs` 了解完整示例
- 阅读 `README.md` 了解更多架构细节
- 浏览 `src/` 目录了解实现细节
- 参考参考代码了解 C++ 实现: `reference_code/SmartScope/yolov8_rknn_inference/`

## 贡献

欢迎提交 issues 和 pull requests!

## 许可证

本项目基于 Apache 2.0 许可证。
