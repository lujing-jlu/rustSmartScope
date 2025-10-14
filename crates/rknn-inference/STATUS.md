# RKNN Inference Crate - Current Status and Usage

## 当前状态

✅ **已完成**:
- Rust crate 结构创建
- C/C++ 源代码和头文件复制
- Rust FFI 绑定实现
- 类型系统和错误处理
- 完整的示例代码(包含图像加载、预处理、推理、可视化)
- 文档(README.md 和 GETTING_STARTED.md)

⚠️ **链接问题**:
当前遇到链接错误,C 函数无法被 C++ 代码正确链接。这是一个已知的 C/C++ 混合编译问题。

## 临时解决方案

由于链接问题,建议先使用参考代码的原始 C++ 实现进行测试。一旦解决链接问题,可以切换到 Rust 版本。

## 文件结构

```
crates/rknn-inference/
├── src/                    # Rust 源代码
│   ├── lib.rs             # 主 API
│   ├── ffi.rs             # C FFI 绑定
│   ├── types.rs           # 类型定义
│   └── error.rs           # 错误处理
├── csrc/                   # C/C++ 源代码
│   ├── yolov8.cc          # YOLOv8 推理
│   ├── postprocess.cc     # 后处理
│   ├── image_utils.c      # 图像工具
│   ├── file_utils.c       # 文件工具
│   └── rknn_wrapper.cpp   # C 包装器
├── include/                # 头文件
├── lib/                    # RKNN 库
│   ├── librknnrt.so
│   └── librga.so
├── examples/               # 示例代码
│   └── yolov8_detect.rs   # 完整检测示例
├── fonts/                  # 字体文件
└── README.md              # 文档

```

## 示例代码功能

`examples/yolov8_detect.rs` 包含完整的功能:

1. **图像加载**: 使用 `image` crate 加载任意格式图像
2. **预处理**:
   - Letterbox 填充(保持宽高比)
   - 缩放到模型输入尺寸
   - RGB 格式转换
3. **推理**: 调用 RKNN 模型进行目标检测
4. **后处理**:
   - 坐标转换(从模型空间到原始图像空间)
   - 边界框绘制
   - 类别标签绘制
5. **输出**:
   - 保存带标注的图像
   - 保存文本格式的检测结果

## 使用方法

### 默认模式(使用内置路径)

```bash
cargo run --example yolov8_detect --release -p rknn-inference
```

默认会使用:
- 模型: `reference_code/SmartScope/yolov8_rknn_inference/model/yolov8m.rknn`
- 图片: `reference_code/SmartScope/yolov8_rknn_inference/model/bus.jpg`
- 输出图片: `output_detection.jpg`
- 输出文本: `output_detection.txt`

### 自定义路径

```bash
cargo run --example yolov8_detect --release -p rknn-inference -- \
    /path/to/model.rknn \
    /path/to/image.jpg \
    /path/to/output.jpg \
    /path/to/output.txt
```

## API 使用示例

```rust
use rknn_inference::{ImageBuffer, Yolov8Detector, get_class_name};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 1. 加载模型
    let mut detector = Yolov8Detector::new("model.rknn")?;

    // 2. 获取模型尺寸
    let (width, height) = detector.model_size();

    // 3. 准备图像(RGB888 格式)
    let image = ImageBuffer::from_rgb888(width as i32, height as i32, rgb_data);

    // 4. 运行推理
    let detections = detector.detect(&image)?;

    // 5. 处理结果
    for det in detections {
        let class_name = get_class_name(det.class_id).unwrap_or_default();
        println!(
            "Class: {}, Confidence: {:.2}%, BBox: [{},{},{},{}]",
            class_name,
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

## 解决链接问题的步骤

1. **检查 build.rs**: 确保所有 C/C++ 文件都被编译
2. **C/C++ 混合编译**: C 函数需要 `extern "C"` 声明
3. **链接顺序**: 确保链接器能找到所有符号
4. **测试简化版本**: 先测试不依赖 C 函数的 wrapper

## 替代方案

如果链接问题难以解决,可以考虑:

1. **纯 C 实现**: 将所有代码改为 C 语言
2. **动态链接**: 先编译 C++ 为共享库,然后 Rust 链接共享库
3. **使用现有二进制**: 通过进程调用参考代码的可执行文件

## 下一步

1. 解决 C/C++ 混合编译的链接问题
2. 测试完整的推理流程
3. 性能优化
4. 添加更多示例
5. 集成到主项目中

## 技术细节

### C/C++ 混合编译问题

问题根源是 `file_utils.c` 中的 C 函数被 `yolov8.cc` 中的 C++ 代码调用时,链接器找不到符号。这是因为:

- C 编译器生成的符号名: `read_data_from_file`
- C++ 编译器期望的符号名: `_Z19read_data_from_filePKcPPc` (name mangling)

即使 `file_utils.h` 有 `extern "C"` 包装,但 `file_utils.c` 本身是用 C 编译的,生成的符号是 C 风格的。

### 可能的解决方案

1. **将 file_utils.c 重命名为 file_utils.cpp**
2. **使用 cc crate 的 `.cpp_link_stdlib()`**
3. **手动创建 C wrapper**
4. **使用 cbindgen 生成绑定**

## 联系和支持

如果遇到问题,请检查:
- RKNN 库是否正确安装
- 模型文件路径是否正确
- 图像文件是否可访问
- 编译输出中的详细错误信息

## 参考

- [RKNN Toolkit Documentation](https://github.com/rockchip-linux/rknn-toolkit2)
- [YOLOv8 Documentation](https://docs.ultralytics.com/)
- [Rust FFI Guide](https://doc.rust-lang.org/nomicon/ffi.html)
