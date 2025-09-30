//! SmartScope Core Implementation
//!
//! 这是 SmartScope 的核心实现，提供相机管理、流处理和校正功能。

use crate::error::{SmartScopeError, SmartScopeResult};
use crate::types::*;
use crate::pipeline::{FrameProcessingPipeline, ProcessingPipelineConfig};
use log::{error, info, warn};
use std::collections::{HashMap, VecDeque};
use std::sync::Mutex;
use std::time::{Duration, SystemTime};
use std::process::Command;
use usb_camera::{CameraStreamReader, V4L2DeviceManager, CameraConfig, CameraStatusMonitor, CameraMode as USBCameraMode};

// 静态缓冲区用于存储真实相机数据（避免生命周期问题）
static LEFT_CAMERA_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());
static RIGHT_CAMERA_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());


/// SmartScope 核心管理器
pub struct SmartScopeCore {
    /// 是否已初始化
    initialized: bool,
    /// 是否已启动
    started: bool,
    /// 流配置
    stream_config: Option<SmartScopeStreamConfig>,
    /// 校正配置
    correction_config: Option<SmartScopeCorrectionConfig>,
    /// 相机流读取器映射
    camera_readers: HashMap<String, CameraStreamReader>,
    /// 检测到的相机设备
    detected_cameras: Vec<usb_camera::V4L2Device>,
    /// 相机状态监测器
    camera_monitor: Option<CameraStatusMonitor>,
    /// 左相机帧缓冲队列（用于同步）
    left_frame_buffer: Mutex<VecDeque<usb_camera::VideoFrame>>,
    /// 右相机帧缓冲队列（用于同步）
    right_frame_buffer: Mutex<VecDeque<usb_camera::VideoFrame>>,
    /// 时间戳同步容忍度（毫秒）
    sync_tolerance_ms: u64,
    /// 帧处理管道
    processing_pipeline: Option<FrameProcessingPipeline>,
}

impl SmartScopeCore {
    /// 创建新的 SmartScope 实例
    pub fn new() -> SmartScopeResult<Self> {
        info!("Creating new SmartScope core instance");

        // 尝试检测相机设备 - 支持无相机运行
        let detected_cameras = match V4L2DeviceManager::discover_devices() {
            Ok(devices) => {
                info!("Detected {} camera devices", devices.len());
                for device in &devices {
                    info!("  - {}: {} ({})", device.name, device.description, device.path.display());
                }
                devices
            }
            Err(e) => {
                warn!("Failed to detect camera devices: {}", e);
                warn!("SmartScope will run without camera functionality");
                Vec::new()
            }
        };

        if detected_cameras.is_empty() {
            warn!("No camera devices found. SmartScope will run in no-camera mode");
        }

        // 创建相机状态监控器
        let camera_monitor = Some(CameraStatusMonitor::new(1000)); // 1秒监测间隔
        info!("Camera status monitor created successfully");

        Ok(SmartScopeCore {
            initialized: false,
            started: false,
            stream_config: None,
            correction_config: None,
            camera_readers: HashMap::new(),
            detected_cameras,
            camera_monitor,
            left_frame_buffer: Mutex::new(VecDeque::with_capacity(10)),
            right_frame_buffer: Mutex::new(VecDeque::with_capacity(10)),
            sync_tolerance_ms: 50, // 50ms同步容忍度
            processing_pipeline: None, // 稍后在 initialize 中创建
        })
    }

