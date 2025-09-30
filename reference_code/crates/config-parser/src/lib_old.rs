use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::Path;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum ConfigError {
    #[error("Unsupported file format: {0}")]
    UnsupportedFormat(String),
    #[error("Failed to read config file: {0}")]
    ReadError(String),
    #[error("Failed to parse config: {0}")]
    ParseError(String),
    #[error("Failed to write config file: {0}")]
    WriteError(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraConfig {
    pub left: CameraSideConfig,
    pub right: CameraSideConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraSideConfig {
    pub search_keywords: Vec<String>,
    pub format: String,
    pub frame_rate: u32,
    pub resolution: Resolution,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Resolution {
    pub width: u32,
    pub height: u32,
}

/// 相机发现配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraDiscoveryConfig {
    /// 搜索关键词列表 - 支持多个关键词匹配
    pub search_keywords: Vec<String>,
    /// 排除关键词列表 - 包含这些关键词的设备将被忽略
    pub exclude_keywords: Vec<String>,
    /// 是否启用模糊匹配 (部分匹配)
    pub fuzzy_match: bool,
    /// 是否区分大小写
    pub case_sensitive: bool,
    /// 最大搜索深度 (扫描设备数量限制)
    pub max_scan_devices: u32,
    /// 搜索超时时间 (秒)
    pub scan_timeout_seconds: u32,
    /// 优先级设备路径 - 手动指定的设备路径，优先级最高
    pub priority_devices: Vec<PriorityDevice>,
    /// 设备验证配置
    pub validation: DeviceValidationConfig,
}

/// 优先级设备配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PriorityDevice {
    /// 设备名称/标识
    pub name: String,
    /// 设备路径
    pub device_path: String,
    /// 相机角色 (Left/Right)
    pub camera_role: CameraRole,
    /// 是否启用
    pub enabled: bool,
}

/// 相机角色
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CameraRole {
    Left,
    Right,
    Any,
}

/// 设备验证配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceValidationConfig {
    /// 是否验证设备可用性
    pub verify_device_access: bool,
    /// 是否验证支持的格式
    pub verify_supported_formats: bool,
    /// 必需的视频格式 (如 "MJPG", "YUYV")
    pub required_formats: Vec<String>,
    /// 最小分辨率要求
    pub min_resolution: Resolution,
    /// 最大分辨率要求
    pub max_resolution: Resolution,
}

impl Default for CameraDiscoveryConfig {
    fn default() -> Self {
        Self {
            search_keywords: vec![
                "camera".to_string(),
                "webcam".to_string(),
                "usb".to_string(),
            ],
            exclude_keywords: vec![
                "hdmirx".to_string(),
                "dummy".to_string(),
                "loopback".to_string(),
            ],
            fuzzy_match: true,
            case_sensitive: false,
            max_scan_devices: 20,
            scan_timeout_seconds: 10,
            priority_devices: vec![
                PriorityDevice {
                    name: "左相机".to_string(),
                    device_path: "/dev/video3".to_string(),
                    camera_role: CameraRole::Left,
                    enabled: true,
                },
                PriorityDevice {
                    name: "右相机".to_string(),
                    device_path: "/dev/video1".to_string(),
                    camera_role: CameraRole::Right,
                    enabled: true,
                },
            ],
            validation: DeviceValidationConfig::default(),
        }
    }
}

impl Default for DeviceValidationConfig {
    fn default() -> Self {
        Self {
            verify_device_access: true,
            verify_supported_formats: true,
            required_formats: vec!["MJPG".to_string(), "YUYV".to_string()],
            min_resolution: Resolution {
                width: 640,
                height: 480,
            },
            max_resolution: Resolution {
                width: 1920,
                height: 1080,
            },
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    pub camera: CameraConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UiConfig {
    pub theme: String,
    pub language: String,
    pub window_size: Resolution,
    pub fullscreen: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProcessingConfig {
    pub enable_gpu_acceleration: bool,
    pub max_threads: Option<u32>,
    pub buffer_size: u32,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            camera: CameraConfig {
                left_camera_keyword: "cameraL".to_string(),
                right_camera_keyword: "cameraR".to_string(),
                resolution: Resolution {
                    width: 1920,
                    height: 1080,
                },
                frame_rate: 30,
                auto_discovery: true,
                discovery: CameraDiscoveryConfig::default(),
            },
            ui: UiConfig {
                theme: "dark".to_string(),
                language: "zh-CN".to_string(),
                window_size: Resolution {
                    width: 1280,
                    height: 720,
                },
                fullscreen: false,
            },
            processing: ProcessingConfig {
                enable_gpu_acceleration: true,
                max_threads: None,
                buffer_size: 10,
            },
        }
    }
}

impl CameraDiscoveryConfig {
    /// 添加搜索关键词
    pub fn add_search_keyword(&mut self, keyword: String) {
        if !self.search_keywords.contains(&keyword) {
            self.search_keywords.push(keyword);
        }
    }

    /// 移除搜索关键词
    pub fn remove_search_keyword(&mut self, keyword: &str) {
        self.search_keywords.retain(|k| k != keyword);
    }

    /// 添加排除关键词
    pub fn add_exclude_keyword(&mut self, keyword: String) {
        if !self.exclude_keywords.contains(&keyword) {
            self.exclude_keywords.push(keyword);
        }
    }

    /// 移除排除关键词
    pub fn remove_exclude_keyword(&mut self, keyword: &str) {
        self.exclude_keywords.retain(|k| k != keyword);
    }

    /// 添加优先级设备
    pub fn add_priority_device(&mut self, device: PriorityDevice) {
        // 检查是否已存在相同路径的设备
        if let Some(existing) = self
            .priority_devices
            .iter_mut()
            .find(|d| d.device_path == device.device_path)
        {
            *existing = device;
        } else {
            self.priority_devices.push(device);
        }
    }

    /// 移除优先级设备
    pub fn remove_priority_device(&mut self, device_path: &str) {
        self.priority_devices
            .retain(|d| d.device_path != device_path);
    }

    /// 获取指定角色的优先级设备
    pub fn get_priority_devices_by_role(&self, role: &CameraRole) -> Vec<&PriorityDevice> {
        self.priority_devices
            .iter()
            .filter(|d| {
                d.enabled
                    && (matches!(d.camera_role, CameraRole::Any)
                        || std::mem::discriminant(&d.camera_role) == std::mem::discriminant(role))
            })
            .collect()
    }

    /// 检查关键词是否匹配
    pub fn matches_keyword(&self, device_name: &str) -> bool {
        let name = if self.case_sensitive {
            device_name.to_string()
        } else {
            device_name.to_lowercase()
        };

        // 检查排除关键词
        for exclude in &self.exclude_keywords {
            let exclude_key = if self.case_sensitive {
                exclude.clone()
            } else {
                exclude.to_lowercase()
            };

            if self.fuzzy_match {
                if name.contains(&exclude_key) {
                    return false;
                }
            } else if name == exclude_key {
                return false;
            }
        }

        // 检查搜索关键词
        for search in &self.search_keywords {
            let search_key = if self.case_sensitive {
                search.clone()
            } else {
                search.to_lowercase()
            };

            if self.fuzzy_match {
                if name.contains(&search_key) {
                    return true;
                }
            } else if name == search_key {
                return true;
            }
        }

        false
    }
}

impl PriorityDevice {
    /// 创建新的优先级设备
    pub fn new(name: String, device_path: String, camera_role: CameraRole) -> Self {
        Self {
            name,
            device_path,
            camera_role,
            enabled: true,
        }
    }

    /// 启用设备
    pub fn enable(&mut self) {
        self.enabled = true;
    }

    /// 禁用设备
    pub fn disable(&mut self) {
        self.enabled = false;
    }
}

#[derive(Debug)]
pub enum ConfigFormat {
    Json,
    Toml,
}

impl ConfigFormat {
    pub fn from_extension(ext: &str) -> Result<Self> {
        match ext.to_lowercase().as_str() {
            "json" => Ok(ConfigFormat::Json),
            "toml" => Ok(ConfigFormat::Toml),
            _ => Err(ConfigError::UnsupportedFormat(ext.to_string()).into()),
        }
    }

    pub fn from_path<P: AsRef<Path>>(path: P) -> Result<Self> {
        let path = path.as_ref();
        let extension = path
            .extension()
            .and_then(|ext| ext.to_str())
            .ok_or_else(|| ConfigError::UnsupportedFormat("no extension".to_string()))?;

        Self::from_extension(extension)
    }
}

pub struct ConfigParser;

impl ConfigParser {
    /// 从文件加载配置
    pub fn load_from_file<P: AsRef<Path>>(path: P) -> Result<AppConfig> {
        let path = path.as_ref();
        let content = fs::read_to_string(path)
            .with_context(|| format!("Failed to read config file: {}", path.display()))?;

        let format = ConfigFormat::from_path(path)?;
        Self::parse_config(&content, format)
    }

    /// 解析配置字符串
    pub fn parse_config(content: &str, format: ConfigFormat) -> Result<AppConfig> {
        match format {
            ConfigFormat::Json => serde_json::from_str(content)
                .map_err(|e| ConfigError::ParseError(format!("JSON parse error: {}", e)).into()),
            ConfigFormat::Toml => toml::from_str(content)
                .map_err(|e| ConfigError::ParseError(format!("TOML parse error: {}", e)).into()),
        }
    }

    /// 保存配置到文件
    pub fn save_to_file<P: AsRef<Path>>(config: &AppConfig, path: P) -> Result<()> {
        let path = path.as_ref();
        let format = ConfigFormat::from_path(path)?;
        let content = Self::serialize_config(config, format)?;

        fs::write(path, content)
            .with_context(|| format!("Failed to write config file: {}", path.display()))?;

        Ok(())
    }

    /// 序列化配置
    pub fn serialize_config(config: &AppConfig, format: ConfigFormat) -> Result<String> {
        match format {
            ConfigFormat::Json => serde_json::to_string_pretty(config).map_err(|e| {
                ConfigError::WriteError(format!("JSON serialize error: {}", e)).into()
            }),
            ConfigFormat::Toml => toml::to_string_pretty(config).map_err(|e| {
                ConfigError::WriteError(format!("TOML serialize error: {}", e)).into()
            }),
        }
    }

    /// 创建默认配置文件
    pub fn create_default_config<P: AsRef<Path>>(path: P) -> Result<()> {
        let config = AppConfig::default();
        Self::save_to_file(&config, path)
    }

    /// 验证配置
    pub fn validate_config(config: &AppConfig) -> Result<()> {
        // 验证分辨率
        if config.camera.resolution.width == 0 || config.camera.resolution.height == 0 {
            return Err(anyhow::anyhow!("Invalid camera resolution"));
        }

        if config.ui.window_size.width == 0 || config.ui.window_size.height == 0 {
            return Err(anyhow::anyhow!("Invalid window size"));
        }

        // 验证帧率
        if config.camera.frame_rate == 0 || config.camera.frame_rate > 120 {
            return Err(anyhow::anyhow!(
                "Invalid frame rate: {}",
                config.camera.frame_rate
            ));
        }

        // 验证缓冲区大小
        if config.processing.buffer_size == 0 {
            return Err(anyhow::anyhow!("Buffer size must be greater than 0"));
        }

        // 验证相机发现配置
        Self::validate_camera_discovery_config(&config.camera.discovery)?;

        Ok(())
    }

    /// 验证相机发现配置
    pub fn validate_camera_discovery_config(config: &CameraDiscoveryConfig) -> Result<()> {
        // 验证搜索关键词不为空
        if config.search_keywords.is_empty() {
            return Err(anyhow::anyhow!("Search keywords cannot be empty"));
        }

        // 验证扫描设备数量限制
        if config.max_scan_devices == 0 {
            return Err(anyhow::anyhow!("Max scan devices must be greater than 0"));
        }

        // 验证扫描超时时间
        if config.scan_timeout_seconds == 0 {
            return Err(anyhow::anyhow!("Scan timeout must be greater than 0"));
        }

        // 验证优先级设备路径
        for device in &config.priority_devices {
            if device.device_path.is_empty() {
                return Err(anyhow::anyhow!("Priority device path cannot be empty"));
            }
            if device.name.is_empty() {
                return Err(anyhow::anyhow!("Priority device name cannot be empty"));
            }
        }

        // 验证设备验证配置
        Self::validate_device_validation_config(&config.validation)?;

        Ok(())
    }

    /// 验证设备验证配置
    pub fn validate_device_validation_config(config: &DeviceValidationConfig) -> Result<()> {
        // 验证分辨率范围
        if config.min_resolution.width == 0 || config.min_resolution.height == 0 {
            return Err(anyhow::anyhow!("Minimum resolution must be greater than 0"));
        }

        if config.max_resolution.width == 0 || config.max_resolution.height == 0 {
            return Err(anyhow::anyhow!("Maximum resolution must be greater than 0"));
        }

        if config.min_resolution.width > config.max_resolution.width
            || config.min_resolution.height > config.max_resolution.height
        {
            return Err(anyhow::anyhow!(
                "Minimum resolution cannot be greater than maximum resolution"
            ));
        }

        // 验证必需格式不为空（如果启用格式验证）
        if config.verify_supported_formats && config.required_formats.is_empty() {
            return Err(anyhow::anyhow!(
                "Required formats cannot be empty when format verification is enabled"
            ));
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::NamedTempFile;

    #[test]
    fn test_default_config() {
        let config = AppConfig::default();
        assert_eq!(config.camera.left_camera_keyword, "cameraL");
        assert_eq!(config.camera.right_camera_keyword, "cameraR");
        assert_eq!(config.camera.resolution.width, 1920);
        assert_eq!(config.camera.resolution.height, 1080);
    }

    #[test]
    fn test_json_serialization() {
        let config = AppConfig::default();
        let json_str = ConfigParser::serialize_config(&config, ConfigFormat::Json).unwrap();
        let parsed_config = ConfigParser::parse_config(&json_str, ConfigFormat::Json).unwrap();

        assert_eq!(
            config.camera.left_camera_keyword,
            parsed_config.camera.left_camera_keyword
        );
        assert_eq!(
            config.camera.resolution.width,
            parsed_config.camera.resolution.width
        );
    }

    #[test]
    fn test_toml_serialization() {
        let config = AppConfig::default();
        let toml_str = ConfigParser::serialize_config(&config, ConfigFormat::Toml).unwrap();
        let parsed_config = ConfigParser::parse_config(&toml_str, ConfigFormat::Toml).unwrap();

        assert_eq!(
            config.camera.left_camera_keyword,
            parsed_config.camera.left_camera_keyword
        );
        assert_eq!(
            config.camera.resolution.width,
            parsed_config.camera.resolution.width
        );
    }

    #[test]
    fn test_file_operations() {
        let config = AppConfig::default();

        // 测试JSON文件
        let json_file = NamedTempFile::with_suffix(".json").unwrap();
        ConfigParser::save_to_file(&config, json_file.path()).unwrap();
        let loaded_config = ConfigParser::load_from_file(json_file.path()).unwrap();
        assert_eq!(
            config.camera.left_camera_keyword,
            loaded_config.camera.left_camera_keyword
        );

        // 测试TOML文件
        let toml_file = NamedTempFile::with_suffix(".toml").unwrap();
        ConfigParser::save_to_file(&config, toml_file.path()).unwrap();
        let loaded_config = ConfigParser::load_from_file(toml_file.path()).unwrap();
        assert_eq!(
            config.camera.left_camera_keyword,
            loaded_config.camera.left_camera_keyword
        );
    }

    #[test]
    fn test_config_validation() {
        let mut config = AppConfig::default();

        // 有效配置应该通过验证
        assert!(ConfigParser::validate_config(&config).is_ok());

        // 无效分辨率
        config.camera.resolution.width = 0;
        assert!(ConfigParser::validate_config(&config).is_err());

        config.camera.resolution.width = 1920;
        config.camera.frame_rate = 0;
        assert!(ConfigParser::validate_config(&config).is_err());
    }

    #[test]
    fn test_camera_discovery_config() {
        let mut discovery_config = CameraDiscoveryConfig::default();

        // 测试添加搜索关键词
        discovery_config.add_search_keyword("test_camera".to_string());
        assert!(discovery_config
            .search_keywords
            .contains(&"test_camera".to_string()));

        // 测试移除搜索关键词
        discovery_config.remove_search_keyword("test_camera");
        assert!(!discovery_config
            .search_keywords
            .contains(&"test_camera".to_string()));

        // 测试关键词匹配
        assert!(discovery_config.matches_keyword("camera_device"));
        assert!(discovery_config.matches_keyword("USB Camera"));
        assert!(!discovery_config.matches_keyword("hdmirx_device"));

        // 测试优先级设备
        let priority_device = PriorityDevice::new(
            "测试相机".to_string(),
            "/dev/video99".to_string(),
            CameraRole::Left,
        );
        discovery_config.add_priority_device(priority_device);

        let left_devices = discovery_config.get_priority_devices_by_role(&CameraRole::Left);
        assert!(!left_devices.is_empty());
    }

    #[test]
    fn test_camera_discovery_validation() {
        let mut discovery_config = CameraDiscoveryConfig::default();

        // 有效配置应该通过验证
        assert!(ConfigParser::validate_camera_discovery_config(&discovery_config).is_ok());

        // 空搜索关键词应该失败
        discovery_config.search_keywords.clear();
        assert!(ConfigParser::validate_camera_discovery_config(&discovery_config).is_err());

        // 恢复搜索关键词
        discovery_config.search_keywords.push("camera".to_string());

        // 无效扫描设备数量
        discovery_config.max_scan_devices = 0;
        assert!(ConfigParser::validate_camera_discovery_config(&discovery_config).is_err());
    }
}
