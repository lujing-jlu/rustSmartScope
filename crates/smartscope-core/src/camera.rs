//! 相机管理模块
//!
//! 统一管理USB相机，提供左右相机的图像流

use std::sync::{Arc, Mutex};
use std::time::{SystemTime, Instant, Duration};
use std::process::Command;
use std::thread;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc;
use crate::error::{SmartScopeError, Result};
use crate::config::SmartScopeConfig;

// 重新导出USB相机相关类型
pub use usb_camera::{
    CameraStreamReader, CameraConfig, VideoFrame,
    V4L2DeviceManager, CameraResult, CameraError,
    CameraControl, CameraProperty, ParameterRange,
};

// 注意：不需要重新导出本模块的类型，它们已经在模块中定义

/// 相机模式
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub enum CameraMode {
    /// 无相机连接
    NoCamera = 0,
    /// 单相机模式
    SingleCamera = 1,
    /// 立体相机模式（双目）
    StereoCamera = 2,
}

/// 相机管理器 - 封装USB相机功能
pub struct CameraManager {
    /// 左相机流读取器
    left_reader: Option<CameraStreamReader>,
    /// 右相机流读取器
    right_reader: Option<CameraStreamReader>,
    /// 单相机流读取器（当只有一个相机时使用）
    single_reader: Option<CameraStreamReader>,
    /// 左相机参数控制
    left_control: Option<CameraControl>,
    /// 右相机参数控制
    right_control: Option<CameraControl>,
    /// 单相机参数控制
    single_control: Option<CameraControl>,
    /// 当前配置
    config: SmartScopeConfig,
    /// 运行状态
    running: bool,
    /// 最新的左相机帧
    left_frame: Arc<Mutex<Option<VideoFrame>>>,
    /// 最新的右相机帧
    right_frame: Arc<Mutex<Option<VideoFrame>>>,
    /// 最新的单相机帧
    single_frame: Arc<Mutex<Option<VideoFrame>>>,
    /// 当前相机模式
    current_mode: Arc<Mutex<CameraMode>>,
    /// 监测线程运行标志
    monitor_running: Arc<AtomicBool>,
    /// 监测线程句柄
    monitor_thread: Option<thread::JoinHandle<()>>,
    /// 缓存的相机连接状态
    cached_connection_status: Arc<Mutex<(bool, bool, Instant)>>, // (left, right, last_check_time)
    /// 模式变化通知接收端
    mode_change_rx: Option<mpsc::Receiver<CameraMode>>,
}

impl CameraManager {
    /// 创建新的相机管理器
    pub fn new(config: SmartScopeConfig) -> Self {
        Self {
            left_reader: None,
            right_reader: None,
            single_reader: None,
            left_control: None,
            right_control: None,
            single_control: None,
            config,
            running: false,
            left_frame: Arc::new(Mutex::new(None)),
            right_frame: Arc::new(Mutex::new(None)),
            single_frame: Arc::new(Mutex::new(None)),
            current_mode: Arc::new(Mutex::new(CameraMode::NoCamera)),
            monitor_running: Arc::new(AtomicBool::new(false)),
            monitor_thread: None,
            // 初始状态：未连接，时间设为很久以前以触发首次检测
            cached_connection_status: Arc::new(Mutex::new((false, false, Instant::now() - Duration::from_secs(10)))),
            mode_change_rx: None,
        }
    }