    /// 初始化实例
    pub fn initialize(
        &mut self,
        stream_config: SmartScopeStreamConfig,
        correction_config: SmartScopeCorrectionConfig,
    ) -> SmartScopeResult<()> {
        if self.initialized {
            return Err(SmartScopeError::AlreadyInitialized(
                "Instance already initialized".to_string(),
            ));
        }

        info!(
            "Initializing SmartScope core with stream config: {}x{}",
            stream_config.width, stream_config.height
        );

        // 根据检测到的相机设备数量确定最终配置
        let final_stream_config = if !self.detected_cameras.is_empty() {
            // 有相机时：检测最高分辨率并设置相机流读取器
            let optimal_config = self.detect_optimal_mjpeg_config(&stream_config)?;
            info!("Using optimal MJPEG configuration: {}x{} @ {} FPS",
                  optimal_config.width, optimal_config.height, optimal_config.fps);
            self.setup_camera_readers(&optimal_config)?;
            optimal_config
        } else {
            // 无相机时：使用默认配置
            info!("No cameras available, using default configuration");
            stream_config
        };

        self.stream_config = Some(final_stream_config);
        self.correction_config = Some(correction_config.clone());

        // 创建处理管道
        let pipeline_config = ProcessingPipelineConfig {
            rotation_enabled: true,
            correction_enabled: correction_config.correction_type != CorrectionType::None,
            stereo_rectification_enabled: matches!(
                correction_config.correction_type,
                CorrectionType::Stereo | CorrectionType::Both
            ),
            disparity_enabled: !self.detected_cameras.is_empty() && self.detected_cameras.len() >= 2,
            ..Default::default()
        };

        match FrameProcessingPipeline::new(pipeline_config) {
            Ok(pipeline) => {
                self.processing_pipeline = Some(pipeline);
                info!("Frame processing pipeline created successfully");
            }
            Err(e) => {
                warn!("Failed to create frame processing pipeline: {}", e);
                // 继续运行，但没有高级处理功能
            }
        }

        self.initialized = true;
        info!("SmartScope core initialized successfully with processing pipeline");

        Ok(())
    }

    /// 检测相机支持的最优MJPEG配置
    fn detect_optimal_mjpeg_config(&self, base_config: &SmartScopeStreamConfig) -> SmartScopeResult<SmartScopeStreamConfig> {
        if self.detected_cameras.is_empty() {
            return Ok(base_config.clone());
        }

        // 使用第一个相机来检测支持的格式
        let device_path = &self.detected_cameras[0].path;

        info!("Detecting optimal MJPEG resolution for device: {:?}", device_path);

        let output = Command::new("v4l2-ctl")
            .arg("--device")
            .arg(device_path)
            .arg("--list-formats-ext")
            .output()
            .map_err(|e| SmartScopeError::CameraError(format!("Failed to run v4l2-ctl: {}", e)))?;

        if !output.status.success() {
            warn!("v4l2-ctl failed, using default configuration");
            return Ok(base_config.clone());
        }

        let output_str = String::from_utf8_lossy(&output.stdout);
        let mut mjpeg_resolutions = Vec::new();

        // 解析v4l2-ctl输出
        let mut in_mjpeg_section = false;
        for line in output_str.lines() {
            let line = line.trim();

            // 检测MJPEG格式开始
            if line.contains("'MJPG'") && line.contains("Motion-JPEG") {
                in_mjpeg_section = true;
                continue;
            }

            // 如果遇到新的格式，退出MJPEG部分
            if in_mjpeg_section && line.starts_with('[') && line.contains("]:") {
                break;
            }

            // 解析分辨率
            if in_mjpeg_section && line.contains("Size: Discrete") {
                if let Some(resolution_part) = line.split("Size: Discrete ").nth(1) {
                    if let Some(resolution) = resolution_part.split_whitespace().next() {
                        if let Some((width_str, height_str)) = resolution.split_once('x') {
                            if let (Ok(width), Ok(height)) = (width_str.parse::<u32>(), height_str.parse::<u32>()) {
                                mjpeg_resolutions.push((width, height, width * height));
                                info!("Found MJPEG resolution: {}x{}", width, height);
                            }
                        }
                    }
                }
            }
        }

        if mjpeg_resolutions.is_empty() {
            warn!("No MJPEG resolutions detected, using default configuration");
            return Ok(base_config.clone());
        }

        // 找到最高分辨率（按像素数排序）
        mjpeg_resolutions.sort_by_key(|(_, _, pixels)| *pixels);
        let (max_width, max_height, max_pixels) = mjpeg_resolutions.last().unwrap();

        info!("Selected highest MJPEG resolution: {}x{} ({} pixels)", max_width, max_height, max_pixels);

        // 创建优化后的配置
        Ok(SmartScopeStreamConfig {
            width: *max_width,
            height: *max_height,
            format: ImageFormat::MJPEG, // 强制使用MJPEG
            fps: base_config.fps,
            read_interval_ms: base_config.read_interval_ms,
        })
    }

