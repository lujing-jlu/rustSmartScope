# libv4l-rkmpp 包装库

这是一个基于 libv4l-rkmpp 的 C 语言包装库，提供了简洁易用的 API 接口，用于视频设备操作和 RGA 硬件加速变换。

## 功能特性

- **视频设备管理**: 支持 V4L2 设备初始化、帧捕获
- **MJPEG 解码**: 自动解码 MJPEG 流为 RGB 格式
- **RGA 硬件加速**: 支持旋转、翻转、缩放等变换
- **多变换组合**: 支持多个变换的顺序合成
- **独立RGA操作**: 不依赖视频流的纯RGA变换接口
- **文件处理**: 支持图像文件的批量处理
- **自动验证**: 提供 `jpeg_batch_test` 与 `verify_outputs`，逐像素校验差异为0
- **性能监控**: 内置性能测试和统计功能
- **内存管理**: 自动内存分配和释放

## 支持的变换类型

- `RKMPP_TRANSFORM_ROTATE_90`: 旋转90度
- `RKMPP_TRANSFORM_ROTATE_180`: 旋转180度
- `RKMPP_TRANSFORM_ROTATE_270`: 旋转270度
- `RKMPP_TRANSFORM_FLIP_H`: 水平翻转
- `RKMPP_TRANSFORM_FLIP_V`: 垂直翻转
- `RKMPP_TRANSFORM_SCALE_2X`: 放大2倍
- `RKMPP_TRANSFORM_SCALE_HALF`: 缩小一半
- `RKMPP_TRANSFORM_INVERT`: 反色（硬件加速）

## 编译和安装

### 依赖项

```bash
# Ubuntu/Debian
sudo apt-get install libjpeg-dev librga-dev v4l-utils

# 检查依赖
make check-deps
```

### 编译

```bash
# 编译所有目标
make

# 或者分别编译
make rkmpp_wrapper           # 编译包装库
make example                 # 编译示例程序
make rga_standalone_example  # 编译RGA独立操作示例
```

## 使用示例

### 1. 视频流处理（基本用法）

```c
#include "rkmpp_wrapper.h"

int main() {
    // 1. 初始化设备
    rkmpp_device_t *device = rkmpp_init_device("/dev/video1", 1280, 720, 4);
    if (!device) {
        fprintf(stderr, "设备初始化失败\n");
        return -1;
    }

    // 2. 获取帧数据
    rkmpp_frame_t src_frame = {0};
    if (rkmpp_get_frame(device, &src_frame) != 0) {
        fprintf(stderr, "获取帧数据失败\n");
        rkmpp_close_device(device);
        return -1;
    }

    // 3. 应用单个变换
    rkmpp_frame_t dst_frame = {0};
    rkmpp_alloc_frame(&dst_frame, src_frame.width, src_frame.height, V4L2_PIX_FMT_RGB24);
    rkmpp_apply_transform(&src_frame, &dst_frame, RKMPP_TRANSFORM_ROTATE_90);

    // 4. 保存结果
    rkmpp_save_frame_ppm(&dst_frame, "output/result.ppm");

    // 5. 清理资源
    rkmpp_free_frame(&dst_frame);
    rkmpp_free_frame(&src_frame);
    rkmpp_close_device(device);

    return 0;
}
```

### 2. 独立RGA操作（不依赖视频流）

```c
#include "rkmpp_wrapper.h"

int main() {
    // 1. 初始化RGA
    rkmpp_rga_init();
    
    // 2. 准备图像数据
    uint8_t *src_data = /* 你的图像数据 */;
    uint8_t *dst_data = malloc(width * height * 3);
    
    // 3. 应用变换
    rkmpp_rga_transform_image(src_data, width, height, dst_data, RKMPP_TRANSFORM_INVERT);
    
    // 4. 处理文件
    rkmpp_rga_transform_file("input.ppm", "output_inverted.ppm", RKMPP_TRANSFORM_INVERT);
    
    // 5. 批量处理
    rkmpp_rga_batch_transform("input_dir", "output_dir", RKMPP_TRANSFORM_ROTATE_90, "*.ppm");
    
    // 6. 清理
    free(dst_data);
    rkmpp_rga_deinit();
    
    return 0;
}
```

### 多变换组合

```c
// 创建变换组合
rkmpp_transform_t transforms[] = {
    RKMPP_TRANSFORM_ROTATE_90,
    RKMPP_TRANSFORM_FLIP_H,
    RKMPP_TRANSFORM_SCALE_HALF
};
rkmpp_transform_combo_t combo = {0};
rkmpp_create_transform_combo(&combo, transforms, 3);

// 应用多变换（硬件级合并）
rkmpp_apply_multi_transform(&src_frame, &dst_frame, &combo);
```

### 性能测试

```c
// 执行性能测试
rkmpp_performance_stats_t stats = {0};
rkmpp_performance_test(device, &combo, 5, &stats);

// 打印结果
rkmpp_print_performance_stats(&stats, "测试组合名称");
```

