//! 相机管理模块
//!
//! 统一管理USB相机，提供左右相机的图像流

use std::sync::{Arc, Mutex};
use std::time::{SystemTime, Instant, Duration};
use std::process::Command;
use crate::error::{SmartScopeError, Result};
use crate::config::SmartScopeConfig;

// 重新导出USB相机相关类型
pub use usb_camera::{
    CameraStreamReader, CameraConfig, VideoFrame,
    V4L2DeviceManager, CameraResult, CameraError
};

// 注意：不需要重新导出本模块的类型，它们已经在模块中定义

/// 相机管理器 - 封装USB相机功能
pub struct CameraManager {
    /// 左相机流读取器
    left_reader: Option<CameraStreamReader>,
    /// 右相机流读取器
    right_reader: Option<CameraStreamReader>,
    /// 当前配置
    config: SmartScopeConfig,
    /// 运行状态
    running: bool,
    /// 最新的左相机帧
    left_frame: Arc<Mutex<Option<VideoFrame>>>,
    /// 最新的右相机帧
    right_frame: Arc<Mutex<Option<VideoFrame>>>,
    /// 缓存的相机连接状态
    cached_connection_status: Arc<Mutex<(bool, bool, Instant)>>, // (left, right, last_check_time)
}

impl CameraManager {
    /// 创建新的相机管理器
    pub fn new(config: SmartScopeConfig) -> Self {
        Self {
            left_reader: None,
            right_reader: None,
            config,
            running: false,
            left_frame: Arc::new(Mutex::new(None)),
            right_frame: Arc::new(Mutex::new(None)),
            // 初始状态：未连接，时间设为很久以前以触发首次检测
            cached_connection_status: Arc::new(Mutex::new((false, false, Instant::now() - Duration::from_secs(10)))),
        }
    }

    /// 检测可用的相机设备
    pub fn detect_cameras(&self) -> Result<Vec<DetectedCamera>> {
        tracing::info!("检测可用的相机设备...");

        let output = Command::new("v4l2-ctl")
            .arg("--list-devices")
            .output()
            .map_err(|e| SmartScopeError::Unknown(format!("执行v4l2-ctl失败: {}", e)))?;

        if !output.status.success() {
            return Err(SmartScopeError::Unknown(format!(
                "v4l2-ctl命令执行失败: {}",
                String::from_utf8_lossy(&output.stderr)
            )));
        }

        let output_str = String::from_utf8_lossy(&output.stdout);
        let cameras = self.parse_v4l2_output(&output_str)?;

        tracing::info!("检测到 {} 个相机设备", cameras.len());
        for camera in &cameras {
            tracing::info!("相机: {} 位于 {}", camera.name, camera.device_path);
        }

        Ok(cameras)
    }

    /// 解析v4l2-ctl输出
    fn parse_v4l2_output(&self, output: &str) -> Result<Vec<DetectedCamera>> {
        let mut cameras = Vec::new();
        let mut current_device: Option<DetectedCamera> = None;

        for line in output.lines() {
            let line = line.trim();

            if line.is_empty() {
                continue;
            }

            // 检查是否是设备名行（以':'结尾）
            if line.ends_with(':') {
                // 保存前一个设备
                if let Some(device) = current_device.take() {
                    // 跳过HDMI设备
                    if !device.name.to_lowercase().contains("hdmi")
                        && !device.name.to_lowercase().contains("rk_hdmirx")
                        && !device.device_path.is_empty() {
                        cameras.push(device);
                    }
                }

                // 解析设备信息
                let device_info = &line[..line.len() - 1]; // 移除末尾的':'
                if let Some((name, description)) = self.parse_device_info(device_info) {
                    // 跳过HDMI设备
                    if !name.to_lowercase().contains("hdmi")
                        && !name.to_lowercase().contains("rk_hdmirx") {
                        current_device = Some(DetectedCamera {
                            name: name.to_string(),
                            description: description.to_string(),
                            device_path: String::new(), // 稍后设置
                            is_left: self.is_left_camera(name),
                            is_right: self.is_right_camera(name),
                        });
                    }
                }
            }
            // 检查是否是设备路径行（以'/'开头）
            else if line.starts_with('/') && current_device.is_some() {
                // 只使用第一个video设备路径
                if line.contains("/video") {
                    if let Some(ref mut device) = current_device {
                        if device.device_path.is_empty() {
                            device.device_path = line.to_string();
                        }
                    }
                }
            }
        }

        // 不要忘记最后一个设备
        if let Some(device) = current_device {
            if !device.name.to_lowercase().contains("hdmi")
                && !device.name.to_lowercase().contains("rk_hdmirx")
                && !device.device_path.is_empty() {
                cameras.push(device);
            }
        }

        Ok(cameras)
    }