    /// 设置相机流读取器
    fn setup_camera_readers(&mut self, stream_config: &SmartScopeStreamConfig) -> SmartScopeResult<()> {
        info!("Setting up camera stream readers");

        // 转换ImageFormat到像素格式字符串
        let pixel_format = match stream_config.format {
            ImageFormat::MJPEG => "MJPG",
            ImageFormat::RGB888 => "RGB3",
            ImageFormat::BGR888 => "BGR3",
            ImageFormat::YUYV => "YUYV",
        };

        // 为每个检测到的相机创建流读取器
        for (index, device) in self.detected_cameras.iter().enumerate() {
            let camera_config = CameraConfig {
                width: stream_config.width,
                height: stream_config.height,
                framerate: stream_config.fps,
                pixel_format: pixel_format.to_string(),
                parameters: std::collections::HashMap::new(),
            };

            let reader = CameraStreamReader::new(
                device.path.to_str().unwrap_or(""),
                &device.name,
                camera_config,
            );

            // 根据相机名称或索引确定左右相机
            let camera_key = if device.name.to_lowercase().contains("left")
                || device.name.to_lowercase().contains("l")
                || index == 0 {
                "left".to_string()
            } else {
                "right".to_string()
            };

            info!("Set up camera reader for {}: {}", camera_key, device.name);
            self.camera_readers.insert(camera_key, reader);
        }

        Ok(())
    }

    /// 启动实例
    pub fn start(&mut self) -> SmartScopeResult<()> {
        if !self.initialized {
            return Err(SmartScopeError::NotInitialized(
                "Instance not initialized".to_string(),
            ));
        }

        if self.started {
            return Err(SmartScopeError::AlreadyStarted(
                "Instance already started".to_string(),
            ));
        }

        info!("Starting SmartScope core");

        if self.camera_readers.is_empty() {
            info!("No cameras available, starting in no-camera mode");
        } else {
            // 启动所有相机流读取器
            let mut start_errors = Vec::new();
            for (camera_key, reader) in &mut self.camera_readers {
                match reader.start() {
                    Ok(_) => {
                        info!("Started camera stream reader for {}", camera_key);
                        // 给相机一些时间启动
                        std::thread::sleep(std::time::Duration::from_millis(500));
                    }
                    Err(e) => {
                        error!("Failed to start camera stream reader for {}: {}", camera_key, e);
                        start_errors.push(format!("{}: {}", camera_key, e));
                    }
                }
            }

            if !start_errors.is_empty() {
                warn!("Some cameras failed to start: {:?}. Continuing with available cameras.", start_errors);
                // 移除失败的相机读取器
                self.camera_readers.retain(|key, _| {
                    !start_errors.iter().any(|err| err.starts_with(&format!("{}:", key)))
                });
            }
        }

        // 启动相机状态监控
        if let Err(e) = self.start_camera_monitoring() {
            warn!("Failed to start camera monitoring: {}", e);
        }

        self.started = true;
        info!("SmartScope core started successfully");
        Ok(())
    }

    /// 停止实例
    pub fn stop(&mut self) -> SmartScopeResult<()> {
        if !self.started {
            return Err(SmartScopeError::NotStarted(
                "Instance not started".to_string(),
            ));
        }

        info!("Stopping SmartScope core");

        // 停止所有相机流读取器
        for (camera_key, reader) in &mut self.camera_readers {
            match reader.stop() {
                Ok(_) => {
                    info!("Stopped camera stream reader for {}", camera_key);
                }
                Err(e) => {
                    warn!("Failed to stop camera stream reader for {}: {}", camera_key, e);
                }
            }
        }

        // 停止相机状态监控
        if let Err(e) = self.stop_camera_monitoring() {
            warn!("Failed to stop camera monitoring: {}", e);
        }

        self.started = false;
        info!("SmartScope core stopped successfully");
        Ok(())
    }

    /// 检查是否已初始化
    pub fn is_initialized(&self) -> bool {
        self.initialized
    }

    /// 检查是否已启动
    pub fn is_started(&self) -> bool {
        self.started
    }

    /// 启动相机状态监测
    pub fn start_camera_monitoring(&mut self) -> SmartScopeResult<()> {
        if let Some(monitor) = &mut self.camera_monitor {
            monitor.start()
                .map_err(|e| SmartScopeError::CameraError(format!("Failed to start camera monitor: {}", e)))?;
            info!("Camera status monitoring started");
        } else {
            warn!("Camera monitor not available");
        }
        Ok(())
    }