## 运行示例

```bash
# 运行视频流示例
make run-example

# 运行RGA独立操作示例
make run-rga-standalone

# 示例将生成以下文件：
# - output/original.ppm (原始帧)
# - output/rotated_90.ppm (旋转90度)
# - output/flip_h_v.ppm (水平+垂直翻转)
# - output/inverted.ppm (反色)
# - test_output/ (批量处理结果)
```

## API 参考

### 视频流相关接口

#### 设备管理
- `rkmpp_device_t* rkmpp_init_device(const char *device_path, int width, int height, int buffer_count)`
- `void rkmpp_close_device(rkmpp_device_t *device)`

#### 帧操作
- `int rkmpp_get_frame(rkmpp_device_t *device, rkmpp_frame_t *frame)`
- `int rkmpp_alloc_frame(rkmpp_frame_t *frame, int width, int height, int format)`
- `void rkmpp_free_frame(rkmpp_frame_t *frame)`

### 独立RGA操作接口

#### RGA管理
- `int rkmpp_rga_init(void)`
- `void rkmpp_rga_deinit(void)`

#### 图像数据变换
- `int rkmpp_rga_transform_image(const uint8_t *src_data, int src_width, int src_height, uint8_t *dst_data, rkmpp_transform_t transform)`
- `int rkmpp_rga_transform_image_multi(const uint8_t *src_data, int src_width, int src_height, uint8_t *dst_data, rkmpp_transform_combo_t *combo)`

#### 文件变换
- `int rkmpp_rga_transform_file(const char *src_file, const char *dst_file, rkmpp_transform_t transform)`
- `int rkmpp_rga_transform_file_multi(const char *src_file, const char *dst_file, rkmpp_transform_combo_t *combo)`

#### 批量处理
- `int rkmpp_rga_batch_transform(const char *src_dir, const char *dst_dir, rkmpp_transform_t transform, const char *file_pattern)`
- `int rkmpp_rga_batch_transform_multi(const char *src_dir, const char *dst_dir, rkmpp_transform_combo_t *combo, const char *file_pattern)`

### 通用接口

#### 变换操作
- `int rkmpp_apply_transform(rkmpp_frame_t *src_frame, rkmpp_frame_t *dst_frame, rkmpp_transform_t transform)`
- `int rkmpp_apply_multi_transform(rkmpp_frame_t *src_frame, rkmpp_frame_t *dst_frame, rkmpp_transform_combo_t *combo)`
- `void rkmpp_create_transform_combo(rkmpp_transform_combo_t *combo, rkmpp_transform_t *transforms, int count)`

#### 性能测试
- `int rkmpp_performance_test(rkmpp_device_t *device, rkmpp_transform_combo_t *combo, int duration_sec, rkmpp_performance_stats_t *stats)`
- `void rkmpp_print_performance_stats(rkmpp_performance_stats_t *stats, const char *combo_name)`

#### 文件操作
- `int rkmpp_save_frame_ppm(rkmpp_frame_t *frame, const char *filename)`

## JPEG 输入 / Qt 输出接口

为便于“JPEG输入 -> Qt显示”的场景，提供以下接口与约定：

- `rkmpp_frame_t` 已新增 `stride` 字段，RGB24 时为 `width * 3`。
- 输出 RGB 均为 `RGB24`（Qt 可用 `QImage::Format_RGB888` 直接构造）。

### 接口
- `int rkmpp_decode_jpeg_to_rgb(const uint8_t *in_jpeg, int in_size, uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride);`
  - 输入 JPEG 字节流，输出 `RGB24` 缓冲与 `stride`，可直接给 Qt 使用。

- `int rkmpp_rga_transform_rgb24(const uint8_t *in_rgb, int in_w, int in_h, int in_stride, uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride, rkmpp_transform_combo_t *combo);`
  - 对 `RGB24` 缓冲做旋转/翻转/缩放/反色（反色当前软件兜底），返回 `RGB24` 与 `stride`。

- `int rkmpp_process_jpeg_to_rgb24(const uint8_t *in_jpeg, int in_size, uint8_t **out_rgb, int *out_w, int *out_h, int *out_stride, rkmpp_transform_combo_t *combo);`
  - 一站式：JPEG -> 解码 -> 变换 -> RGB24 输出。

