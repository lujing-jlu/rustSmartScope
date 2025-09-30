//! 基础配置管理

use serde::{Deserialize, Serialize};
use crate::error::{SmartScopeError, Result};
use std::path::Path;

/// 应用程序配置
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    pub name: String,
    pub version: String,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            name: "RustSmartScope".to_string(),
            version: "0.1.0".to_string(),
        }
    }
}

impl AppConfig {
    /// 从TOML文件加载配置
    pub fn load_from_file<P: AsRef<Path>>(path: P) -> Result<Self> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| SmartScopeError::Config(format!("读取配置文件失败: {}", e)))?;

        let config: AppConfig = toml::from_str(&content)
            .map_err(|e| SmartScopeError::Config(format!("解析配置文件失败: {}", e)))?;

        Ok(config)
    }

    /// 保存配置到TOML文件
    pub fn save_to_file<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let content = toml::to_string_pretty(self)
            .map_err(|e| SmartScopeError::Config(format!("序列化配置失败: {}", e)))?;

        std::fs::write(path, content)
            .map_err(|e| SmartScopeError::Config(format!("写入配置文件失败: {}", e)))?;

        Ok(())
    }
}