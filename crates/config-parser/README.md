# Config Parser

SmartScope 项目的配置管理库，支持相机发现和系统配置的解析、验证和管理。

## 🚀 特性

- **多格式支持**: JSON 和 TOML 配置文件格式
- **相机发现配置**: 灵活的相机搜索关键词管理
- **优先级设备**: 手动指定高优先级相机设备
- **关键词匹配**: 支持模糊匹配和精确匹配
- **配置验证**: 完整的配置有效性检查
- **类型安全**: 基于 Rust 类型系统的配置管理

## 📦 快速开始

### 基本用法

```rust
use config_parser::{AppConfig, ConfigParser};

// 创建默认配置
let config = AppConfig::default();

// 保存配置
ConfigParser::save_to_file(&config, "config.json")?;

// 加载配置
let loaded_config = ConfigParser::load_from_file("config.json")?;

// 验证配置
ConfigParser::validate_config(&loaded_config)?;
```

### 相机发现配置

```rust
use config_parser::{CameraDiscoveryConfig, PriorityDevice, CameraRole};

let mut discovery_config = CameraDiscoveryConfig::default();

// 添加搜索关键词
discovery_config.add_search_keyword("mycamera".to_string());
discovery_config.add_exclude_keyword("virtual".to_string());

// 添加优先级设备
let priority_device = PriorityDevice::new(
    "左相机".to_string(),
    "/dev/video3".to_string(),
    CameraRole::Left,
);
discovery_config.add_priority_device(priority_device);

// 测试关键词匹配
let matches = discovery_config.matches_keyword("USB Camera");
```

## 🔧 配置结构

### 应用配置 (AppConfig)

```rust
pub struct AppConfig {
    pub camera: CameraConfig,      // 相机配置
    pub ui: UiConfig,             // 界面配置
    pub processing: ProcessingConfig, // 处理配置
}
```

### 相机配置 (CameraConfig)

```rust
pub struct CameraConfig {
    pub left_camera_keyword: String,   // 左相机关键词
    pub right_camera_keyword: String,  // 右相机关键词
    pub resolution: Resolution,         // 分辨率
    pub frame_rate: u32,               // 帧率
    pub auto_discovery: bool,          // 自动发现
    pub discovery: CameraDiscoveryConfig, // 发现配置
}
```

### 相机发现配置 (CameraDiscoveryConfig)

```rust
pub struct CameraDiscoveryConfig {
    pub search_keywords: Vec<String>,     // 搜索关键词
    pub exclude_keywords: Vec<String>,    // 排除关键词
    pub fuzzy_match: bool,               // 模糊匹配
    pub case_sensitive: bool,            // 区分大小写
    pub max_scan_devices: u32,           // 最大扫描设备数
    pub scan_timeout_seconds: u32,       // 扫描超时
    pub priority_devices: Vec<PriorityDevice>, // 优先级设备
    pub validation: DeviceValidationConfig,    // 设备验证
}
```

## 🎯 使用场景

### 场景1: 手动配置相机搜索关键词

```rust
let mut config = AppConfig::default();

// 添加自定义搜索关键词
config.camera.discovery.add_search_keyword("专用相机".to_string());
config.camera.discovery.add_search_keyword("industrial_camera".to_string());

// 排除不需要的设备
config.camera.discovery.add_exclude_keyword("loopback".to_string());
config.camera.discovery.add_exclude_keyword("virtual".to_string());

// 保存配置
ConfigParser::save_to_file(&config, "camera_config.json")?;
```

### 场景2: 配置优先级设备

```rust
let mut discovery_config = CameraDiscoveryConfig::default();

// 添加左相机优先级设备
let left_camera = PriorityDevice::new(
    "主左相机".to_string(),
    "/dev/video3".to_string(),
    CameraRole::Left,
);
discovery_config.add_priority_device(left_camera);

// 添加右相机优先级设备
let right_camera = PriorityDevice::new(
    "主右相机".to_string(),
    "/dev/video1".to_string(),
    CameraRole::Right,
);
discovery_config.add_priority_device(right_camera);

// 查询指定角色的设备
let left_devices = discovery_config.get_priority_devices_by_role(&CameraRole::Left);
```

### 场景3: 关键词匹配测试

```rust
let discovery_config = CameraDiscoveryConfig::default();

let test_devices = vec![
    "USB Camera",
    "webcam_device",
    "hdmirx_controller",
    "virtual_camera",
];

for device in &test_devices {
    if discovery_config.matches_keyword(device) {
        println!("设备 '{}' 匹配搜索条件", device);
    }
}
```

## 📊 API 参考

### ConfigParser

- `load_from_file(path)` - 从文件加载配置
- `save_to_file(config, path)` - 保存配置到文件
- `validate_config(config)` - 验证完整配置
- `validate_camera_discovery_config(config)` - 验证相机发现配置
- `create_default_config(path)` - 创建默认配置文件

### CameraDiscoveryConfig

- `add_search_keyword(keyword)` - 添加搜索关键词
- `remove_search_keyword(keyword)` - 移除搜索关键词
- `add_exclude_keyword(keyword)` - 添加排除关键词
- `remove_exclude_keyword(keyword)` - 移除排除关键词
- `add_priority_device(device)` - 添加优先级设备
- `remove_priority_device(path)` - 移除优先级设备
- `matches_keyword(device_name)` - 检查关键词匹配
- `get_priority_devices_by_role(role)` - 获取指定角色设备

### PriorityDevice

- `new(name, path, role)` - 创建新设备
- `enable()` - 启用设备
- `disable()` - 禁用设备

## 🧪 示例

运行示例程序：

```bash
# 基本配置示例
cargo run --example config_test

# 相机发现配置示例
cargo run --example camera_discovery_demo
```

## 📋 默认配置

### 搜索关键词
- "camera"
- "webcam" 
- "usb"

### 排除关键词
- "hdmirx"
- "dummy"
- "loopback"

### 优先级设备
- 左相机: `/dev/video3`
- 右相机: `/dev/video1`

### 验证配置
- 最小分辨率: 640x480
- 最大分辨率: 1920x1080
- 必需格式: ["MJPG", "YUYV"]

## 🔧 配置文件示例

### JSON 格式

```json
{
  "camera": {
    "left_camera_keyword": "cameraL",
    "right_camera_keyword": "cameraR",
    "discovery": {
      "search_keywords": ["camera", "webcam", "usb"],
      "exclude_keywords": ["hdmirx", "dummy", "loopback"],
      "fuzzy_match": true,
      "case_sensitive": false,
      "priority_devices": [
        {
          "name": "左相机",
          "device_path": "/dev/video3",
          "camera_role": "Left",
          "enabled": true
        }
      ]
    }
  }
}
```

### TOML 格式

```toml
[camera]
left_camera_keyword = "cameraL"
right_camera_keyword = "cameraR"

[camera.discovery]
search_keywords = ["camera", "webcam", "usb"]
exclude_keywords = ["hdmirx", "dummy", "loopback"]
fuzzy_match = true
case_sensitive = false

[[camera.discovery.priority_devices]]
name = "左相机"
device_path = "/dev/video3"
camera_role = "Left"
enabled = true
```

## 📄 许可证

MIT OR Apache-2.0