    /// 检测可用的相机设备
    pub fn detect_cameras(&self) -> Result<Vec<DetectedCamera>> {
        tracing::debug!("检测可用的相机设备...");

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

        tracing::debug!("检测到 {} 个相机设备", cameras.len());
        for camera in &cameras {
            tracing::debug!("相机: {} 位于 {}", camera.name, camera.device_path);
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

        // 根据检测到的相机数量确定模式
        let mode = self.determine_camera_mode(&detected_cameras);
        tracing::info!("检测到相机模式: {:?}", mode);

        if let Ok(mut current_mode) = self.current_mode.lock() {
            *current_mode = mode;
        }

        if detected_cameras.is_empty() {
            tracing::warn!("未检测到相机设备，运行在无相机模式");
            return Ok(()); // 允许在无相机模式下运行
        }

        // 创建相机配置
        let camera_config = CameraConfig {
            width: self.config.camera.width,
            height: self.config.camera.height,
            framerate: self.config.camera.fps,
            pixel_format: "MJPG".to_string(),
            parameters: std::collections::HashMap::new(),
        };

        match mode {
            CameraMode::StereoCamera => {
                // 双目模式：为左右相机创建流读取器和参数控制
                for camera in detected_cameras {
                    if camera.is_left {
                        tracing::info!("创建左相机流读取器: {}", camera.device_path);
                        let reader = CameraStreamReader::new(
                            &camera.device_path,
                            &camera.name,
                            camera_config.clone(),
                        );
                        self.left_reader = Some(reader);

                        // 创建左相机参数控制
                        let control = CameraControl::new(&camera.device_path);
                        tracing::info!("左相机参数控制创建成功");
                        self.left_control = Some(control);
                    } else if camera.is_right {
                        tracing::info!("创建右相机流读取器: {}", camera.device_path);
                        let reader = CameraStreamReader::new(
                            &camera.device_path,
                            &camera.name,
                            camera_config.clone(),
                        );
                        self.right_reader = Some(reader);

                        // 创建右相机参数控制
                        let control = CameraControl::new(&camera.device_path);
                        tracing::info!("右相机参数控制创建成功");
                        self.right_control = Some(control);
                    }
                }
            }
            CameraMode::SingleCamera => {
                // 单目模式：使用第一个检测到的相机
                if let Some(camera) = detected_cameras.first() {
                    tracing::info!("创建单相机流读取器: {}", camera.device_path);
                    let reader = CameraStreamReader::new(
                        &camera.device_path,
                        &camera.name,
                        camera_config.clone(),
                    );
                    self.single_reader = Some(reader);

                    // 创建单相机参数控制
                    let control = CameraControl::new(&camera.device_path);
                    tracing::info!("单相机参数控制创建成功");
                    self.single_control = Some(control);
                }
            }
            CameraMode::NoCamera => {
                // 无相机模式，不创建读取器
            }
        }

        tracing::info!("相机管理器初始化完成");
        Ok(())
    }

    /// 根据检测到的相机确定相机模式
    fn determine_camera_mode(&self, cameras: &[DetectedCamera]) -> CameraMode {
        let left_count = cameras.iter().filter(|c| c.is_left).count();
        let right_count = cameras.iter().filter(|c| c.is_right).count();

        if left_count > 0 && right_count > 0 {
            CameraMode::StereoCamera
        } else if !cameras.is_empty() {
            CameraMode::SingleCamera
        } else {
            CameraMode::NoCamera
        }
    }

    /// 启动相机
    pub fn start(&mut self) -> Result<()> {
        if self.running {
            return Ok(());
        }

        tracing::info!("启动相机系统");

        // 如果没有初始化，先初始化
        if self.left_reader.is_none() && self.right_reader.is_none() && self.single_reader.is_none() {
            self.initialize()?;
        }

        let mode = *self.current_mode.lock().unwrap();

        match mode {
            CameraMode::StereoCamera => {
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
            }
            CameraMode::SingleCamera => {
                // 启动单相机
                if let Some(ref mut reader) = self.single_reader {
                    reader.start().map_err(|e| {
                        SmartScopeError::Unknown(format!("启动单相机失败: {}", e))
                    })?;
                    tracing::info!("单相机启动成功");
                }
            }
            CameraMode::NoCamera => {
                tracing::warn!("无相机模式，跳过相机启动");
            }
        }

        // 启动相机状态监测线程
        self.start_monitor_thread();

        self.running = true;
        tracing::info!("相机系统启动成功，当前模式: {:?}", mode);
        Ok(())
    }

    /// 启动监测线程
    fn start_monitor_thread(&mut self) {
        if self.monitor_running.load(Ordering::Relaxed) {
            return;
        }

        tracing::info!("启动相机状态监测线程");

        let monitor_running = Arc::clone(&self.monitor_running);
        let current_mode = Arc::clone(&self.current_mode);
        let config = self.config.clone();

        // 创建模式变化通知通道
        let (mode_tx, mode_rx) = mpsc::channel();
        self.mode_change_rx = Some(mode_rx);

        monitor_running.store(true, Ordering::Relaxed);

        let handle = thread::spawn(move || {
            Self::monitor_loop(monitor_running, current_mode, config, mode_tx);
        });

        self.monitor_thread = Some(handle);
    }

    /// 监测线程循环
    fn monitor_loop(
        running: Arc<AtomicBool>,
        current_mode: Arc<Mutex<CameraMode>>,
        config: SmartScopeConfig,
        mode_tx: mpsc::Sender<CameraMode>,
    ) {
        let mut last_mode = CameraMode::NoCamera;

        while running.load(Ordering::Relaxed) {
            // 每1秒检测一次
            thread::sleep(Duration::from_secs(1));

            // 临时创建manager只为检测
            let temp_manager = CameraManager::new(config.clone());

            match temp_manager.detect_cameras() {
                Ok(cameras) => {
                    let new_mode = temp_manager.determine_camera_mode(&cameras);

                    if new_mode != last_mode {
                        tracing::info!("相机模式变化: {:?} -> {:?}", last_mode, new_mode);

                        if let Ok(mut mode) = current_mode.lock() {
                            *mode = new_mode;
                        }

                        // 发送模式变化通知
                        let _ = mode_tx.send(new_mode);

                        last_mode = new_mode;
                    }
                }
                Err(e) => {
                    tracing::error!("监测相机失败: {}", e);
                }
            }
        }

        tracing::info!("相机状态监测线程停止");
    }

    /// 停止相机
    pub fn stop(&mut self) -> Result<()> {
        if !self.running {
            return Ok(());
        }

        tracing::info!("停止相机系统");

        // 停止监测线程
        self.monitor_running.store(false, Ordering::Relaxed);
        if let Some(handle) = self.monitor_thread.take() {
            let _ = handle.join();
        }

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

        // 停止单相机
        if let Some(ref mut reader) = self.single_reader {
            reader.stop().map_err(|e| {
                SmartScopeError::Unknown(format!("停止单相机失败: {}", e))
            })?;
        }

        // 清理帧缓存
        if let Ok(mut left) = self.left_frame.lock() {
            *left = None;
        }
        if let Ok(mut right) = self.right_frame.lock() {
            *right = None;
        }
        if let Ok(mut single) = self.single_frame.lock() {
            *single = None;
        }

        self.running = false;
        tracing::info!("相机系统停止完成");
        Ok(())
    }

    /// 处理新帧数据
    pub fn process_frames(&mut self) {
        // 检查是否有模式变化通知
        if let Some(ref rx) = self.mode_change_rx {
            if let Ok(new_mode) = rx.try_recv() {
                tracing::info!("收到模式变化通知，重新初始化相机: {:?}", new_mode);
                if let Err(e) = self.reinitialize_for_mode(new_mode) {
                    tracing::error!("重新初始化相机失败: {}", e);
                }
            }
        }

        let mode = *self.current_mode.lock().unwrap();

        match mode {
            CameraMode::StereoCamera => {
                // 并行读取左右相机帧（使用rayon或者手动spawn）
                // 为了避免引入rayon依赖，我们优化读取顺序，先尝试读取所有帧到临时变量
                // 这样可以最小化锁的持有时间

                let left_frame_opt = self.left_reader.as_mut()
                    .and_then(|reader| reader.read_frame().ok())
                    .flatten();

                let right_frame_opt = self.right_reader.as_mut()
                    .and_then(|reader| reader.read_frame().ok())
                    .flatten();

                // 批量更新帧缓存，减少锁竞争
                if let Some(frame) = left_frame_opt {
                    if let Ok(mut left) = self.left_frame.lock() {
                        *left = Some(frame);
                    }
                }

                if let Some(frame) = right_frame_opt {
                    if let Ok(mut right) = self.right_frame.lock() {
                        *right = Some(frame);
                    }
                }
            }
            CameraMode::SingleCamera => {
                // 从单相机读取帧
                if let Some(ref mut reader) = self.single_reader {
                    if let Ok(Some(frame)) = reader.read_frame() {
                        if let Ok(mut single) = self.single_frame.lock() {
                            *single = Some(frame);
                        }
                    }
                }
            }
            CameraMode::NoCamera => {
                // 无相机模式，不读取帧
            }
        }
    }

    /// 针对新模式重新初始化相机
    fn reinitialize_for_mode(&mut self, new_mode: CameraMode) -> Result<()> {
        tracing::info!("相机模式切换: {:?}，重新初始化中...", new_mode);

        // 停止所有现有的相机读取器
        if let Some(mut reader) = self.left_reader.take() {
            tracing::info!("停止左相机读取器");
            let _ = reader.stop(); // 忽略错误，现在stop()能处理panic了
        }
        if let Some(mut reader) = self.right_reader.take() {
            tracing::info!("停止右相机读取器");
            let _ = reader.stop();
        }
        if let Some(mut reader) = self.single_reader.take() {
            tracing::info!("停止单相机读取器");
            let _ = reader.stop();
        }

        // 等待一小段时间，确保资源完全释放
        thread::sleep(Duration::from_millis(100));

        // 清空帧缓存
        if let Ok(mut left) = self.left_frame.lock() {
            *left = None;
        }
        if let Ok(mut right) = self.right_frame.lock() {
            *right = None;
        }
        if let Ok(mut single) = self.single_frame.lock() {
            *single = None;
        }

        // 如果是NoCamera模式，直接返回
        if new_mode == CameraMode::NoCamera {
            tracing::info!("无相机模式，无需初始化读取器");
            return Ok(());
        }

        // 重新检测相机
        let detected_cameras = self.detect_cameras()?;

        // 创建相机配置
        let camera_config = CameraConfig {
            width: self.config.camera.width,
            height: self.config.camera.height,
            framerate: self.config.camera.fps,
            pixel_format: "MJPG".to_string(),
            parameters: std::collections::HashMap::new(),
        };

        // 根据新模式创建并启动相应的读取器
        match new_mode {
            CameraMode::StereoCamera => {
                // 双目模式
                for camera in detected_cameras {
                    if camera.is_left {
                        tracing::info!("创建并启动左相机流读取器: {}", camera.device_path);
                        let mut reader = CameraStreamReader::new(
                            &camera.device_path,
                            &camera.name,
                            camera_config.clone(),
                        );
                        reader.start().map_err(|e| {
                            SmartScopeError::Unknown(format!("启动左相机失败: {}", e))
                        })?;
                        self.left_reader = Some(reader);

                        // 创建左相机参数控制
                        let control = CameraControl::new(&camera.device_path);
                        tracing::info!("左相机参数控制创建成功");
                        self.left_control = Some(control);
                    } else if camera.is_right {
                        tracing::info!("创建并启动右相机流读取器: {}", camera.device_path);
                        let mut reader = CameraStreamReader::new(
                            &camera.device_path,
                            &camera.name,
                            camera_config.clone(),
                        );
                        reader.start().map_err(|e| {
                            SmartScopeError::Unknown(format!("启动右相机失败: {}", e))
                        })?;
                        self.right_reader = Some(reader);

                        // 创建右相机参数控制
                        let control = CameraControl::new(&camera.device_path);
                        tracing::info!("右相机参数控制创建成功");
                        self.right_control = Some(control);
                    }
                }
            }
            CameraMode::SingleCamera => {
                // 单目模式
                if let Some(camera) = detected_cameras.first() {
                    tracing::info!("创建并启动单相机流读取器: {}", camera.device_path);
                    let mut reader = CameraStreamReader::new(
                        &camera.device_path,
                        &camera.name,
                        camera_config.clone(),
                    );
                    reader.start().map_err(|e| {
                        SmartScopeError::Unknown(format!("启动单相机失败: {}", e))
                    })?;
                    self.single_reader = Some(reader);

                    // 创建单相机参数控制
                    let control = CameraControl::new(&camera.device_path);
                    tracing::info!("单相机参数控制创建成功");
                    self.single_control = Some(control);
                }
            }
            CameraMode::NoCamera => {
                // 已经在上面处理了
            }
        }

        tracing::info!("相机模式切换完成: {:?}", new_mode);
        Ok(())
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

    /// 获取左相机最新帧（使用take避免克隆，调用后会清空缓存）
    pub fn get_left_frame(&self) -> Option<VideoFrame> {
        self.left_frame.lock().ok()?.take()
    }

    /// 获取右相机最新帧（使用take避免克隆，调用后会清空缓存）
    pub fn get_right_frame(&self) -> Option<VideoFrame> {
        self.right_frame.lock().ok()?.take()
    }

    /// 获取单相机最新帧（使用take避免克隆，调用后会清空缓存）
    pub fn get_single_frame(&self) -> Option<VideoFrame> {
        self.single_frame.lock().ok()?.take()
    }

    /// 获取当前相机模式
    pub fn get_camera_mode(&self) -> CameraMode {
        *self.current_mode.lock().unwrap()
    }

    /// 检查相机是否运行中
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// 获取相机状态信息
    pub fn get_status(&self) -> CameraStatus {
        // 实时检测相机连接状态
        let (left_connected, right_connected) = self.check_camera_connections();
        let mode = self.get_camera_mode();

        CameraStatus {
            running: self.running,
            mode,
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

    // ============================================================================
    // 相机参数控制方法
    // ============================================================================

    /// 设置左相机参数
    pub fn set_left_camera_parameter(&mut self, property: &CameraProperty, value: i32) -> Result<()> {
        if let Some(ref mut control) = self.left_control {
            control.set_parameter(property, value)
                .map_err(|e| SmartScopeError::Unknown(format!("设置左相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("左相机参数控制未初始化".to_string()))
        }
    }

    /// 设置右相机参数
    pub fn set_right_camera_parameter(&mut self, property: &CameraProperty, value: i32) -> Result<()> {
        if let Some(ref mut control) = self.right_control {
            control.set_parameter(property, value)
                .map_err(|e| SmartScopeError::Unknown(format!("设置右相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("右相机参数控制未初始化".to_string()))
        }
    }

    /// 设置单相机参数
    pub fn set_single_camera_parameter(&mut self, property: &CameraProperty, value: i32) -> Result<()> {
        if let Some(ref mut control) = self.single_control {
            control.set_parameter(property, value)
                .map_err(|e| SmartScopeError::Unknown(format!("设置单相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("单相机参数控制未初始化".to_string()))
        }
    }

    /// 获取左相机参数
    pub fn get_left_camera_parameter(&mut self, property: &CameraProperty) -> Result<i32> {
        if let Some(ref mut control) = self.left_control {
            control.get_parameter(property)
                .map_err(|e| SmartScopeError::Unknown(format!("获取左相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("左相机参数控制未初始化".to_string()))
        }
    }

    /// 获取右相机参数
    pub fn get_right_camera_parameter(&mut self, property: &CameraProperty) -> Result<i32> {
        if let Some(ref mut control) = self.right_control {
            control.get_parameter(property)
                .map_err(|e| SmartScopeError::Unknown(format!("获取右相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("右相机参数控制未初始化".to_string()))
        }
    }

    /// 获取单相机参数
    pub fn get_single_camera_parameter(&mut self, property: &CameraProperty) -> Result<i32> {
        if let Some(ref mut control) = self.single_control {
            control.get_parameter(property)
                .map_err(|e| SmartScopeError::Unknown(format!("获取单相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("单相机参数控制未初始化".to_string()))
        }
    }

    /// 获取左相机参数范围
    pub fn get_left_camera_parameter_range(&mut self, property: &CameraProperty) -> Result<ParameterRange> {
        if let Some(ref mut control) = self.left_control {
            control.get_parameter_range(property)
                .map_err(|e| SmartScopeError::Unknown(format!("获取左相机参数范围失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("左相机参数控制未初始化".to_string()))
        }
    }

    /// 获取右相机参数范围
    pub fn get_right_camera_parameter_range(&mut self, property: &CameraProperty) -> Result<ParameterRange> {
        if let Some(ref mut control) = self.right_control {
            control.get_parameter_range(property)
                .map_err(|e| SmartScopeError::Unknown(format!("获取右相机参数范围失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("右相机参数控制未初始化".to_string()))
        }
    }

    /// 获取单相机参数范围
    pub fn get_single_camera_parameter_range(&mut self, property: &CameraProperty) -> Result<ParameterRange> {
        if let Some(ref mut control) = self.single_control {
            control.get_parameter_range(property)
                .map_err(|e| SmartScopeError::Unknown(format!("获取单相机参数范围失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("单相机参数控制未初始化".to_string()))
        }
    }

    /// 重置左相机参数到默认值
    pub fn reset_left_camera_parameters(&mut self) -> Result<()> {
        if let Some(ref mut control) = self.left_control {
            control.reset_to_defaults()
                .map_err(|e| SmartScopeError::Unknown(format!("重置左相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("左相机参数控制未初始化".to_string()))
        }
    }

    /// 重置右相机参数到默认值
    pub fn reset_right_camera_parameters(&mut self) -> Result<()> {
        if let Some(ref mut control) = self.right_control {
            control.reset_to_defaults()
                .map_err(|e| SmartScopeError::Unknown(format!("重置右相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("右相机参数控制未初始化".to_string()))
        }
    }

    /// 重置单相机参数到默认值
    pub fn reset_single_camera_parameters(&mut self) -> Result<()> {
        if let Some(ref mut control) = self.single_control {
            control.reset_to_defaults()
                .map_err(|e| SmartScopeError::Unknown(format!("重置单相机参数失败: {}", e)))
        } else {
            Err(SmartScopeError::Unknown("单相机参数控制未初始化".to_string()))
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
    pub mode: CameraMode,
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