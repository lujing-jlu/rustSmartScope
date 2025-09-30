//! 基础状态管理

use crate::config::AppConfig;
use std::sync::{Arc, RwLock};

/// 应用程序状态
#[derive(Debug, Clone)]
pub struct AppState {
    pub config: Arc<RwLock<AppConfig>>,
    pub initialized: bool,
}

impl AppState {
    pub fn new() -> Self {
        Self {
            config: Arc::new(RwLock::new(AppConfig::default())),
            initialized: false,
        }
    }

    pub fn initialize(&mut self) -> crate::Result<()> {
        self.initialized = true;
        tracing::info!("SmartScope core initialized");
        Ok(())
    }

    pub fn shutdown(&mut self) {
        self.initialized = false;
        tracing::info!("SmartScope core shutdown");
    }

    pub fn is_initialized(&self) -> bool {
        self.initialized
    }
}

impl Default for AppState {
    fn default() -> Self {
        Self::new()
    }
}