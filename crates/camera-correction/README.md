# Camera Correction Library

这个库为 SmartScope 项目提供相机参数加载和验证功能，与现有的 C++ 实现完全兼容。

## 功能特性

- **参数加载**: 从 SmartScope 格式文件加载相机参数
- **参数验证**: 验证相机参数的完整性和正确性
- **OpenCV 集成就绪**: 为 OpenCV 集成做好准备
- **高性能**: 纯 Rust 实现，无外部依赖

## 安装

### 依赖要求

- Rust 1.70+

### 编译

```bash
cargo build --release
```

## 使用方法

### 基本用法

```rust
use camera_correction::{CameraParameters, StereoParameters};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 从目录加载相机参数
    let params = CameraParameters::from_directory("./camera_parameters")?;

    // 获取立体参数
    let stereo = params.get_stereo_parameters()?;
    
    // 访问相机内参
    let left_fx = stereo.left_intrinsics.matrix.fx;
    let left_fy = stereo.left_intrinsics.matrix.fy;
    
    // 访问畸变系数
    let left_k1 = stereo.left_intrinsics.distortion.k1;
    
    // 访问外参
    let baseline = stereo.right_extrinsics.translation[0].abs();
    
    Ok(())
}
```

### 参数验证

```rust
use camera_correction::CameraParameters;

let params = CameraParameters::from_directory("./camera_parameters")?;
let stereo = params.get_stereo_parameters()?;

// 验证所有参数
stereo.validate()?;
println!("参数验证通过！");
```

## 相机参数文件格式

库支持 SmartScope 的标准相机参数文件格式：

### 内参文件 (camera0_intrinsics.dat, camera1_intrinsics.dat)
```
intrinsic:
fx 0.0 cx
0.0 fy cy
0.0 0.0 1.0
distortion:
k1 k2 p1 p2 k3
```

### 外参文件 (camera1_rot_trans.dat)
```
R:
r11 r12 r13
r21 r22 r23
r31 r32 r33
T:
tx
ty
tz
```

## 示例程序

运行示例程序：

```bash
# 使用默认参数
cargo run --example parameter_loader

# 指定相机参数目录
cargo run --example parameter_loader -- --params-dir ./my_camera_params

# 启用详细日志
cargo run --example parameter_loader -- --verbose
```

## API 参考

### CameraParameters

主要的相机参数管理类：

- `from_directory(path)`: 从目录加载相机参数
- `get_stereo_parameters()`: 获取立体相机参数

### StereoParameters

立体相机参数：

- `left_intrinsics`: 左相机内参
- `right_intrinsics`: 右相机内参
- `right_extrinsics`: 右相机外参（相对于左相机）

### CameraMatrix

相机内参矩阵：

- `fx`, `fy`: 焦距
- `cx`, `cy`: 主点坐标
- `to_matrix()`: 转换为 3x3 矩阵

### DistortionCoeffs

畸变系数：

- `k1`, `k2`, `k3`: 径向畸变系数
- `p1`, `p2`: 切向畸变系数
- `to_vector()`: 转换为向量

## 错误处理

库使用 `Result<T, CorrectionError>` 进行错误处理：

```rust
match CameraParameters::from_directory("./camera_parameters") {
    Ok(params) => {
        println!("参数加载成功");
    }
    Err(e) => {
        eprintln!("参数加载失败: {}", e);
    }
}
```

## 测试

运行测试：

```bash
cargo test
```

运行文档测试：

```bash
cargo test --doc
```

## 性能特性

- 纯 Rust 实现，无 GC 开销
- 零拷贝参数访问
- 高效的文件解析
- 内存友好的数据结构

## 扩展性

这个库设计为可扩展的：

1. **当前版本**: 参数加载和验证
2. **未来版本**: OpenCV 集成，畸变校正，立体校正
3. **插件系统**: 支持不同的相机参数格式

## 故障排除

### 参数文件问题

- 确保参数文件格式正确
- 检查文件路径和权限
- 验证相机矩阵和畸变系数的维度

### 编译问题

- 确保 Rust 版本 >= 1.70
- 检查工作区配置

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

MIT License

## 相关项目

- [SmartScope](https://github.com/smartscope): 主要的 C++ 项目
- [OpenCV](https://opencv.org/): 计算机视觉库（未来集成）