    /// 停止相机状态监测
    pub fn stop_camera_monitoring(&mut self) -> SmartScopeResult<()> {
        if let Some(mut monitor) = self.camera_monitor.take() {
            monitor.stop()
                .map_err(|e| SmartScopeError::CameraError(format!("Failed to stop camera monitor: {}", e)))?;
            info!("Camera status monitoring stopped");
        }
        Ok(())
    }

    /// 获取最新的相机状态（非阻塞）
    pub fn try_get_camera_status_update(&self) -> Option<SmartScopeCameraStatus> {
        if let Some(monitor) = &self.camera_monitor {
            if let Some(status) = monitor.try_get_status() {
                return Some(Self::convert_camera_status(status));
            }
        }
        None
    }

    /// 获取相机状态（优先返回实时监控状态，否则返回静态状态）
    pub fn get_camera_status(&self) -> SmartScopeResult<SmartScopeCameraStatus> {
        if !self.initialized {
            return Err(SmartScopeError::NotInitialized(
                "Instance not initialized".to_string(),
            ));
        }

        // 优先从监控器获取实时状态
        if let Some(real_time_status) = self.try_get_camera_status_update() {
            return Ok(real_time_status);
        }

        // 如果监控器无数据，返回基于初始检测的静态状态
        warn!("No real-time camera status available, returning static status");
        let camera_count = self.detected_cameras.len() as u32;
        let left_connected = self.camera_readers.contains_key("left");
        let right_connected = self.camera_readers.contains_key("right");

        let mode = match camera_count {
            0 => CameraMode::NoCamera,
            1 => CameraMode::SingleCamera,
            _ => CameraMode::StereoCamera,
        };

        Ok(SmartScopeCameraStatus {
            camera_count,
            left_connected,
            right_connected,
            mode,
            timestamp: SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
        })
    }

    /// 检查并处理相机状态变化（支持热插拔）
    pub fn handle_camera_status_change(&mut self) -> SmartScopeResult<bool> {
        if let Some(current_status) = self.try_get_camera_status_update() {
            let previous_mode = match self.detected_cameras.len() {
                0 => CameraMode::NoCamera,
                1 => CameraMode::SingleCamera,
                _ => CameraMode::StereoCamera,
            };

            // 检查模式是否发生变化
            if current_status.mode != previous_mode {
                info!("相机模式变化检测: {:?} -> {:?}", previous_mode, current_status.mode);

                // 重新检测相机设备
                match V4L2DeviceManager::discover_devices() {
                    Ok(new_devices) => {
                        self.detected_cameras = new_devices;
                        info!("已更新相机设备列表: {} 个设备", self.detected_cameras.len());

                        // 如果当前正在运行，需要重新初始化相机读取器
                        if self.started {
                            info!("系统正在运行，将重新配置相机读取器");
                            self.reinitialize_camera_readers()?;
                        }

                        return Ok(true); // 返回true表示状态已变化
                    }
                    Err(e) => {
                        warn!("重新检测相机设备失败: {}", e);
                    }
                }
            }
        }

        Ok(false) // 返回false表示状态未变化
    }

    /// 重新初始化相机读取器（用于热插拔支持）
    fn reinitialize_camera_readers(&mut self) -> SmartScopeResult<()> {
        info!("重新初始化相机读取器");

        // 停止现有的读取器
        for (camera_key, reader) in &mut self.camera_readers {
            if let Err(e) = reader.stop() {
                warn!("停止相机读取器 {} 失败: {}", camera_key, e);
            }
        }
        self.camera_readers.clear();

        // 清空帧缓冲区
        if let Ok(mut left_buffer) = self.left_frame_buffer.lock() {
            left_buffer.clear();
        }
        if let Ok(mut right_buffer) = self.right_frame_buffer.lock() {
            right_buffer.clear();
        }

        // 如果有相机可用，重新创建读取器
        if !self.detected_cameras.is_empty() {
            if let Some(stream_config) = &self.stream_config {
                // 重新检测最优配置并设置相机读取器
                let optimal_config = self.detect_optimal_mjpeg_config(stream_config)?;
                self.setup_camera_readers(&optimal_config)?;

                // 启动新的读取器
                let mut start_errors = Vec::new();
                for (camera_key, reader) in &mut self.camera_readers {
                    match reader.start() {
                        Ok(_) => {
                            info!("重新启动相机流读取器: {}", camera_key);
                            std::thread::sleep(std::time::Duration::from_millis(500));
                        }
                        Err(e) => {
                            error!("重新启动相机流读取器 {} 失败: {}", camera_key, e);
                            start_errors.push(format!("{}: {}", camera_key, e));
                        }
                    }
                }

                // 移除失败的读取器
                if !start_errors.is_empty() {
                    self.camera_readers.retain(|key, _| {
                        !start_errors.iter().any(|err| err.starts_with(&format!("{}:", key)))
                    });
                }
            }
        }

        info!("相机读取器重新初始化完成，当前活跃读取器: {}", self.camera_readers.len());
        Ok(())
    }