    /// 解析设备信息行
    fn parse_device_info<'a>(&self, info: &'a str) -> Option<(&'a str, &'a str)> {
        if let Some(colon_pos) = info.find(':') {
            let name = info[..colon_pos].trim();
            let description = info[colon_pos + 1..].trim();
            Some((name, description))
        } else {
            Some((info, ""))
        }
    }

    /// 初始化相机系统
    pub fn initialize(&mut self) -> Result<()> {
        tracing::info!("初始化相机管理器");

        // 首先检测可用的相机设备
        let detected_cameras = self.detect_cameras()?;

        if detected_cameras.is_empty() {
            return Err(SmartScopeError::Unknown("未检测到可用的相机设备".to_string()));
        }

        // 创建相机配置
        let camera_config = CameraConfig {
            width: self.config.camera.width,
            height: self.config.camera.height,
            framerate: self.config.camera.fps,
            pixel_format: "MJPG".to_string(),
            parameters: std::collections::HashMap::new(),
        };

        // 为左右相机创建流读取器
        for camera in detected_cameras {
            if camera.is_left {
                tracing::info!("创建左相机流读取器: {}", camera.device_path);
                let reader = CameraStreamReader::new(
                    &camera.device_path,
                    &camera.name,
                    camera_config.clone(),
                );
                self.left_reader = Some(reader);
            } else if camera.is_right {
                tracing::info!("创建右相机流读取器: {}", camera.device_path);
                let reader = CameraStreamReader::new(
                    &camera.device_path,
                    &camera.name,
                    camera_config.clone(),
                );
                self.right_reader = Some(reader);
            }
        }

        tracing::info!("相机管理器初始化完成");
        Ok(())
    }

    /// 启动相机
    pub fn start(&mut self) -> Result<()> {
        if self.running {
            return Ok(());
        }

        tracing::info!("启动相机系统");

        if self.left_reader.is_none() && self.right_reader.is_none() {
            self.initialize()?;
        }

        // 启动左相机
        if let Some(ref mut reader) = self.left_reader {
            reader.start().map_err(|e| {
                SmartScopeError::Unknown(format!("启动左相机失败: {}", e))
            })?;
            tracing::info!("左相机启动成功");
        }

        // 启动右相机
        if let Some(ref mut reader) = self.right_reader {
            reader.start().map_err(|e| {
                SmartScopeError::Unknown(format!("启动右相机失败: {}", e))
            })?;
            tracing::info!("右相机启动成功");
        }

        self.running = true;
        tracing::info!("相机系统启动成功");
        Ok(())
    }

    /// 停止相机
    pub fn stop(&mut self) -> Result<()> {
        if !self.running {
            return Ok(());
        }

        tracing::info!("停止相机系统");

        // 停止左相机
        if let Some(ref mut reader) = self.left_reader {
            reader.stop().map_err(|e| {
                SmartScopeError::Unknown(format!("停止左相机失败: {}", e))
            })?;
        }

        // 停止右相机
        if let Some(ref mut reader) = self.right_reader {
            reader.stop().map_err(|e| {
                SmartScopeError::Unknown(format!("停止右相机失败: {}", e))
            })?;
        }

        // 清理帧缓存
        if let Ok(mut left) = self.left_frame.lock() {
            *left = None;
        }
        if let Ok(mut right) = self.right_frame.lock() {
            *right = None;
        }

        self.running = false;
        tracing::info!("相机系统停止完成");
        Ok(())
    }

    /// 处理新帧数据
    pub fn process_frames(&mut self) {
        // 从左相机读取帧
        if let Some(ref mut reader) = self.left_reader {
            if let Ok(Some(frame)) = reader.read_frame() {
                if let Ok(mut left) = self.left_frame.lock() {
                    *left = Some(frame);
                }
            }
        }

        // 从右相机读取帧
        if let Some(ref mut reader) = self.right_reader {
            if let Ok(Some(frame)) = reader.read_frame() {
                if let Ok(mut right) = self.right_frame.lock() {
                    *right = Some(frame);
                }
            }
        }
    }

    /// 判断是否为左相机
    fn is_left_camera(&self, camera_name: &str) -> bool {
        let name_lower = camera_name.to_lowercase();
        self.config.camera.left.name_keywords.iter()
            .any(|keyword| name_lower.contains(&keyword.to_lowercase()))
    }

    /// 判断是否为右相机
    fn is_right_camera(&self, camera_name: &str) -> bool {
        let name_lower = camera_name.to_lowercase();
        self.config.camera.right.name_keywords.iter()
            .any(|keyword| name_lower.contains(&keyword.to_lowercase()))
    }

    /// 获取左相机最新帧
    pub fn get_left_frame(&self) -> Option<VideoFrame> {
        self.left_frame.lock().ok()?.clone()
    }

    /// 获取右相机最新帧
    pub fn get_right_frame(&self) -> Option<VideoFrame> {
        self.right_frame.lock().ok()?.clone()
    }

    /// 检查相机是否运行中
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// 获取相机状态信息
    pub fn get_status(&self) -> CameraStatus {
        // 实时检测相机连接状态
        let (left_connected, right_connected) = self.check_camera_connections();

        CameraStatus {
            running: self.running,
            left_camera_connected: left_connected,
            right_camera_connected: right_connected,
            last_left_frame_time: self.left_frame.lock().ok()
                .and_then(|f| f.as_ref().map(|frame| frame.timestamp)),
            last_right_frame_time: self.right_frame.lock().ok()
                .and_then(|f| f.as_ref().map(|frame| frame.timestamp)),
        }
    }

    /// 检查相机连接状态（带缓存，1秒更新一次）
    fn check_camera_connections(&self) -> (bool, bool) {
        const CACHE_DURATION: Duration = Duration::from_secs(1);

        if let Ok(mut cache) = self.cached_connection_status.lock() {
            let (left, right, last_check) = *cache;

            // 如果缓存还有效，直接返回
            if last_check.elapsed() < CACHE_DURATION {
                return (left, right);
            }

            // 缓存过期，重新检测
            match self.detect_cameras() {
                Ok(cameras) => {
                    let left_connected = cameras.iter().any(|c| c.is_left);
                    let right_connected = cameras.iter().any(|c| c.is_right);

                    // 更新缓存
                    *cache = (left_connected, right_connected, Instant::now());
                    (left_connected, right_connected)
                }
                Err(_) => {
                    // 检测失败，更新缓存为未连接
                    *cache = (false, false, Instant::now());
                    (false, false)
                }
            }
        } else {
            // 无法获取锁，返回未连接
            (false, false)
        }
    }
}

impl Drop for CameraManager {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}

/// 相机状态信息
#[derive(Debug, Clone)]
pub struct CameraStatus {
    pub running: bool,
    pub left_camera_connected: bool,
    pub right_camera_connected: bool,
    pub last_left_frame_time: Option<SystemTime>,
    pub last_right_frame_time: Option<SystemTime>,
}

/// 检测到的相机设备
#[derive(Debug, Clone)]
pub struct DetectedCamera {
    pub name: String,
    pub description: String,
    pub device_path: String,
    pub is_left: bool,
    pub is_right: bool,
}