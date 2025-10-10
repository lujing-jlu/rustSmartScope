# RGA 人性化 API 使用指南

## 概述

RGA 人性化 API 提供了简单易用的接口，让开发者能够轻松使用 Rockchip RGA 2D 硬件加速器进行图像处理。

## 主要特性

- 🚀 **简单易用**: 链式调用，直观的 API 设计
- ⚡ **硬件加速**: 充分利用 RGA 硬件加速能力
- 🛡️ **类型安全**: 完整的 Rust 类型系统支持
- 🔧 **灵活配置**: 支持多种图像格式和变换操作
- 📁 **文件支持**: 直接支持常见图像格式的读写

## 快速开始

### 1. 基本使用

```rust
use rockchip_rga::{RgaProcessor, RgaTransform};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 创建 RGA 处理器
    let processor = RgaProcessor::new()?;
    
    // 加载图像
    let image = processor.load_image("input.jpg")?;
    
    // 应用变换
    let rotated = processor.transform(&image, &RgaTransform::Rotate90)?;
    
    // 保存结果
    rotated.save("output.jpg")?;
    
    Ok(())
}
```

### 2. 链式操作

```rust
// 批量应用多个变换
let transforms = vec![
    RgaTransform::Rotate90,
    RgaTransform::FlipHorizontal,
    RgaTransform::ScaleRatio(0.5, 0.5),
];

let result = processor.batch_transform(&image, &transforms)?;
```

## API 参考

### RgaProcessor

主要的处理器类，负责管理 RGA 上下文和执行图像变换。

#### 方法

- `new() -> Result<Self, RgaError>`: 创建新的处理器实例
- `load_image<P: AsRef<Path>>(path: P) -> Result<RgaImage, RgaError>`: 从文件加载图像
- `create_image(data, width, height, format) -> Result<RgaImage, RgaError>`: 从内存数据创建图像
- `transform(image, transform) -> Result<RgaImage, RgaError>`: 应用单个变换
- `batch_transform(image, transforms) -> Result<RgaImage, RgaError>`: 批量应用变换

### RgaTransform

变换操作枚举，定义了所有支持的图像变换类型。

#### 变换类型

- `Rotate90`: 旋转 90 度
- `Rotate180`: 旋转 180 度
- `Rotate270`: 旋转 270 度
- `FlipHorizontal`: 水平翻转
- `FlipVertical`: 垂直翻转
- `Rotate(f32)`: 自定义角度旋转（仅支持 0°, 90°, 180°, 270°）
- `Scale(u32, u32)`: 缩放到指定尺寸
- `ScaleRatio(f32, f32)`: 按比例缩放
- `Crop(u32, u32, u32, u32)`: 裁剪 (x, y, width, height)
- `Invert`: 反色（软件实现）
- `Composite(Vec<RgaTransform>)`: 组合变换

### RgaImage

图像类，封装了图像数据和相关操作。

#### 方法

- `new(width, height, format) -> Self`: 创建新图像
- `from_file<P: AsRef<Path>>(path: P) -> Result<Self, RgaError>`: 从文件加载
- `from_data(data, width, height, format) -> Result<Self, RgaError>`: 从内存数据创建
- `width() -> u32`: 获取图像宽度
- `height() -> u32`: 获取图像高度
- `format() -> RgaFormat`: 获取图像格式
- `data() -> &[u8]`: 获取图像数据
- `data_mut() -> &mut [u8]`: 获取可变图像数据
- `save<P: AsRef<Path>>(path: P) -> Result<(), RgaError>`: 保存图像到文件

#### 便捷方法

- `rotate_90() -> Result<Self, RgaError>`: 旋转 90 度
- `rotate_180() -> Result<Self, RgaError>`: 旋转 180 度
- `rotate_270() -> Result<Self, RgaError>`: 旋转 270 度
- `flip_horizontal() -> Result<Self, RgaError>`: 水平翻转
- `flip_vertical() -> Result<Self, RgaError>`: 垂直翻转
- `scale(new_width, new_height) -> Result<Self, RgaError>`: 缩放
- `scale_ratio(ratio_x, ratio_y) -> Result<Self, RgaError>`: 按比例缩放
- `crop(x, y, width, height) -> Result<Self, RgaError>`: 裁剪
- `invert() -> Result<Self, RgaError>`: 反色

