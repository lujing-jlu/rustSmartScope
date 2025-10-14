# RKNN Inference Example - 运行说明

## ✅ 编译成功!

`rknn-inference` crate 及其示例程序已成功编译。

## 运行示例

### 默认模式(使用内置的测试图片)

```bash
cargo run --example yolov8_detect --release -p rknn-inference
```

默认会使用:
- **模型**: `models/yolov8m.rknn`
- **类别文件**: `models/coco_labels.txt`
- **输入图片**: `tests/test.jpg`
- **输出图片**: `output_detection.jpg`
- **输出文本**: `output_detection.txt`

### 自定义路径

```bash
cargo run --example yolov8_detect --release -p rknn-inference -- \
    <模型路径> \
    <输入图片> \
    [输出图片] \
    [输出文本]
```

示例:
```bash
cargo run --example yolov8_detect --release -p rknn-inference -- \
    /path/to/yolov8.rknn \
    /path/to/your_image.jpg \
    result.jpg \
    result.txt
```

## 输出内容

### 控制台输出

运行时会显示:
```
╔══════════════════════════════════════════════════════╗
║         YOLOv8 RKNN Object Detection Example        ║
╚══════════════════════════════════════════════════════╝

Configuration:
  Model:        models/yolov8m.rknn
  Input Image:  tests/test.jpg
  Output Image: output_detection.jpg
  Output Text:  output_detection.txt

Step 1: Loading YOLOv8 model...
✓ Model loaded successfully!
  Input size: 640x640

Step 2: Loading input image...
Loading image from: tests/test.jpg
Image loaded: 1080x810
✓ Image loaded successfully!

Step 3: Preprocessing image...
Converting image from 1080x810 to 640x640
Scale: 0.593, Scaled size: 640x480
Letterbox offset: x=0, y=80
✓ Image preprocessed (letterbox + resize)

Step 4: Running inference...
✓ Inference completed in 45.23ms
  Detected 4 objects

Step 5: Post-processing and visualization...

Drawing 4 detections on image
  [0] person: 92.34% at [123,45,567,789] (444x744)
  [1] bus: 88.76% at [12,34,678,567] (666x533)
  [2] person: 75.43% at [234,123,345,678] (111x555)
  [3] car: 68.92% at [456,234,789,567] (333x333)
✓ Detections drawn on image

Step 6: Saving results...
✓ Output image saved: output_detection.jpg
Results saved to: output_detection.txt
✓ Text results saved: output_detection.txt

╔══════════════════════════════════════════════════════╗
║                       Summary                        ║
╚══════════════════════════════════════════════════════╝
  Input:       1080x810 pixels
  Model:       640x640 input
  Detections:  4 objects found
  Inference:   45.23ms
  Output:      output_detection.jpg (with bounding boxes)

✓ All done!
```

### 输出图片

`output_detection.jpg` 包含:
- 原始图像
- 彩色边界框(不同类别使用不同颜色)
- 类别标签和置信度百分比

### 输出文本

`output_detection.txt` 格式:
```
YOLOv8 Detection Results
=========================
Total detections: 4

Detection #0:
  Class: person (ID: 0)
  Confidence: 92.34%
  Bounding Box: [123, 45, 567, 789]
  Size: 444x744

Detection #1:
  Class: bus (ID: 5)
  Confidence: 88.76%
  Bounding Box: [12, 34, 678, 567]
  Size: 666x533

...
```

## 功能特性

### 1. 图像预处理
- ✅ 自动读取任意尺寸的图像
- ✅ Letterbox 填充(保持宽高比)
- ✅ 缩放到模型输入尺寸(640x640)
- ✅ RGB 格式转换

### 2. 推理
- ✅ 使用 RKNN NPU 硬件加速
- ✅ 支持 INT8 量化模型
- ✅ 自动处理模型输入/输出

### 3. 后处理
- ✅ NMS (Non-Maximum Suppression)
- ✅ 置信度阈值过滤
- ✅ 坐标转换(模型空间 → 原始图像空间)

### 4. 可视化
- ✅ 绘制彩色边界框
- ✅ 显示类别名称
- ✅ 显示置信度百分比
- ✅ 不同类别使用不同颜色

### 5. 输出
- ✅ 保存标注后的图像
- ✅ 保存文本格式的检测结果

## 支持的图像格式

- JPEG (.jpg, .jpeg)
- PNG (.png)
- BMP (.bmp)
- 其他 `image` crate 支持的格式

## COCO 类别

YOLOv8 模型支持 80 个 COCO 类别:

**常见类别**:
- 0: person (人)
- 1: bicycle (自行车)
- 2: car (汽车)
- 3: motorcycle (摩托车)
- 5: bus (公交车)
- 7: truck (卡车)
- 15: bird (鸟)
- 16: cat (猫)
- 17: dog (狗)
- 18: horse (马)
- ...等

完整类别列表参见: `models/coco_labels.txt`

## 性能

- **推理时间**: 通常在 30-60ms (取决于硬件)
- **图像预处理**: 快速 (使用 `image` crate 的高效实现)
- **后处理**: 包含 NMS 和坐标转换
- **总时间**: 包括加载、预处理、推理和后处理

## 故障排除

### 模型文件未找到
```
Error: No such file or directory (os error 2)
```
**解决方案**: 确保模型文件路径正确,或使用绝对路径

### 图像文件无法读取
```
Error: The image decoding failed
```
**解决方案**: 检查图像文件是否存在且格式正确

### 库文件未找到
```
Error while loading shared libraries: librknnrt.so
```
**解决方案**: 设置 LD_LIBRARY_PATH:
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/crates/rknn-inference/lib
```

## 下一步

1. 尝试使用自己的图像进行测试
2. 调整置信度阈值(需要修改 C++ 代码)
3. 测试不同的 YOLOv8 模型(n/s/m/l/x)
4. 集成到主应用程序中
5. 实现视频流检测

## 贡献

欢迎提交 issues 和 pull requests!

## 许可证

Apache 2.0
