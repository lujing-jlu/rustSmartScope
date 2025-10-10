//! SmartScope状态管理系统
//!
//! 支持配置热重载、状态同步等功能

use crate::config::{SmartScopeConfig, PartialConfig};
use crate::camera::CameraManager;
use crate::video_transform::VideoTransformProcessor;
use crate::image_pipeline::ImagePipeline;
use std::sync::{Arc, RwLock, Mutex};
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
    pub video_transform: Arc<RwLock<VideoTransformProcessor>>,
    /// 畸变校正启用状态
    pub distortion_correction_enabled: Arc<RwLock<bool>>,
    /// 图像处理管道
    pub image_pipeline: Option<Arc<Mutex<ImagePipeline>>>,
}

impl Clone for AppState {
    fn clone(&self) -> Self {
        Self {
            config: Arc::clone(&self.config),
            initialized: self.initialized,
            config_watcher: None,  // Watcher不能clone，需要重新创建
            config_path: self.config_path.clone(),
            camera_manager: None,  // CameraManager不能clone，需要重新创建
            video_transform: Arc::clone(&self.video_transform),
            distortion_correction_enabled: Arc::clone(&self.distortion_correction_enabled),
            image_pipeline: self.image_pipeline.as_ref().map(Arc::clone),
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
            video_transform: Arc::new(RwLock::new(VideoTransformProcessor::new())),
            distortion_correction_enabled: Arc::new(RwLock::new(false)),
            image_pipeline: None,
        }
    }

    pub fn initialize(&mut self) -> crate::Result<()> {
        // 加载配置
        let config = SmartScopeConfig::load_with_fallback();

        // 验证配置
        config.validate()?;

        *self.config.write().unwrap() = config.clone();

        // 初始化图像处理管道
        let video_transform = Arc::clone(&self.video_transform);
        let camera_params_dir = Some("camera_parameters");

        match ImagePipeline::new(camera_params_dir, video_transform) {
            Ok(pipeline) => {
                self.image_pipeline = Some(Arc::new(Mutex::new(pipeline)));
                tracing::info!("图像处理管道初始化成功");
            }
            Err(e) => {
                tracing::warn!("图像处理管道初始化失败: {}, 将使用降级模式", e);
                self.image_pipeline = None;
            }
        }

        // 初始化相机管理器
        let mut camera_manager = CameraManager::new(config);
        camera_manager.initialize()?;
        self.camera_manager = Some(camera_manager);

        self.initialized = true;
        tracing::debug!("SmartScope core initialized");
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
        tracing::debug!("SmartScope core shutdown");
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
                                tracing::debug!("配置热重载成功");
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
            tracing::debug!("配置已更新并保存到文件: {}", path);
        }

