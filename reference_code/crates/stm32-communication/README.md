# STM32 Communication Library

STM32 HID通信库，用于SmartScope项目中的设备控制。支持LED亮度控制、电池监控、温度传感器读取、镜头控制和舵机角度控制。

## 功能特性

### 🔌 HID通信
- 基于USB HID协议的设备通信
- 自动设备枚举和连接管理
- 支持数据发送和接收
- 超时和错误处理

### 💡 亮度控制
- **新协议**: 6档亮度等级 (0-5)
  - 0: 关闭 (0%)
  - 1: 低亮度 (20%)
  - 2: 中低亮度 (40%)
  - 3: 中等亮度 (60%)
  - 4: 高亮度 (80%)
  - 5: 最大亮度 (100%)

### 🔍 镜头控制
- **镜头锁定**: 支持镜头锁定/解锁状态控制
- **调节速度**: 3档调节速度
  - 0: 快调
  - 1: 慢调
  - 2: 微调

### 🎛️ 舵机控制 (新功能)
- **双轴控制**: X轴和Y轴独立控制
- **角度范围**: 30.0° - 150.0°
- **默认位置**: 90.0° (中心位置)
- **精确控制**: 浮点数精度角度设置

### 📊 状态监控
- 实时温度监测 (摄氏度)
- 电池电量监控 (百分比)
- 设备状态查询
- 时间戳记录

## 协议格式

### 数据包结构 (32字节)

| 字段名 | 字节偏移 | 长度 | 数据类型 | 描述 |
|--------|----------|------|----------|------|
| Header | 0 | 2 | uint8_t[2] | 协议头 `0x55 0xAA` |
| Command | 2 | 1 | uint8_t | 指令类型 |
| 温度 | 3 | 2 | int16_t | 温度(摄氏度*10) |
| 亮度 | 5 | 2 | uint16_t | 亮度等级 (0-5) |
| 电池百分比 | 7 | 2 | uint16_t | 电池百分比 (0-1000) |
| 镜头锁定 | 9 | 1 | uint8_t | 镜头锁定状态 |
| 镜头快慢 | 10 | 1 | uint8_t | 镜头调节速度 |
| 舵机X轴角度 | 11 | 4 | float | X轴角度 (30.0-150.0) |
| 舵机Y轴角度 | 15 | 4 | float | Y轴角度 (30.0-150.0) |
| ... | 19 | 11 | - | 保留字段 |
| CRC16 | 30 | 2 | uint16_t | 校验和 |

## 使用示例

### 基本使用

```rust
use stm32_communication::{Stm32Controller, HidConfig};

// 创建控制器
let mut controller = Stm32Controller::new(None);

// 初始化
controller.initialize()?;

// 设置亮度等级
controller.set_light_level(3)?; // 60%亮度

// 读取设备状态
let status = controller.read_device_status()?;
println!("温度: {:.1}°C", status.temperature);
println!("电池: {:.1}%", status.battery_value);
```

### 舵机控制

```rust
// 设置单个舵机角度
controller.set_servo_x_angle(45.0)?;
controller.set_servo_y_angle(135.0)?;

// 同时设置两个舵机角度
controller.set_servo_angles(60.0, 120.0)?;

// 重置到默认位置 (90°, 90°)
controller.reset_servo_angles()?;
```

### 镜头控制

```rust
// 锁定镜头
controller.set_lens_locked(true)?;

// 设置调节速度为慢调
controller.set_lens_speed(1)?;

// 检查镜头状态
if controller.is_lens_locked() {
    println!("镜头已锁定");
}
```

### 批量配置更新

```rust
// 一次性设置所有参数
controller.set_device_config(
    3,      // 亮度等级 (60%)
    true,   // 镜头锁定
    1,      // 慢调速度
    45.0,   // X轴角度
    135.0   // Y轴角度
)?;

// 获取当前配置
let config = controller.get_device_config();
println!("当前配置: {:?}", config);

// 重置到默认设置
controller.reset_to_defaults()?;
```

### 舵机校准

```rust
// 执行舵机校准序列
controller.calibrate_servos()?;
```

### 周期性状态监控

```rust
// 启动周期性状态更新 (每1000ms)
controller.start_periodic_updates(1000);

// 接收状态更新消息
while let Ok(message) = controller.receive_message() {
    match message {
        ControllerMessage::StatusUpdate(status) => {
            println!("状态更新: 温度={:.1}°C, 电池={:.1}%",
                     status.temperature, status.battery_value);
        }
        ControllerMessage::Error(e) => {
            println!("错误: {}", e);
        }
        _ => {}
    }
}
```

## 运行示例

```bash
# 基础舵机控制示例
cargo run --example servo_control

# 高级控制功能示例
cargo run --example advanced_control

# 性能测试示例
cargo run --example performance_test

# 运行所有测试
cargo test

# 运行特定包的测试
cargo test -p stm32-communication
```

## 错误处理

库提供了完整的错误类型定义：

```rust
pub enum Stm32Error {
    DeviceNotFound,           // 设备未找到
    DeviceNotConnected,       // 设备未连接
    PermissionDenied(String), // 权限拒绝
    IoError(String),          // IO错误
    HidApiError(String),      // HID API错误
    Timeout,                  // 通信超时
    InvalidResponse,          // 无效响应
    CrcError,                 // CRC校验失败
    ProtocolError(String),    // 协议错误
    ConfigurationError(String), // 配置错误
    InitializationError(String), // 初始化错误
}
```

## 更新日志

### v0.1.0 - 协议升级
- ✨ 新增舵机双轴角度控制 (X/Y轴, 30°-150°)
- ✨ 新增镜头锁定状态控制
- ✨ 新增镜头调节速度控制 (快调/慢调/微调)
- 🔄 更新亮度控制范围 (0-5等级)
- 🔄 优化电池百分比显示
- 🧪 完善测试用例覆盖
- 📚 添加详细文档和示例

## 许可证

本项目采用 MIT 许可证。
