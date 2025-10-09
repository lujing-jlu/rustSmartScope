//! SmartScope状态管理系统
//!
//! 支持配置热重载、状态同步等功能

use crate::config::{SmartScopeConfig, PartialConfig};
use crate::camera::CameraManager;
use std::sync::{Arc, RwLock};
use std::path::Path;
use notify::{Watcher, RecommendedWatcher, RecursiveMode, Event};
use std::sync::mpsc;

/// SmartScope应用状态
pub struct AppState {
    pub config: Arc<RwLock<SmartScopeConfig>>,
    pub initialized: bool,
    pub config_watcher: Option<RecommendedWatcher>,
    pub config_path: Option<String>,
    pub camera_manager: Option<CameraManager>,
}

impl Clone for AppState {
    fn clone(&self) -> Self {
        Self {
            config: Arc::clone(&self.config),
            initialized: self.initialized,
            config_watcher: None,  // Watcher不能clone，需要重新创建
            config_path: self.config_path.clone(),
            camera_manager: None,  // CameraManager不能clone，需要重新创建
        }
    }
}

impl AppState {
    pub fn new() -> Self {
        Self {
            config: Arc::new(RwLock::new(SmartScopeConfig::default())),
            initialized: false,
            config_watcher: None,
            config_path: None,
            camera_manager: None,
        }
    }

    pub fn initialize(&mut self) -> crate::Result<()> {
        // 加载配置
        let config = SmartScopeConfig::load_with_fallback();

        // 验证配置
        config.validate()?;

        *self.config.write().unwrap() = config.clone();

        // 初始化相机管理器
        let mut camera_manager = CameraManager::new(config);
        camera_manager.initialize()?;
        self.camera_manager = Some(camera_manager);

        self.initialized = true;
        tracing::info!("SmartScope core initialized");
        Ok(())
    }

    pub fn shutdown(&mut self) {
        // 停止相机管理器
        if let Some(mut camera_manager) = self.camera_manager.take() {
            let _ = camera_manager.stop();
        }

        // 停止配置文件监控
        if let Some(watcher) = self.config_watcher.take() {
            drop(watcher);
        }

        self.initialized = false;
        tracing::info!("SmartScope core shutdown");
    }

    /// 启用配置文件热重载
    pub fn enable_config_hot_reload<P: AsRef<Path>>(&mut self, config_path: P) -> crate::Result<()> {
        let path = config_path.as_ref().to_string_lossy().to_string();
        self.config_path = Some(path.clone());

        let (tx, rx) = mpsc::channel();
        let mut watcher = notify::recommended_watcher(tx)
            .map_err(|e| crate::error::SmartScopeError::Unknown(format!("创建文件监控失败: {}", e)))?;

        watcher.watch(config_path.as_ref(), RecursiveMode::NonRecursive)
            .map_err(|e| crate::error::SmartScopeError::Unknown(format!("监控配置文件失败: {}", e)))?;

        // 在后台处理文件变化事件
        let config_arc = Arc::clone(&self.config);
        let path_clone = path.clone();  // 为线程创建另一个副本
        std::thread::spawn(move || {
            while let Ok(event) = rx.recv() {
                if let Ok(Event { kind: notify::EventKind::Modify(_), .. }) = event {
                    tracing::info!("检测到配置文件变化，重新加载配置");

                    match SmartScopeConfig::load_from_file(&path_clone) {
                        Ok(new_config) => {
                            if let Err(e) = new_config.validate() {
                                tracing::error!("配置文件验证失败: {}", e);
                                continue;
                            }

                            if let Ok(mut config) = config_arc.write() {
                                *config = new_config;
                                tracing::info!("配置热重载成功");
                            }
                        }
                        Err(e) => {
                            tracing::error!("重新加载配置失败: {}", e);
                        }
                    }
                }
            }
        });

        self.config_watcher = Some(watcher);
        tracing::info!("配置文件热重载已启用: {}", path);
        Ok(())
    }

    /// 更新部分配置
    pub fn update_config(&self, partial: PartialConfig) -> crate::Result<()> {
        let mut config = self.config.write().unwrap();
        config.merge_from(partial)?;

        // 如果有配置文件路径，保存到文件
        if let Some(path) = &self.config_path {
            config.save_to_file(path)?;
            tracing::info!("配置已更新并保存到文件: {}", path);
        }

        Ok(())
    }

    /// 重新加载配置
    pub fn reload_config(&self) -> crate::Result<()> {
        let new_config = SmartScopeConfig::load_with_fallback();
        new_config.validate()?;

        *self.config.write().unwrap() = new_config;
        tracing::info!("配置重新加载成功");
        Ok(())
    }

    /// 获取配置的只读副本
    pub fn get_config(&self) -> SmartScopeConfig {
        self.config.read().unwrap().clone()
    }

    pub fn is_initialized(&self) -> bool {
        self.initialized
    }

    /// 启动相机系统
    pub fn start_camera(&mut self) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.start()?;
            tracing::info!("相机系统启动成功");
        }
        Ok(())
    }

    /// 停止相机系统
    pub fn stop_camera(&mut self) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.stop()?;
            tracing::info!("相机系统停止成功");
        }
        Ok(())
    }

    /// 处理相机帧数据
    pub fn process_camera_frames(&mut self) {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.process_frames();
        }
    }

    /// 获取左相机最新帧
    pub fn get_left_camera_frame(&self) -> Option<crate::camera::VideoFrame> {
        self.camera_manager.as_ref()?.get_left_frame()
    }

    /// 获取右相机最新帧
    pub fn get_right_camera_frame(&self) -> Option<crate::camera::VideoFrame> {
        self.camera_manager.as_ref()?.get_right_frame()
    }

    /// 获取单相机最新帧
    pub fn get_single_camera_frame(&self) -> Option<crate::camera::VideoFrame> {
        self.camera_manager.as_ref()?.get_single_frame()
    }

    /// 获取相机状态
    pub fn get_camera_status(&self) -> Option<crate::camera::CameraStatus> {
        self.camera_manager.as_ref().map(|cm| cm.get_status())
    }

    /// 获取当前相机模式
    pub fn get_camera_mode(&self) -> crate::camera::CameraMode {
        self.camera_manager.as_ref()
            .map(|cm| cm.get_camera_mode())
            .unwrap_or(crate::camera::CameraMode::NoCamera)
    }

    /// 检查相机是否运行中
    pub fn is_camera_running(&self) -> bool {
        self.camera_manager.as_ref()
            .map(|cm| cm.is_running())
            .unwrap_or(false)
    }
}

impl Default for AppState {
    fn default() -> Self {
        Self::new()
    }
}