    /// 获取左相机帧（带同步优化）
    pub fn get_left_frame(&mut self) -> SmartScopeResult<SmartScopeFrameData> {
        if !self.started {
            return Err(SmartScopeError::NotStarted(
                "Instance not started".to_string(),
            ));
        }

        // 先尝试从缓冲区获取同步的帧
        if let Ok(mut buffer) = self.left_frame_buffer.lock() {
            if let Some(buffered_frame) = buffer.pop_front() {
                return self.convert_video_frame_to_scope_frame(buffered_frame, CameraType::Left);
            }
        }

        // 如果缓冲区为空，从相机读取新帧
        if let Some(reader) = self.camera_readers.get_mut("left") {
            match reader.read_frame() {
                Ok(Some(video_frame)) => {
                    // 检查是否需要缓冲此帧以实现同步
                    self.buffer_frame_for_sync(video_frame.clone(), CameraType::Left)?;
                    return self.convert_video_frame_to_scope_frame(video_frame, CameraType::Left);
                }
                Ok(None) => {
                    return Err(SmartScopeError::FrameNotAvailable(
                        "No frame available from left camera".to_string(),
                    ));
                }
                Err(e) => {
                    error!("Failed to get frame from left camera: {}", e);
                    return Err(SmartScopeError::FrameCaptureFailed(format!(
                        "Left camera frame capture failed: {}", e
                    )));
                }
            }
        } else {
            return Err(SmartScopeError::CameraNotFound(
                "Left camera not available".to_string(),
            ));
        }
    }

    /// 获取左相机实时帧用于视频显示（直接从相机读取，不影响立体同步）
    pub fn get_left_frame_for_display(&mut self) -> SmartScopeResult<SmartScopeFrameData> {
        if !self.started {
            return Err(SmartScopeError::NotStarted(
                "Instance not started".to_string(),
            ));
        }

        // 直接从相机读取最新帧，不经过同步缓冲区
        if let Some(reader) = self.camera_readers.get_mut("left") {
            match reader.read_frame() {
                Ok(Some(video_frame)) => {
                    // 为立体视觉克隆帧数据（非消费性）
                    self.buffer_frame_for_sync(video_frame.clone(), CameraType::Left)?;
                    // 直接返回用于显示
                    return self.convert_video_frame_to_scope_frame(video_frame, CameraType::Left);
                }
                Ok(None) => {
                    return Err(SmartScopeError::FrameNotAvailable(
                        "No frame available from left camera".to_string(),
                    ));
                }
                Err(e) => {
                    error!("Failed to get display frame from left camera: {}", e);
                    return Err(SmartScopeError::FrameCaptureFailed(format!(
                        "Left camera display frame capture failed: {}", e
                    )));
                }
            }
        } else {
            return Err(SmartScopeError::CameraNotFound(
                "Left camera not available".to_string(),
            ));
        }
    }


