# 快速开始 - RKNN 推理示例

## 一键运行

```bash
# 在项目根目录下运行
cargo run --example yolov8_detect --release -p rknn-inference
```

就这么简单!程序会:
1. 加载 `models/yolov8m.rknn` 模型
2. 读取 `tests/test.jpg` 测试图片
3. 运行 YOLOv8 目标检测
4. 在原图上绘制检测结果
5. 保存到 `output_detection.jpg` 和 `output_detection.txt`

## 预期输出

### 控制台输出
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
Image loaded: XXXxYYY
✓ Image loaded successfully!

Step 3: Preprocessing image...
✓ Image preprocessed (letterbox + resize)

Step 4: Running inference...
✓ Inference completed in XX.XXms
  Detected N objects

Step 5: Post-processing and visualization...
Drawing N detections on image
  [0] <class_name>: XX.XX% at [x1,y1,x2,y2]
  ...
✓ Detections drawn on image

Step 6: Saving results...
✓ Output image saved: output_detection.jpg
✓ Text results saved: output_detection.txt

╔══════════════════════════════════════════════════════╗
║                       Summary                        ║
╚══════════════════════════════════════════════════════╝
  Input:       XXXxYYY pixels
  Model:       640x640 input
  Detections:  N objects found
  Inference:   XX.XXms
  Output:      output_detection.jpg (with bounding boxes)

✓ All done!
```

### 生成的文件

**output_detection.jpg**:
- 带有彩色边界框的图像
- 每个检测对象都有标签和置信度百分比
- 不同类别使用不同颜色

**output_detection.txt**:
```
YOLOv8 Detection Results
=========================
Total detections: N

Detection #0:
  Class: <class_name> (ID: X)
  Confidence: XX.XX%
  Bounding Box: [x1, y1, x2, y2]
  Size: WxH

...
```

## 使用自定义图像

```bash
# 使用你自己的图片
cargo run --example yolov8_detect --release -p rknn-inference -- \
    models/yolov8m.rknn \
    /path/to/your/image.jpg
```

支持的图像格式:
- JPEG (.jpg, .jpeg)
- PNG (.png)
- BMP (.bmp)
- 以及 `image` crate 支持的其他格式

## 文件结构

确保以下文件存在:
```
rustSmartScope/
├── models/
│   ├── yolov8m.rknn         # YOLOv8 RKNN 模型
│   └── coco_labels.txt       # COCO 类别标签
├── tests/
│   └── test.jpg              # 测试图片
└── crates/rknn-inference/
    └── lib/
        ├── librknnrt.so      # RKNN 运行时库
        └── librga.so         # RGA 图形加速库
```

## 常见问题

### 1. 模型文件未找到
```
Error: No such file or directory (os error 2)
```
**解决**: 确保 `models/yolov8m.rknn` 文件存在

### 2. 测试图片未找到
```
Error: No such file or directory (os error 2)
```
**解决**: 确保 `tests/test.jpg` 文件存在

### 3. 库文件未找到
```
error while loading shared libraries: librknnrt.so
```
**解决**: 设置 LD_LIBRARY_PATH
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/crates/rknn-inference/lib
cargo run --example yolov8_detect --release -p rknn-inference
```

或者创建一个运行脚本:
```bash
#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/crates/rknn-inference/lib
cargo run --example yolov8_detect --release -p rknn-inference "$@"
```

### 4. 标签文件未找到
```
Open models/coco_labels.txt fail!
```
**解决**: 确保 `models/coco_labels.txt` 文件存在

## 下一步

- 查看 [RUNNING.md](./RUNNING.md) 了解详细说明
- 查看 [README.md](./README.md) 了解技术细节
- 查看 [GETTING_STARTED.md](./GETTING_STARTED.md) 了解 API 使用

## 性能提示

- 首次运行会下载依赖,可能需要几分钟
- Release 模式比 Debug 模式快得多
- 使用 `--release` 标志以获得最佳性能
- RK3588 NPU 提供硬件加速
- 典型推理时间: 30-60ms

## 技术支持

遇到问题?
1. 检查所有文件路径是否正确
2. 确保 RKNN 库已正确安装
3. 查看详细文档
4. 提交 issue

祝使用愉快! 🚀
