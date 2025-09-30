# USB Camera Examples

这个目录包含了 `usb-camera` 库的使用示例。

## 示例列表

### `usb_camera_example.rs` - 完整的最佳实践示例

这是一个完整的示例，展示了如何正确使用 usb-camera 库：

#### 功能特性

- ✅ **设备发现**: 自动检测可用的USB相机设备
- ✅ **配置管理**: 设置分辨率、帧率、像素格式等参数
- ✅ **流管理**: 正确的启动、停止和资源清理
- ✅ **帧处理**: 高效的帧读取和数据处理
- ✅ **错误处理**: 完善的错误处理和恢复机制
- ✅ **性能监控**: 实时FPS、帧大小、延迟统计
- ✅ **示例保存**: 自动保存示例图像文件

#### 运行示例

```bash
# 在 crates 目录下运行
cargo run --example usb_camera_example

# 或者启用日志输出
RUST_LOG=debug cargo run --example usb_camera_example
```

#### 预期输出

```
🎥 USB Camera Linux-Video 实现示例
==================================================

📹 测试设备: /dev/video1
📋 配置: 1280x720 @ 30 FPS, 格式: MJPG
✅ 流启动成功
📸 开始捕获帧...
  📦 帧 1: 1280x720, 174 KB, ID: 1
💾 示例帧已保存到: sample_frame_1.jpg
  📦 帧 2: 1280x720, 174 KB, ID: 2
  ...
  📦 帧 30: 1280x720, 227 KB, ID: 30

📊 性能统计:
  总帧数: 30
  总时长: 0.97s
  平均FPS: 30.77
  平均帧大小: 221.8 KB
  帧间隔: 平均 33.2ms, 最小 0ms, 最大 40ms
🛑 流已停止

🎉 示例完成！
```

#### 生成的文件

运行示例后会生成以下文件：
- `sample_frame_1.jpg` - 左相机的示例图像
- `sample_frame_3.jpg` - 右相机的示例图像

## 代码结构说明

### 主要组件

1. **CameraConfig** - 相机配置结构
   ```rust
   let config = CameraConfig {
       width: 1280,
       height: 720,
       framerate: 30,
       pixel_format: "MJPG".to_string(),
       parameters: HashMap::new(),
   };
   ```

2. **CameraStreamReader** - 流读取器
   ```rust
   let mut reader = CameraStreamReader::new(device_path, &camera_name, config);
   ```

3. **流管理** - 启动、读取、停止
   ```rust
   reader.start()?;                    // 启动流
   let frame = reader.read_frame()?;   // 读取帧
   reader.stop()?;                     // 停止流
   ```

### 最佳实践

1. **资源管理**: 总是在使用完毕后调用 `stop()` 方法
2. **错误处理**: 使用 `Result` 类型处理所有可能的错误
3. **性能监控**: 监控FPS、帧大小、延迟等关键指标
4. **设备检查**: 在使用前检查设备是否存在
5. **流稳定**: 启动流后等待一段时间让其稳定

### 性能特点

- **真实帧率**: ~30 FPS (符合相机规格)
- **低延迟**: 平均33ms帧间隔
- **高效率**: 直接V4L2内存映射，无重复帧
- **稳定性**: 连续帧捕获无丢失

## 故障排除

### 常见问题

1. **设备不存在**
   ```
   ❌ 设备不存在: /dev/video1
   ```
   - 检查相机是否连接
   - 确认设备路径是否正确
   - 运行 `ls /dev/video*` 查看可用设备

2. **权限问题**
   ```
   ❌ 设备测试失败: Permission denied
   ```
   - 确保用户在 `video` 组中
   - 或使用 `sudo` 运行

3. **设备占用**
   ```
   ❌ 流启动失败: Device or resource busy
   ```
   - 关闭其他使用相机的应用程序
   - 检查是否有其他进程占用设备

### 调试技巧

1. **启用详细日志**:
   ```bash
   RUST_LOG=debug cargo run --example usb_camera_example
   ```

2. **检查设备信息**:
   ```bash
   v4l2-ctl -d /dev/video1 --all
   ```

3. **监控系统资源**:
   ```bash
   htop  # 查看CPU和内存使用
   ```

## 技术细节

### Linux-Video 实现优势

- **直接V4L2访问**: 避免外部进程调用开销
- **内存映射**: 高效的零拷贝数据传输
- **持续流**: 保持设备连接，减少延迟
- **错误恢复**: 自动重连和错误处理

### 与传统方案对比

| 特性 | Linux-Video | v4l2-ctl |
|------|-------------|-----------|
| 性能 | 高 (直接API) | 低 (进程调用) |
| 延迟 | 低 (~33ms) | 高 (>100ms) |
| 资源使用 | 低 | 高 |
| 错误处理 | 完善 | 有限 |
| 帧重复 | 无 | 可能有 |

这个示例展示了如何充分利用 linux-video 实现的优势，获得最佳的相机性能。
