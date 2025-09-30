use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::Path;

/// 配置文件格式
#[derive(Debug, Clone)]
pub enum ConfigFormat {
    Json,
    Toml,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    pub camera: CameraConfig,
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

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            camera: CameraConfig {
                left: CameraSideConfig {
                    search_keywords: vec!["cameraL".to_string(), "left".to_string(), "camera".to_string()],
                    format: "MJPG".to_string(),
                    frame_rate: 30,
                    resolution: Resolution {
                        width: 1280,
                        height: 720,
                    },
                },
                right: CameraSideConfig {
                    search_keywords: vec!["cameraR".to_string(), "right".to_string(), "camera".to_string()],
                    format: "MJPG".to_string(),
                    frame_rate: 30,
                    resolution: Resolution {
                        width: 1280,
                        height: 720,
                    },
                },
            },
        }
    }
}

impl CameraSideConfig {
    /// 检查设备名称是否匹配搜索关键词
    pub fn matches_keyword(&self, device_name: &str) -> bool {
        let name = device_name.to_lowercase();
        
        for keyword in &self.search_keywords {
            let keyword_lower = keyword.to_lowercase();
            if name.contains(&keyword_lower) {
                return true;
            }
        }
        
        false
    }
    
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
}

/// 配置解析器
pub struct ConfigParser;

impl ConfigParser {
    /// 从文件加载配置
    pub fn load_from_file<P: AsRef<Path>>(path: P) -> Result<AppConfig> {
        let path = path.as_ref();
        let content = fs::read_to_string(path)
            .with_context(|| format!("Failed to read config file: {:?}", path))?;

        let config = match path.extension().and_then(|ext| ext.to_str()) {
            Some("json") => serde_json::from_str(&content)
                .with_context(|| "Failed to parse JSON config")?,
            Some("toml") => toml::from_str(&content)
                .with_context(|| "Failed to parse TOML config")?,
            _ => return Err(anyhow::anyhow!("Unsupported config file format")),
        };

        Ok(config)
    }

    /// 保存配置到文件
    pub fn save_to_file<P: AsRef<Path>>(config: &AppConfig, path: P) -> Result<()> {
        let path = path.as_ref();
        let content = match path.extension().and_then(|ext| ext.to_str()) {
            Some("json") => serde_json::to_string_pretty(config)
                .with_context(|| "Failed to serialize config to JSON")?,
            Some("toml") => toml::to_string_pretty(config)
                .with_context(|| "Failed to serialize config to TOML")?,
            _ => return Err(anyhow::anyhow!("Unsupported config file format")),
        };

        fs::write(path, content)
            .with_context(|| format!("Failed to write config file: {:?}", path))?;

        Ok(())
    }

    /// 序列化配置为字符串
    pub fn serialize_config(config: &AppConfig, format: ConfigFormat) -> Result<String> {
        match format {
            ConfigFormat::Json => serde_json::to_string_pretty(config)
                .with_context(|| "Failed to serialize config to JSON"),
            ConfigFormat::Toml => toml::to_string_pretty(config)
                .with_context(|| "Failed to serialize config to TOML"),
        }
    }

    /// 创建默认配置文件
    pub fn create_default_config<P: AsRef<Path>>(path: P) -> Result<AppConfig> {
        let config = AppConfig::default();
        Self::save_to_file(&config, path)?;
        Ok(config)
    }

    /// 验证配置
    pub fn validate_config(config: &AppConfig) -> Result<()> {
        // 验证左相机配置
        Self::validate_camera_side_config(&config.camera.left, "左相机")?;
        
        // 验证右相机配置
        Self::validate_camera_side_config(&config.camera.right, "右相机")?;

        Ok(())
    }

    /// 验证单侧相机配置
    fn validate_camera_side_config(config: &CameraSideConfig, side_name: &str) -> Result<()> {
        // 验证搜索关键词不为空
        if config.search_keywords.is_empty() {
            return Err(anyhow::anyhow!("{} 搜索关键词不能为空", side_name));
        }

        // 验证分辨率
        if config.resolution.width == 0 || config.resolution.height == 0 {
            return Err(anyhow::anyhow!("{} 分辨率无效", side_name));
        }

        // 验证帧率
        if config.frame_rate == 0 || config.frame_rate > 120 {
            return Err(anyhow::anyhow!("{} 帧率无效: {}", side_name, config.frame_rate));
        }

        // 验证格式
        if config.format.is_empty() {
            return Err(anyhow::anyhow!("{} 格式不能为空", side_name));
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_config() {
        let config = AppConfig::default();
        assert!(!config.camera.left.search_keywords.is_empty());
        assert!(!config.camera.right.search_keywords.is_empty());
        assert_eq!(config.camera.left.format, "MJPG");
        assert_eq!(config.camera.right.format, "MJPG");
    }

    #[test]
    fn test_keyword_matching() {
        let config = CameraSideConfig {
            search_keywords: vec!["camera".to_string(), "left".to_string()],
            format: "MJPG".to_string(),
            frame_rate: 30,
            resolution: Resolution { width: 1280, height: 720 },
        };

        assert!(config.matches_keyword("USB Camera"));
        assert!(config.matches_keyword("Left Camera"));
        assert!(config.matches_keyword("camera_device"));
        assert!(!config.matches_keyword("hdmirx"));
    }

    #[test]
    fn test_config_validation() {
        let config = AppConfig::default();
        assert!(ConfigParser::validate_config(&config).is_ok());

        let mut invalid_config = config.clone();
        invalid_config.camera.left.search_keywords.clear();
        assert!(ConfigParser::validate_config(&invalid_config).is_err());
    }

    #[test]
    fn test_add_remove_keywords() {
        let mut config = CameraSideConfig {
            search_keywords: vec!["camera".to_string()],
            format: "MJPG".to_string(),
            frame_rate: 30,
            resolution: Resolution { width: 1280, height: 720 },
        };

        config.add_search_keyword("test".to_string());
        assert!(config.search_keywords.contains(&"test".to_string()));

        config.remove_search_keyword("test");
        assert!(!config.search_keywords.contains(&"test".to_string()));
    }
}