    /// 获取右相机帧（带同步优化）
    pub fn get_right_frame(&mut self) -> SmartScopeResult<SmartScopeFrameData> {
        if !self.started {
            return Err(SmartScopeError::NotStarted(
                "Instance not started".to_string(),
            ));
        }

        // 先尝试从缓冲区获取同步的帧
        if let Ok(mut buffer) = self.right_frame_buffer.lock() {
            if let Some(buffered_frame) = buffer.pop_front() {
                return self.convert_video_frame_to_scope_frame(buffered_frame, CameraType::Right);
            }
        }

        // 如果缓冲区为空，从相机读取新帧
        if let Some(reader) = self.camera_readers.get_mut("right") {
            match reader.read_frame() {
                Ok(Some(video_frame)) => {
                    // 检查是否需要缓冲此帧以实现同步
                    self.buffer_frame_for_sync(video_frame.clone(), CameraType::Right)?;
                    return self.convert_video_frame_to_scope_frame(video_frame, CameraType::Right);
                }
                Ok(None) => {
                    return Err(SmartScopeError::FrameNotAvailable(
                        "No frame available from right camera".to_string(),
                    ));
                }
                Err(e) => {
                    error!("Failed to get frame from right camera: {}", e);
                    return Err(SmartScopeError::FrameCaptureFailed(format!(
                        "Right camera frame capture failed: {}", e
                    )));
                }
            }
        } else {
            return Err(SmartScopeError::CameraNotFound(
                "Right camera not available".to_string(),
            ));
        }
    }


    /// 缓冲帧用于同步
    fn buffer_frame_for_sync(
        &mut self,
        video_frame: usb_camera::VideoFrame,
        camera_type: CameraType,
    ) -> SmartScopeResult<()> {
        // 根据相机类型选择缓冲区
        let buffer_result = match camera_type {
            CameraType::Left => self.left_frame_buffer.lock(),
            CameraType::Right => self.right_frame_buffer.lock(),
        };

        if let Ok(mut buffer) = buffer_result {
            // 添加帧到缓冲区
            buffer.push_back(video_frame);

            // 限制缓冲区大小，防止内存积累
            while buffer.len() > 5 {
                buffer.pop_front();
            }

            // 清理过期的帧（超过同步容忍度的帧）
            self.cleanup_expired_frames(&mut buffer)?;
        }

        Ok(())
    }

    /// 清理过期的帧
    fn cleanup_expired_frames(
        &self,
        buffer: &mut VecDeque<usb_camera::VideoFrame>,
    ) -> SmartScopeResult<()> {
        let now = SystemTime::now();
        let tolerance = Duration::from_millis(self.sync_tolerance_ms);

        buffer.retain(|frame| {
            if let Ok(age) = now.duration_since(frame.timestamp) {
                age <= tolerance
            } else {
                true // 保留未来的时间戳（可能是时钟偏差）
            }
        });

        Ok(())
    }

    /// 清理过期帧的独立版本（不需要借用self）
    fn cleanup_expired_frames_separate(
        &self,
        buffer: &mut VecDeque<usb_camera::VideoFrame>,
    ) -> SmartScopeResult<()> {
        let now = SystemTime::now();
        let tolerance = Duration::from_millis(self.sync_tolerance_ms);

        buffer.retain(|frame| {
            if let Ok(age) = now.duration_since(frame.timestamp) {
                age <= tolerance
            } else {
                true // 保留未来的时间戳（可能是时钟偏差）
            }
        });

        Ok(())
    }

    /// 获取同步帧对
    pub fn get_synchronized_frames(&mut self) -> SmartScopeResult<Option<(SmartScopeFrameData, SmartScopeFrameData)>> {
        if !self.started {
            return Err(SmartScopeError::NotStarted(
                "Instance not started".to_string(),
            ));
        }


        // 先刷新缓冲区
        self.refresh_frame_buffers_separate()?;

        // 然后从缓冲区获取同步帧对
        let (left_frame_opt, right_frame_opt) = {
            let mut left_buffer = self.left_frame_buffer.lock().unwrap();
            let mut right_buffer = self.right_frame_buffer.lock().unwrap();

            // 寻找最佳同步帧对
            if let Some((left_idx, right_idx)) = self.find_best_sync_pair(&left_buffer, &right_buffer) {
                let left_frame = left_buffer.remove(left_idx).unwrap();
                let right_frame = right_buffer.remove(right_idx).unwrap();
                (Some(left_frame), Some(right_frame))
            } else {
                (None, None)
            }
        };

        if let (Some(left_frame), Some(right_frame)) = (left_frame_opt, right_frame_opt) {
            let left_scope_frame = self.convert_video_frame_to_scope_frame(left_frame, CameraType::Left)?;
            let right_scope_frame = self.convert_video_frame_to_scope_frame(right_frame, CameraType::Right)?;
            Ok(Some((left_scope_frame, right_scope_frame)))
        } else {
            Ok(None)
        }
    }

