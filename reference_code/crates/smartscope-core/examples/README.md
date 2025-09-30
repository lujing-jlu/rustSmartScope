# SmartScope Core 示例程序

这个目录包含了多个示例程序，展示如何使用 SmartScope Core 库进行相机操作和测试。

## 可用示例

### 1. 基础相机演示 (`basic_camera_demo.rs`)

展示基本的相机操作流程：
- 创建和初始化 SmartScopeCore
- 配置相机参数
- 启动和停止相机流
- 获取帧数据
- 基础数据验证

```bash
cargo run --example basic_camera_demo
```

**适用场景**: 初学者了解基本API用法

### 2. 性能基准测试 (`performance_benchmark.rs`)

专门用于测试和分析相机性能：
- 帧率基准测试
- 延迟分析统计
- 长时间稳定性测试
- 不同分辨率性能对比
- 双相机性能对比

```bash
cargo run --example performance_benchmark
```

**适用场景**: 性能调优和系统评估

### 3. 立体相机演示 (`stereo_camera_demo.rs`)

专门展示立体相机功能：
- 双相机同步捕获
- 时间戳同步分析
- 立体校正功能测试
- 双相机帧率对比
- 同步质量评估

```bash
cargo run --example stereo_camera_demo
```

**适用场景**: 立体视觉和3D重建应用

### 4. 相机诊断工具 (`camera_diagnostics.rs`)

用于系统问题诊断和故障排除：
- 系统相机设备检测
- V4L2设备状态检查 (Linux)
- SmartScopeCore功能验证
- 错误处理机制测试
- 性能问题诊断

```bash
cargo run --example camera_diagnostics
```

**适用场景**: 故障排除和系统维护

## 运行要求

### 基础要求
- Rust 1.70+
- 已连接的USB相机设备
- 适当的设备权限

### Linux额外要求
```bash
# 添加用户到video组以获取相机访问权限
sudo usermod -a -G video $USER

# 安装V4L2工具 (可选，用于详细设备信息)
sudo apt install v4l-utils
```

### macOS要求
- 可能需要在"系统偏好设置 > 安全性与隐私 > 相机"中授予权限

### Windows要求
- 确保相机驱动已正确安装
- Windows Camera app应该能够识别设备

## 使用建议

1. **初次使用**: 先运行 `camera_diagnostics` 确认系统配置正确
2. **学习API**: 使用 `basic_camera_demo` 了解基本操作
3. **性能测试**: 使用 `performance_benchmark` 评估系统性能
4. **立体应用**: 使用 `stereo_camera_demo` 开发3D相关功能

## 常见问题

### 权限问题
```
错误: Permission denied (os error 13)
解决: sudo usermod -a -G video $USER && logout
```

### 设备未找到
```
错误: 未发现任何视频设备
检查: lsusb | grep -i camera
```

### 帧率过低
```
现象: 实际帧率远低于配置值
排查: 检查USB接口速度和系统负载
```

## 集成测试

除了示例程序，还可以运行集成测试：

```bash
# 基础单元测试
cargo test

# 真实相机集成测试 (需要手动启用)
cargo test camera_detection -- --ignored
cargo test real_camera_initialization -- --ignored
cargo test real_camera_performance -- --ignored
```

## 开发建议

- 示例程序使用相对简单的配置，实际应用中应根据需求调整参数
- 生产环境中建议添加更完善的错误处理和重试机制
- 对于实时应用，考虑使用异步模式和缓冲区管理