### RgaFormat

图像格式枚举。

#### 支持的格式

- `Rgb888`: RGB 24位
- `Rgba8888`: RGBA 32位
- `Rgba5551`: RGBA 16位
- `Rgba4444`: RGBA 16位

## 使用示例

### 1. 基本图像处理

```rust
use rockchip_rga::{RgaProcessor, RgaTransform};

fn basic_processing() -> Result<(), Box<dyn std::error::Error>> {
    let processor = RgaProcessor::new()?;
    let image = processor.load_image("input.jpg")?;
    
    // 旋转并缩放
    let result = processor.transform(&image, &RgaTransform::Rotate90)?;
    let result = processor.transform(&result, &RgaTransform::ScaleRatio(0.5, 0.5))?;
    
    result.save("processed.jpg")?;
    Ok(())
}
```

### 2. 批量处理

```rust
fn batch_processing() -> Result<(), Box<dyn std::error::Error>> {
    let processor = RgaProcessor::new()?;
    let image = processor.load_image("input.jpg")?;
    
    let transforms = vec![
        RgaTransform::Rotate90,
        RgaTransform::FlipHorizontal,
        RgaTransform::ScaleRatio(0.8, 0.8),
        RgaTransform::Crop(100, 100, 400, 300),
    ];
    
    let result = processor.batch_transform(&image, &transforms)?;
    result.save("batch_processed.jpg")?;
    
    Ok(())
}
```

### 3. 从内存数据创建图像

```rust
fn create_from_memory() -> Result<(), Box<dyn std::error::Error>> {
    let processor = RgaProcessor::new()?;
    
    // 创建测试图像数据
    let width = 320;
    let height = 240;
    let mut data = vec![0u8; width as usize * height as usize * 3];
    
    // 填充数据...
    for (i, pixel) in data.chunks_mut(3).enumerate() {
        let x = (i % width as usize) as u8;
        let y = (i / width as usize) as u8;
        pixel[0] = x;     // R
        pixel[1] = y;     // G
        pixel[2] = 128;   // B
    }
    
    let image = processor.create_image(data, width, height, RgaFormat::Rgb888)?;
    let rotated = image.rotate_90()?;
    rotated.save("memory_created.jpg")?;
    
    Ok(())
}
```

### 4. 错误处理

```rust
fn error_handling() -> Result<(), Box<dyn std::error::Error>> {
    let processor = RgaProcessor::new()?;
    
    match processor.load_image("nonexistent.jpg") {
        Ok(image) => {
            println!("图像加载成功: {}x{}", image.width(), image.height());
        }
        Err(e) => {
            eprintln!("图像加载失败: {}", e);
            return Err(e.into());
        }
    }
    
    Ok(())
}
```

## 性能优化建议

1. **重用处理器**: 创建 `RgaProcessor` 实例的开销较大，建议重用同一个实例
2. **批量操作**: 使用 `batch_transform` 而不是多次调用 `transform`
3. **内存管理**: 对于大图像，注意及时释放不需要的 `RgaImage` 实例
4. **格式选择**: 优先使用 `Rgb888` 格式，因为硬件支持最好

## 注意事项

1. **硬件限制**: 某些变换操作可能受硬件限制，失败时会返回相应的错误
2. **内存对齐**: 内部自动处理 16 字节内存对齐
3. **线程安全**: 当前实现不是线程安全的，多线程使用时需要加锁
4. **错误处理**: 所有操作都可能失败，请妥善处理错误情况

## 故障排除

### 常见错误

- `RgaError::InitError`: RGA 初始化失败，检查硬件支持
- `RgaError::InvalidParameter`: 参数错误，检查图像尺寸和格式
- `RgaError::OperationError`: 操作失败，可能是硬件不支持该操作
- `RgaError::IoError`: 文件 I/O 错误，检查文件路径和权限

### 调试技巧

1. 启用详细日志输出
2. 检查图像格式和尺寸是否在硬件支持范围内
3. 验证 RGA 驱动是否正确安装
4. 使用较小的测试图像进行调试

## 更多示例

更多使用示例请参考 `examples/` 目录下的示例代码：

- `rga_simple_demo.rs`: 基本使用示例
- `rga_comprehensive_test.rs`: 全面功能测试
- `rga_scale_jpg.rs`: JPEG 缩放示例