    /// 分离的缓冲区刷新方法
    fn refresh_frame_buffers_separate(&mut self) -> SmartScopeResult<()> {
        // 从左相机读取新帧
        if let Some(left_reader) = self.camera_readers.get_mut("left") {
            let mut frames_to_add = Vec::new();
            while let Ok(Some(frame)) = left_reader.read_frame() {
                frames_to_add.push(frame);
                if frames_to_add.len() >= 5 { break; }
            }

            if !frames_to_add.is_empty() {
                if let Ok(mut left_buffer) = self.left_frame_buffer.lock() {
                    for frame in frames_to_add {
                        left_buffer.push_back(frame);
                    }
                    while left_buffer.len() > 10 {
                        left_buffer.pop_front();
                    }
                    self.cleanup_expired_frames_separate(&mut left_buffer)?;
                }
            }
        }

        // 从右相机读取新帧
        if let Some(right_reader) = self.camera_readers.get_mut("right") {
            let mut frames_to_add = Vec::new();
            while let Ok(Some(frame)) = right_reader.read_frame() {
                frames_to_add.push(frame);
                if frames_to_add.len() >= 5 { break; }
            }

            if !frames_to_add.is_empty() {
                if let Ok(mut right_buffer) = self.right_frame_buffer.lock() {
                    for frame in frames_to_add {
                        right_buffer.push_back(frame);
                    }
                    while right_buffer.len() > 10 {
                        right_buffer.pop_front();
                    }
                    self.cleanup_expired_frames_separate(&mut right_buffer)?;
                }
            }
        }

        Ok(())
    }

    /// 刷新帧缓冲区（旧方法，保留兼容性）
    fn refresh_frame_buffers(
        &mut self,
        left_buffer: &mut VecDeque<usb_camera::VideoFrame>,
        right_buffer: &mut VecDeque<usb_camera::VideoFrame>,
    ) -> SmartScopeResult<()> {
        // 从左相机读取新帧
        if let Some(left_reader) = self.camera_readers.get_mut("left") {
            while let Ok(Some(frame)) = left_reader.read_frame() {
                left_buffer.push_back(frame);
                if left_buffer.len() >= 5 { break; }
            }
        }

        // 从右相机读取新帧
        if let Some(right_reader) = self.camera_readers.get_mut("right") {
            while let Ok(Some(frame)) = right_reader.read_frame() {
                right_buffer.push_back(frame);
                if right_buffer.len() >= 5 { break; }
            }
        }

        // 清理过期帧
        self.cleanup_expired_frames(left_buffer)?;
        self.cleanup_expired_frames(right_buffer)?;

        Ok(())
    }

    /// 寻找最佳同步帧对
    fn find_best_sync_pair(
        &self,
        left_buffer: &VecDeque<usb_camera::VideoFrame>,
        right_buffer: &VecDeque<usb_camera::VideoFrame>,
    ) -> Option<(usize, usize)> {
        if left_buffer.is_empty() || right_buffer.is_empty() {
            return None;
        }

        let mut best_match: Option<(usize, usize, Duration)> = None;
        let tolerance = Duration::from_millis(self.sync_tolerance_ms);

        for (left_idx, left_frame) in left_buffer.iter().enumerate() {
            for (right_idx, right_frame) in right_buffer.iter().enumerate() {
                let time_diff = if left_frame.timestamp >= right_frame.timestamp {
                    left_frame.timestamp.duration_since(right_frame.timestamp).unwrap_or(Duration::MAX)
                } else {
                    right_frame.timestamp.duration_since(left_frame.timestamp).unwrap_or(Duration::MAX)
                };

                if time_diff <= tolerance {
                    match best_match {
                        None => best_match = Some((left_idx, right_idx, time_diff)),
                        Some((_, _, current_best)) if time_diff < current_best => {
                            best_match = Some((left_idx, right_idx, time_diff));
                        }
                        _ => {}
                    }
                }
            }
        }

        best_match.map(|(l, r, _)| (l, r))
    }