        Ok(())
    }

    /// 重新加载配置
    pub fn reload_config(&self) -> crate::Result<()> {
        let new_config = SmartScopeConfig::load_with_fallback();
        new_config.validate()?;

        *self.config.write().unwrap() = new_config;
        tracing::debug!("配置重新加载成功");
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
            tracing::debug!("相机系统启动成功");
        }
        Ok(())
    }

    /// 停止相机系统
    pub fn stop_camera(&mut self) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.stop()?;
            tracing::debug!("相机系统停止成功");
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

    /// 获取处理后的左相机帧（应用畸变校正和视频变换）
    pub fn get_processed_left_frame(&self) -> Option<crate::camera::VideoFrame> {
        let raw_frame = self.camera_manager.as_ref()?.get_left_frame()?;

        // 如果有 ImagePipeline，使用它处理
        if let Some(ref pipeline) = self.image_pipeline {
            if let Ok(mut pipeline) = pipeline.lock() {
                // 同步畸变校正状态到 pipeline
                let distortion_enabled = *self.distortion_correction_enabled.read().unwrap();
                pipeline.set_distortion_correction_enabled(distortion_enabled);

                // 处理帧数据
                match pipeline.process_mjpeg_frame(&raw_frame.data, true) {
                    Ok((rgb_data, width, height)) => {
                        // 返回处理后的帧（RGB格式）
                        return Some(crate::camera::VideoFrame {
                            data: rgb_data,
                            width,
                            height,
                            format: 0, // RGB format
                            timestamp: raw_frame.timestamp,
                            camera_name: raw_frame.camera_name.clone(),
                            frame_id: raw_frame.frame_id,
                        });
                    }
                    Err(e) => {
                        tracing::error!("图像处理失败: {}", e);
                    }
                }
            }
        }

        // 降级：返回原始帧
        Some(raw_frame)
    }

    /// 获取处理后的右相机帧（应用畸变校正和视频变换）
    pub fn get_processed_right_frame(&self) -> Option<crate::camera::VideoFrame> {
        let raw_frame = self.camera_manager.as_ref()?.get_right_frame()?;

        // 如果有 ImagePipeline，使用它处理
        if let Some(ref pipeline) = self.image_pipeline {
            if let Ok(mut pipeline) = pipeline.lock() {
                // 同步畸变校正状态到 pipeline
                let distortion_enabled = *self.distortion_correction_enabled.read().unwrap();
                pipeline.set_distortion_correction_enabled(distortion_enabled);

                // 处理帧数据
                match pipeline.process_mjpeg_frame(&raw_frame.data, false) {
                    Ok((rgb_data, width, height)) => {
                        // 返回处理后的帧（RGB格式）
                        return Some(crate::camera::VideoFrame {
                            data: rgb_data,
                            width,
                            height,
                            format: 0, // RGB format
                            timestamp: raw_frame.timestamp,
                            camera_name: raw_frame.camera_name.clone(),
                            frame_id: raw_frame.frame_id,
                        });
                    }
                    Err(e) => {
                        tracing::error!("图像处理失败: {}", e);
                    }
                }
            }
        }

        // 降级：返回原始帧
        Some(raw_frame)
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

    // ============================================================================
    // 视频变换相关方法
    // ============================================================================

    /// 设置旋转角度
    pub fn video_set_rotation(&self, degrees: u32) -> crate::Result<()> {
        let mut transform = self.video_transform.write().unwrap();
        transform.get_config_mut().set_rotation(degrees);
        tracing::debug!("视频旋转设置为: {}°", degrees);
        Ok(())
    }

    /// 应用旋转（累加90度）
    pub fn video_apply_rotation(&self) -> crate::Result<()> {
        let mut transform = self.video_transform.write().unwrap();
        transform.get_config_mut().apply_rotation();
        let degrees = transform.get_config().rotation_degrees;
        tracing::debug!("视频旋转: {}°", degrees);
        Ok(())
    }

    /// 切换水平翻转
    pub fn video_toggle_flip_horizontal(&self) -> crate::Result<()> {
        let mut transform = self.video_transform.write().unwrap();
        transform.get_config_mut().toggle_flip_horizontal();
        let enabled = transform.get_config().flip_horizontal;
        tracing::debug!("水平翻转: {}", enabled);
        Ok(())
    }

    /// 切换垂直翻转
    pub fn video_toggle_flip_vertical(&self) -> crate::Result<()> {
        let mut transform = self.video_transform.write().unwrap();
        transform.get_config_mut().toggle_flip_vertical();
        let enabled = transform.get_config().flip_vertical;
        tracing::debug!("垂直翻转: {}", enabled);
        Ok(())
    }

    /// 切换反色
    pub fn video_toggle_invert(&self) -> crate::Result<()> {
        let mut transform = self.video_transform.write().unwrap();
        transform.get_config_mut().toggle_invert();
        let enabled = transform.get_config().invert_colors;
        tracing::debug!("反色: {}", enabled);
        Ok(())
    }

    /// 重置所有视频变换
    pub fn video_reset_transforms(&self) -> crate::Result<()> {
        let mut transform = self.video_transform.write().unwrap();
        transform.get_config_mut().reset();
        tracing::info!("视频变换已重置");
        Ok(())
    }

    /// 获取当前旋转角度
    pub fn video_get_rotation(&self) -> u32 {
        let transform = self.video_transform.read().unwrap();
        transform.get_config().rotation_degrees
    }

    /// 获取水平翻转状态
    pub fn video_get_flip_horizontal(&self) -> bool {
        let transform = self.video_transform.read().unwrap();
        transform.get_config().flip_horizontal
    }

    /// 获取垂直翻转状态
    pub fn video_get_flip_vertical(&self) -> bool {
        let transform = self.video_transform.read().unwrap();
        transform.get_config().flip_vertical
    }

    /// 获取反色状态
    pub fn video_get_invert(&self) -> bool {
        let transform = self.video_transform.read().unwrap();
        transform.get_config().invert_colors
    }

    /// 检查RGA硬件是否可用
    pub fn video_is_rga_available(&self) -> bool {
        let transform = self.video_transform.read().unwrap();
        transform.is_rga_available()
    }

    // ============================================================================
    // 畸变校正相关方法
    // ============================================================================

    /// 切换畸变校正
    pub fn toggle_distortion_correction(&self) -> crate::Result<()> {
        let mut enabled = self.distortion_correction_enabled.write().unwrap();
        *enabled = !*enabled;
        tracing::info!("畸变校正: {}", if *enabled { "启用" } else { "禁用" });
        Ok(())
    }

    /// 设置畸变校正状态
    pub fn set_distortion_correction(&self, enabled: bool) -> crate::Result<()> {
        *self.distortion_correction_enabled.write().unwrap() = enabled;
        tracing::debug!("畸变校正设置为: {}", enabled);
        Ok(())
    }

    /// 获取畸变校正状态
    pub fn get_distortion_correction(&self) -> bool {
        *self.distortion_correction_enabled.read().unwrap()
    }

    // ============================================================================
    // 相机参数控制相关方法
    // ============================================================================

    /// 设置左相机参数
    pub fn set_left_camera_parameter(&mut self, property: &crate::camera::CameraProperty, value: i32) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.set_left_camera_parameter(property, value)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 设置右相机参数
    pub fn set_right_camera_parameter(&mut self, property: &crate::camera::CameraProperty, value: i32) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.set_right_camera_parameter(property, value)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 设置单相机参数
    pub fn set_single_camera_parameter(&mut self, property: &crate::camera::CameraProperty, value: i32) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.set_single_camera_parameter(property, value)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 获取左相机参数
    pub fn get_left_camera_parameter(&mut self, property: &crate::camera::CameraProperty) -> crate::Result<i32> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.get_left_camera_parameter(property)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 获取右相机参数
    pub fn get_right_camera_parameter(&mut self, property: &crate::camera::CameraProperty) -> crate::Result<i32> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.get_right_camera_parameter(property)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 获取单相机参数
    pub fn get_single_camera_parameter(&mut self, property: &crate::camera::CameraProperty) -> crate::Result<i32> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.get_single_camera_parameter(property)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 获取左相机参数范围
    pub fn get_left_camera_parameter_range(&mut self, property: &crate::camera::CameraProperty) -> crate::Result<crate::camera::ParameterRange> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.get_left_camera_parameter_range(property)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 获取右相机参数范围
    pub fn get_right_camera_parameter_range(&mut self, property: &crate::camera::CameraProperty) -> crate::Result<crate::camera::ParameterRange> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.get_right_camera_parameter_range(property)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 获取单相机参数范围
    pub fn get_single_camera_parameter_range(&mut self, property: &crate::camera::CameraProperty) -> crate::Result<crate::camera::ParameterRange> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.get_single_camera_parameter_range(property)
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 重置左相机参数到默认值
    pub fn reset_left_camera_parameters(&mut self) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.reset_left_camera_parameters()
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 重置右相机参数到默认值
    pub fn reset_right_camera_parameters(&mut self) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.reset_right_camera_parameters()
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }

    /// 重置单相机参数到默认值
    pub fn reset_single_camera_parameters(&mut self) -> crate::Result<()> {
        if let Some(ref mut camera_manager) = self.camera_manager {
            camera_manager.reset_single_camera_parameters()
        } else {
            Err(crate::error::SmartScopeError::Unknown("相机管理器未初始化".to_string()))
        }
    }
}

impl Default for AppState {
    fn default() -> Self {
        Self::new()
    }
}