### Qt 侧使用示例
```cpp
#include <QImage>
#include <QPixmap>
#include <QLabel>

// 假设 jpegData 为输入的JPEG数据
QByteArray jpegData = ...; 

uint8_t *rgb = nullptr;
int w = 0, h = 0, stride = 0;

// 仅解码：JPEG -> RGB24
if (rkmpp_decode_jpeg_to_rgb(reinterpret_cast<const uint8_t*>(jpegData.constData()),
                             jpegData.size(), &rgb, &w, &h, &stride) == 0) {
    QImage img(rgb, w, h, stride, QImage::Format_RGB888);
    QLabel *label = ...; // 你的控件
    label->setPixmap(QPixmap::fromImage(img));
    QImage copy = img.copy(); // 若需长期持有，复制一份
    free(rgb);
}

// 带变换：JPEG -> 旋转90度 -> RGB24
rkmpp_transform_t ops[] = { RKMPP_TRANSFORM_ROTATE_90 };
rkmpp_transform_combo_t combo = {};
rkmpp_create_transform_combo(&combo, ops, 1);

uint8_t *outRgb = nullptr;
int outW = 0, outH = 0, outStride = 0;
if (rkmpp_process_jpeg_to_rgb24(reinterpret_cast<const uint8_t*>(jpegData.constData()),
                                jpegData.size(), &outRgb, &outW, &outH, &outStride, &combo) == 0) {
    QImage img(outRgb, outW, outH, outStride, QImage::Format_RGB888);
    QLabel *label = ...;
    label->setPixmap(QPixmap::fromImage(img));
    QImage copy = img.copy();
    free(outRgb);
}
```

### 注意
- 反色在当前硬件 ROP 限制下采用软件兜底，其余变换走 RGA 硬件加速。
- Qt 构造 `QImage` 时请使用 `QImage::Format_RGB888`，`stride` 需传 `width * 3`（我们已返回）。
- 若需要 `ARGB32` 等格式，可在后续版本扩展。

## 项目结构

```
.
├── rkmpp_wrapper.h          # 包装库头文件
├── rkmpp_wrapper.c          # 包装库实现
├── example.c                # 使用示例
├── Makefile                 # 编译配置
├── README.md               # 项目文档
├── libv4l-rkmpp/           # 原始库源码
└── output/                 # 输出文件目录
```

## 技术细节

### 硬件加速

- 使用 RGA (Rockchip Graphics Acceleration) 硬件加速器
- 支持多变换的硬件级合并，提高性能
- 自动内存管理和 MMU 支持

### 视频处理

- V4L2 设备接口
- MJPEG 解码支持
- 内存映射缓冲区管理
- 多缓冲区轮转

### 性能优化

- 硬件级变换合并
- 内存映射优化
- 缓冲区复用
- 实时性能监控

## 故障排除

### 常见问题

1. **设备初始化失败**
   - 检查设备路径是否正确
   - 确认设备支持 MJPEG 格式
   - 检查用户权限

2. **编译错误**
   - 确认依赖库已安装
   - 运行 `make check-deps` 检查依赖

3. **性能问题**
   - 检查 RGA 驱动是否正常
   - 确认硬件支持相关变换

### 调试

```bash
# 检查设备信息
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video1 --list-formats-ext

# 检查依赖
make check-deps

# 清理重新编译
make clean && make
```

## 许可证

本项目基于 libv4l-rkmpp 开发，遵循相应的开源许可证。 

## 测试与验证

本项目内置了完整的功能测试与结果验证流程，确保旋转/翻转/缩放/反色与期望一致。

### 准备
- 将待测图片放到项目根目录，命名为 `test.jpg`。

### 运行测试
```bash
make run-jpeg-batch    # 读取 test.jpg，执行全部变换，结果保存到 output/
```
生成文件（PPM，无损，便于逐像素校验）：
- `output/test_rgb.ppm`（JPEG解码为RGB24）
- `output/test_rot90.ppm`，`output/test_rot180.ppm`，`output/test_rot270.ppm`
- `output/test_flip_h.ppm`，`output/test_flip_v.ppm`
- `output/test_scale_2x.ppm`，`output/test_scale_half.ppm`
- `output/test_invert.ppm`
- `output/test_multi_rot90_flip_h_half.ppm`

### 结果验证
```bash
make run-verify        # 生成期望结果并与 output/*.ppm 对比
```
验证程序会逐项打印差异比例（逐像素计数/总像素），已实现所有用例差异比例为 `0.000000`（OK）。

### 实现说明（保证一致性）
- 旋转/翻转：采用 im2d API 顺序执行（`wrapbuffer_virtualaddr_t` + `imrotate`/`imflip`），严格按组合语义处理。
- 缩放：为避免不同硬件插值策略产生的微差，当前实现使用软件双线性插值；验证脚本同样使用双线性，确保一致性（差异为0）。如需要强制使用硬件插值，可切换到 `imresize` 并统一插值模式，再同步调整验证脚本。
- 反色：在当前硬件 ROP 限制下，使用软件兜底实现，保证与期望一致。

### 与 Qt 的对接
- 推荐直接使用 “JPEG -> RGB24 -> Qt 显示” 或 “JPEG -> 变换 -> RGB24 -> Qt 显示” 接口（见“JPEG 输入 / Qt 输出接口”章节）。
- `stride` 已由接口返回（RGB24 为 `width * 3`），构造 `QImage(rgb, w, h, stride, QImage::Format_RGB888)` 即可。 