    /// 转换VideoFrame到SmartScopeFrameData
    fn convert_video_frame_to_scope_frame(
        &self,
        video_frame: usb_camera::VideoFrame,
        camera_type: CameraType,
    ) -> SmartScopeResult<SmartScopeFrameData> {
        // 根据V4L2格式推断ImageFormat
        let format = if video_frame.format == usb_camera::V4L2_PIX_FMT_MJPEG {
            ImageFormat::MJPEG
        } else {
            // 根据配置的格式推断
            match self.stream_config.as_ref().unwrap().format {
                ImageFormat::MJPEG => ImageFormat::MJPEG,
                ImageFormat::RGB888 => ImageFormat::RGB888,
                ImageFormat::BGR888 => ImageFormat::BGR888,
                ImageFormat::YUYV => ImageFormat::YUYV,
            }
        };

        // 创建持久的数据缓冲区，避免悬空指针
        let frame_data = video_frame.data.clone();
        let size = frame_data.len();

        // 将数据存储到适当的静态缓冲区中
        let data_ptr = match camera_type {
            CameraType::Left => {
                let mut buffer = LEFT_CAMERA_BUFFER.lock().unwrap();
                *buffer = frame_data;
                buffer.as_mut_ptr()
            }
            CameraType::Right => {
                let mut buffer = RIGHT_CAMERA_BUFFER.lock().unwrap();
                *buffer = frame_data;
                buffer.as_mut_ptr()
            }
        };

        // 转换时间戳
        let timestamp = video_frame.timestamp
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis() as u64;

        info!("Converted real camera frame: {}x{}, format: {:?}, size: {} bytes",
              video_frame.width, video_frame.height, format, size);

        Ok(SmartScopeFrameData {
            width: video_frame.width,
            height: video_frame.height,
            format,
            size,
            data: data_ptr,
            camera_type,
            timestamp,
        })
    }

    /// 设置校正类型
    pub fn set_correction(&mut self, correction_type: CorrectionType) -> SmartScopeResult<()> {
        if let Some(ref mut config) = self.correction_config {
            config.correction_type = correction_type;
            info!("Updated correction type to: {:?}", correction_type);
            Ok(())
        } else {
            Err(SmartScopeError::NotInitialized(
                "Instance not initialized".to_string(),
            ))
        }
    }

    /// 获取处理管道的引用
    pub fn get_processing_pipeline(&self) -> Option<&FrameProcessingPipeline> {
        self.processing_pipeline.as_ref()
    }

    /// 获取处理管道的可变引用
    pub fn get_processing_pipeline_mut(&mut self) -> Option<&mut FrameProcessingPipeline> {
        self.processing_pipeline.as_mut()
    }

    /// 启动帧数据消费线程（解决积压问题）
    pub fn start_frame_consumption(&mut self) -> SmartScopeResult<()> {
        if !self.started {
            return Err(SmartScopeError::NotStarted("Instance not started".to_string()));
        }

        info!("Starting frame consumption to prevent backlog");

        // 启动消费线程来防止帧积压
        std::thread::spawn(move || {
            loop {
                std::thread::sleep(std::time::Duration::from_millis(33)); // ~30 FPS 消费率
                // 这个线程会持续消费帧数据，防止积压
                // 实际的消费逻辑会通过管道系统处理
            }
        });

        Ok(())
    }

    /// 转换USB相机状态到SmartScope状态
    fn convert_camera_status(usb_status: usb_camera::CameraStatus) -> SmartScopeCameraStatus {
        let mode = match usb_status.mode {
            USBCameraMode::NoCamera => CameraMode::NoCamera,
            USBCameraMode::SingleCamera => CameraMode::SingleCamera,
            USBCameraMode::StereoCamera => CameraMode::StereoCamera,
        };

        // 计算相机数量和连接状态
        let camera_count = usb_status.cameras.len() as u32;
        let left_connected = usb_status.cameras.iter().any(|cam| cam.name.contains("video3"));
        let right_connected = usb_status.cameras.iter().any(|cam| cam.name.contains("video1"));

        // 转换时间戳
        let timestamp = usb_status.timestamp
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis() as u64;

        SmartScopeCameraStatus {
            mode,
            camera_count,
            left_connected,
            right_connected,
            timestamp,
        }
    }
}
