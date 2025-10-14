//! SmartScope Minimal C FFI Interface
//!
//! 提供最基础的C兼容接口，用于演示Rust-C++桥接框架

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};
use std::sync::{Mutex, Once};

use smartscope_core::config::SmartScopeConfig;
use smartscope_core::{
    init_global_logger, log_from_cpp, AppState, CameraMode, LogLevel, LogRotation,
    LoggerConfig, SmartScopeError,
};

#[macro_use]
extern crate lazy_static;

/// 全局应用状态实例
static mut APP_STATE: Option<AppState> = None;
static INIT: Once = Once::new();

/// C FFI错误码
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorCode {
    Success = 0,
    Error = -1,
    ConfigError = -3,
    IoError = -5,
}

impl From<SmartScopeError> for ErrorCode {
    fn from(error: SmartScopeError) -> Self {
        match error {
            SmartScopeError::Config(_) => ErrorCode::ConfigError,
            SmartScopeError::Io(_) => ErrorCode::IoError,
            _ => ErrorCode::Error,
        }
    }
}

// ============================
// AI 推理服务（RKNN）全局状态
// ============================
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex as StdMutex;
use rknn_inference::{MultiDetectorInferenceService, ImagePreprocessor, ImageBuffer, ImageFormat};

lazy_static! {
    static ref AI_SERVICE: StdMutex<Option<MultiDetectorInferenceService>> = StdMutex::new(None);
    static ref AI_ENABLED: AtomicBool = AtomicBool::new(false);
}

#[repr(C)]
pub struct CDetection {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    pub confidence: f32,
    pub class_id: i32,
}

const MAX_DETECTIONS: usize = 128;

/// 初始化SmartScope系统和日志
#[no_mangle]
pub extern "C" fn smartscope_init() -> c_int {
    INIT.call_once(|| {
        // 初始化统一日志系统
        let log_config = LoggerConfig {
            level: LogLevel::Info,
            log_dir: "logs".to_string(),
            console_output: true,
            file_output: true,
            json_format: false,
            rotation: LogRotation::Daily,
        };

        if let Err(e) = init_global_logger(log_config) {
            eprintln!("Failed to initialize logger: {}", e);
        }

        unsafe {
            let mut state = AppState::new();
            match state.initialize() {
                Ok(_) => {
                    APP_STATE = Some(state);
                }
                Err(e) => {
                    eprintln!("Failed to initialize: {}", e);
                }
            }
        }
    });

    ErrorCode::Success as c_int
}

/// 关闭SmartScope系统
#[no_mangle]
#[allow(static_mut_refs)]
pub extern "C" fn smartscope_shutdown() -> c_int {
    unsafe {
        if let Some(mut state) = APP_STATE.take() {
            state.shutdown();
        }
    }

    ErrorCode::Success as c_int
}

/// 获取应用状态指针（内部使用）
#[allow(static_mut_refs)]
fn get_app_state() -> Result<&'static mut AppState, SmartScopeError> {
    unsafe {
        APP_STATE
            .as_mut()
            .ok_or_else(|| SmartScopeError::Unknown("SmartScope not initialized".to_string()))
    }
}

/// 检查系统是否已初始化
#[no_mangle]
#[allow(static_mut_refs)]
pub extern "C" fn smartscope_is_initialized() -> bool {
    unsafe {
        APP_STATE
            .as_ref()
            .map(|s| s.is_initialized())
            .unwrap_or(false)
    }
}

/// 加载配置文件
#[no_mangle]
pub extern "C" fn smartscope_load_config(config_path: *const c_char) -> c_int {
    if config_path.is_null() {
        return ErrorCode::ConfigError as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let path_str = unsafe {
        match CStr::from_ptr(config_path).to_str() {
            Ok(s) => s,
            Err(_) => return ErrorCode::ConfigError as c_int,
        }
    };

    match SmartScopeConfig::load_from_file(path_str) {
        Ok(config) => {
            *app_state.config.write().unwrap() = config;
            tracing::info!("Configuration loaded from: {}", path_str);
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("Failed to load config: {}", e);
            ErrorCode::from(e) as c_int
        }
    }
}

/// 保存配置文件
#[no_mangle]
pub extern "C" fn smartscope_save_config(config_path: *const c_char) -> c_int {
    if config_path.is_null() {
        return ErrorCode::ConfigError as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let path_str = unsafe {
        match CStr::from_ptr(config_path).to_str() {
            Ok(s) => s,
            Err(_) => return ErrorCode::ConfigError as c_int,
        }
    };

    let config = app_state.config.read().unwrap().clone();
    match config.save_to_file(path_str) {
        Ok(_) => {
            tracing::info!("Configuration saved to: {}", path_str);
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("Failed to save config: {}", e);
            ErrorCode::from(e) as c_int
        }
    }
}

/// 获取版本字符串
#[no_mangle]
pub extern "C" fn smartscope_get_version() -> *const c_char {
    "RustSmartScope 0.1.0\0".as_ptr() as *const c_char
}

/// 获取错误信息字符串
#[no_mangle]
pub extern "C" fn smartscope_get_error_string(error_code: c_int) -> *const c_char {
    let error_msg = match error_code {
        x if x == ErrorCode::Success as c_int => "Success",
        x if x == ErrorCode::Error as c_int => "General error",
        x if x == ErrorCode::ConfigError as c_int => "Configuration error",
        x if x == ErrorCode::IoError as c_int => "IO error",
        _ => "Unknown error",
    };

    // 注意: 这里返回的是静态字符串指针，不需要释放
    error_msg.as_ptr() as *const c_char
}

/// 释放由Rust分配的内存
#[no_mangle]
pub extern "C" fn smartscope_free_string(s: *mut c_char) {
    if s.is_null() {
        return;
    }
    unsafe {
        let _ = CString::from_raw(s);
    }
}

// ============================================================================
// 日志相关C FFI接口
// ============================================================================

/// C FFI日志级别（与Rust的LogLevel对应）
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CLogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
}

impl From<CLogLevel> for LogLevel {
    fn from(level: CLogLevel) -> Self {
        match level {
            CLogLevel::Trace => LogLevel::Trace,
            CLogLevel::Debug => LogLevel::Debug,
            CLogLevel::Info => LogLevel::Info,
            CLogLevel::Warn => LogLevel::Warn,
            CLogLevel::Error => LogLevel::Error,
        }
    }
}

/// C++调用的日志函数
#[no_mangle]
pub extern "C" fn smartscope_log(
    level: CLogLevel,
    module: *const c_char,
    message: *const c_char,
) -> c_int {
    if module.is_null() || message.is_null() {
        return ErrorCode::Error as c_int;
    }

    let module_str = match unsafe { CStr::from_ptr(module) }.to_str() {
        Ok(s) => s,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let message_str = match unsafe { CStr::from_ptr(message) }.to_str() {
        Ok(s) => s,
        Err(_) => return ErrorCode::Error as c_int,
    };

    log_from_cpp(level.into(), module_str, message_str);
    ErrorCode::Success as c_int
}

/// QML调用的日志函数
#[no_mangle]
pub extern "C" fn smartscope_log_qml(level: CLogLevel, message: *const c_char) -> c_int {
    if message.is_null() {
        return ErrorCode::Error as c_int;
    }

    let message_str = match unsafe { CStr::from_ptr(message) }.to_str() {
        Ok(s) => s,
        Err(_) => return ErrorCode::Error as c_int,
    };

    log_from_cpp(level.into(), "QML", message_str);
    ErrorCode::Success as c_int
}

/// 设置日志级别
#[no_mangle]
pub extern "C" fn smartscope_set_log_level(level: CLogLevel) -> c_int {
    // TODO: 实现动态日志级别设置
    log_from_cpp(
        LogLevel::Info,
        "Logger",
        &format!("Log level change requested: {:?}", level),
    );
    ErrorCode::Success as c_int
}

// ============================================================================
// 相机相关C FFI接口
// ============================================================================

/// C FFI相机模式枚举
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CCameraMode {
    NoCamera = 0,
    SingleCamera = 1,
    StereoCamera = 2,
}

impl From<CameraMode> for CCameraMode {
    fn from(mode: CameraMode) -> Self {
        match mode {
            CameraMode::NoCamera => CCameraMode::NoCamera,
            CameraMode::SingleCamera => CCameraMode::SingleCamera,
            CameraMode::StereoCamera => CCameraMode::StereoCamera,
        }
    }
}

/// C FFI相机帧数据结构
#[repr(C)]
pub struct CCameraFrame {
    pub data: *const u8,
    pub data_len: usize,
    pub width: u32,
    pub height: u32,
    pub format: u32,
    pub timestamp_sec: u64,
    pub timestamp_nsec: u32,
}

/// C FFI相机状态结构
#[repr(C)]
pub struct CCameraStatus {
    pub running: bool,
    pub mode: u32, // CCameraMode as u32
    pub left_camera_connected: bool,
    pub right_camera_connected: bool,
    pub last_left_frame_sec: u64,
    pub last_right_frame_sec: u64,
}

/// 启动相机系统
#[no_mangle]
pub extern "C" fn smartscope_start_camera() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.start_camera() {
        Ok(_) => {
            tracing::info!("相机系统启动成功");
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("相机系统启动失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 停止相机系统
#[no_mangle]
pub extern "C" fn smartscope_stop_camera() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.stop_camera() {
        Ok(_) => {
            tracing::info!("相机系统停止成功");
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("相机系统停止失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 处理相机帧数据（需要定期调用）
#[no_mangle]
pub extern "C" fn smartscope_process_camera_frames() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    app_state.process_camera_frames();
    ErrorCode::Success as c_int
}

// 静态缓冲区用于存储帧数据（防止指针失效）
lazy_static! {
    static ref LEFT_FRAME_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());
    static ref RIGHT_FRAME_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());
    static ref SINGLE_FRAME_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());
}

/// 获取左相机最新帧（已处理：MJPEG解码 + 畸变校正 + 视频变换）
#[no_mangle]
pub extern "C" fn smartscope_get_left_frame(frame_out: *mut CCameraFrame) -> c_int {
    if frame_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    // 使用处理后的帧（包含畸变校正）
    if let Some(frame) = app_state.get_processed_left_frame() {
        let timestamp = frame
            .timestamp
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default();

        // 将帧数据复制到静态缓冲区，确保指针在FFI调用期间有效
        let mut buffer = LEFT_FRAME_BUFFER.lock().unwrap();
        buffer.clear();
        buffer.extend_from_slice(&frame.data);

        tracing::debug!(
            "FFI: 返回左相机帧 - 大小: {} 字节, 尺寸: {}x{}, 格式: {}",
            buffer.len(),
            frame.width,
            frame.height,
            frame.format
        );

        unsafe {
            (*frame_out) = CCameraFrame {
                data: buffer.as_ptr(),
                data_len: buffer.len(),
                width: frame.width,
                height: frame.height,
                format: frame.format,
                timestamp_sec: timestamp.as_secs(),
                timestamp_nsec: timestamp.subsec_nanos(),
            };
        }
        ErrorCode::Success as c_int
    } else {
        ErrorCode::Error as c_int
    }
}

/// 获取右相机最新帧（已处理：MJPEG解码 + 畸变校正 + 视频变换）
#[no_mangle]
pub extern "C" fn smartscope_get_right_frame(frame_out: *mut CCameraFrame) -> c_int {
    if frame_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    // 使用处理后的帧（包含畸变校正）
    if let Some(frame) = app_state.get_processed_right_frame() {
        let timestamp = frame
            .timestamp
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default();

        // 将帧数据复制到静态缓冲区，确保指针在FFI调用期间有效
        let mut buffer = RIGHT_FRAME_BUFFER.lock().unwrap();
        buffer.clear();
        buffer.extend_from_slice(&frame.data);

        tracing::debug!(
            "FFI: 返回右相机帧 - 大小: {} 字节, 尺寸: {}x{}, 格式: {}",
            buffer.len(),
            frame.width,
            frame.height,
            frame.format
        );

        unsafe {
            (*frame_out) = CCameraFrame {
                data: buffer.as_ptr(),
                data_len: buffer.len(),
                width: frame.width,
                height: frame.height,
                format: frame.format,
                timestamp_sec: timestamp.as_secs(),
                timestamp_nsec: timestamp.subsec_nanos(),
            };
        }
        ErrorCode::Success as c_int
    } else {
        ErrorCode::Error as c_int
    }
}

/// 获取单相机最新帧
#[no_mangle]
pub extern "C" fn smartscope_get_single_frame(frame_out: *mut CCameraFrame) -> c_int {
    if frame_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    if let Some(frame) = app_state.get_single_camera_frame() {
        let timestamp = frame
            .timestamp
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default();

        // 将帧数据复制到静态缓冲区，确保指针在FFI调用期间有效
        let mut buffer = SINGLE_FRAME_BUFFER.lock().unwrap();
        buffer.clear();
        buffer.extend_from_slice(&frame.data);

        tracing::debug!(
            "FFI: 返回单相机帧 - 大小: {} 字节, 尺寸: {}x{}, 格式: {}",
            buffer.len(),
            frame.width,
            frame.height,
            frame.format
        );

        unsafe {
            (*frame_out) = CCameraFrame {
                data: buffer.as_ptr(),
                data_len: buffer.len(),
                width: frame.width,
                height: frame.height,
                format: frame.format,
                timestamp_sec: timestamp.as_secs(),
                timestamp_nsec: timestamp.subsec_nanos(),
            };
        }
        ErrorCode::Success as c_int
    } else {
        ErrorCode::Error as c_int
    }
}

// ==============================================
// AI 推理 FFI 接口
// ==============================================

/// 初始化AI推理服务（RKNN YOLOv8）
/// model_path: RKNN模型路径，如 "models/yolov8m.rknn"
/// num_workers: 工作线程数量，建议6
#[no_mangle]
pub extern "C" fn smartscope_ai_init(model_path: *const c_char, num_workers: i32) -> c_int {
    if model_path.is_null() || num_workers <= 0 {
        return ErrorCode::Error as c_int;
    }

    let path_str = unsafe { match CStr::from_ptr(model_path).to_str() { Ok(s) => s, Err(_) => return ErrorCode::Error as c_int } };

    let mut guard = AI_SERVICE.lock().unwrap();
    match MultiDetectorInferenceService::new(path_str, num_workers as usize) {
        Ok(service) => {
            *guard = Some(service);
            AI_ENABLED.store(false, Ordering::Relaxed);
            tracing::info!("AI inference service initialized: {} workers, model: {}", num_workers, path_str);
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("Failed to init AI service: {:?}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 关闭AI推理服务
#[no_mangle]
pub extern "C" fn smartscope_ai_shutdown() {
    let mut guard = AI_SERVICE.lock().unwrap();
    *guard = None;
    AI_ENABLED.store(false, Ordering::Relaxed);
    tracing::info!("AI inference service shutdown");
}

/// 启用/禁用AI检测
#[no_mangle]
pub extern "C" fn smartscope_ai_set_enabled(enabled: bool) {
    AI_ENABLED.store(enabled, Ordering::Relaxed);
}

/// 查询AI检测是否启用
#[no_mangle]
pub extern "C" fn smartscope_ai_is_enabled() -> bool {
    AI_ENABLED.load(Ordering::Relaxed)
}

/// 提交一帧RGB888图像到AI推理服务（非阻塞）
/// data指向RGB888数据（width*height*3字节）
#[no_mangle]
pub extern "C" fn smartscope_ai_submit_rgb888(width: i32, height: i32, data: *const u8, len: usize) -> c_int {
    if !AI_ENABLED.load(Ordering::Relaxed) {
        return ErrorCode::Success as c_int; // 检测未启用时忽略
    }

    if width <= 0 || height <= 0 || data.is_null() {
        return ErrorCode::Error as c_int;
    }

    let guard = AI_SERVICE.lock().unwrap();
    let service = match guard.as_ref() { Some(s) => s, None => return ErrorCode::Error as c_int };

    let data_slice = unsafe { std::slice::from_raw_parts(data, len.min((width as usize)*(height as usize)*3)) };

    // 使用预处理器进行letterbox+缩放
    let rgb_image = match image::RgbImage::from_raw(width as u32, height as u32, data_slice.to_vec()) {
        Some(img) => img,
        None => return ErrorCode::Error as c_int,
    };

    let preprocessor = rknn_inference::ImagePreprocessor::new();
    let image_buffer = preprocessor.preprocess(&rgb_image);

    match service.submit_inference(&image_buffer) {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("AI submit failed: {:?}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 尝试获取最新推理结果
///
/// 参数：
/// - results_out: 指向CDetection数组的指针
/// - max_results: 数组最大容量
/// 返回：检测数量（>=0），无结果或过期返回0，错误返回-1
#[no_mangle]
pub extern "C" fn smartscope_ai_try_get_latest_result(results_out: *mut CDetection, max_results: i32) -> c_int {
    if results_out.is_null() || max_results <= 0 {
        return -1;
    }

    if !AI_ENABLED.load(Ordering::Relaxed) {
        return 0;
    }

    let guard = AI_SERVICE.lock().unwrap();
    let service = match guard.as_ref() { Some(s) => s, None => return -1 };

    if let Some((_task_id, result)) = service.try_get_latest_result() {
        match result {
            Ok(dets) => {
                let count = dets.len().min(max_results as usize).min(MAX_DETECTIONS);
                for i in 0..count {
                    let d = dets[i];
                    unsafe {
                        let out = results_out.add(i);
                        (*out).left = d.bbox.left;
                        (*out).top = d.bbox.top;
                        (*out).right = d.bbox.right;
                        (*out).bottom = d.bbox.bottom;
                        (*out).confidence = d.confidence;
                        (*out).class_id = d.class_id;
                    }
                }
                count as c_int
            }
            Err(_e) => 0,
        }
    } else {
        0
    }
}

/// 获取相机状态
#[no_mangle]
pub extern "C" fn smartscope_get_camera_status(status_out: *mut CCameraStatus) -> c_int {
    if status_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    if let Some(status) = app_state.get_camera_status() {
        let left_time = status
            .last_left_frame_time
            .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
            .map(|d| d.as_secs())
            .unwrap_or(0);

        let right_time = status
            .last_right_frame_time
            .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
            .map(|d| d.as_secs())
            .unwrap_or(0);

        let c_mode: CCameraMode = status.mode.into();

        unsafe {
            (*status_out) = CCameraStatus {
                running: status.running,
                mode: c_mode as u32,
                left_camera_connected: status.left_camera_connected,
                right_camera_connected: status.right_camera_connected,
                last_left_frame_sec: left_time,
                last_right_frame_sec: right_time,
            };
        }
        ErrorCode::Success as c_int
    } else {
        ErrorCode::Error as c_int
    }
}

/// 获取当前相机模式
#[no_mangle]
pub extern "C" fn smartscope_get_camera_mode() -> u32 {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return CCameraMode::NoCamera as u32,
    };

    let mode = app_state.get_camera_mode();
    let c_mode: CCameraMode = mode.into();
    c_mode as u32
}

/// 检查相机是否运行中
#[no_mangle]
pub extern "C" fn smartscope_is_camera_running() -> bool {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return false,
    };

    app_state.is_camera_running()
}

// ============================================================================
// 视频变换相关C FFI接口
// ============================================================================

/// 应用旋转（累加90度）
#[no_mangle]
pub extern "C" fn smartscope_video_apply_rotation() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.video_apply_rotation() {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("应用旋转失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 设置旋转角度
#[no_mangle]
pub extern "C" fn smartscope_video_set_rotation(degrees: u32) -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.video_set_rotation(degrees) {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("设置旋转失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 切换水平翻转
#[no_mangle]
pub extern "C" fn smartscope_video_toggle_flip_horizontal() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.video_toggle_flip_horizontal() {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("切换水平翻转失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 切换垂直翻转
#[no_mangle]
pub extern "C" fn smartscope_video_toggle_flip_vertical() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.video_toggle_flip_vertical() {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("切换垂直翻转失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 切换反色
#[no_mangle]
pub extern "C" fn smartscope_video_toggle_invert() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.video_toggle_invert() {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("切换反色失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 重置所有视频变换
#[no_mangle]
pub extern "C" fn smartscope_video_reset_transforms() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.video_reset_transforms() {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("重置视频变换失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 获取当前旋转角度
#[no_mangle]
pub extern "C" fn smartscope_video_get_rotation() -> u32 {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return 0,
    };

    app_state.video_get_rotation()
}

/// 获取水平翻转状态
#[no_mangle]
pub extern "C" fn smartscope_video_get_flip_horizontal() -> bool {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return false,
    };

    app_state.video_get_flip_horizontal()
}

/// 获取垂直翻转状态
#[no_mangle]
pub extern "C" fn smartscope_video_get_flip_vertical() -> bool {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return false,
    };

    app_state.video_get_flip_vertical()
}

/// 获取反色状态
#[no_mangle]
pub extern "C" fn smartscope_video_get_invert() -> bool {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return false,
    };

    app_state.video_get_invert()
}

/// 检查RGA硬件是否可用
#[no_mangle]
pub extern "C" fn smartscope_video_is_rga_available() -> bool {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return false,
    };

    app_state.video_is_rga_available()
}

// ============================================================================
// 畸变校正相关C FFI接口
// ============================================================================

/// 切换畸变校正
#[no_mangle]
pub extern "C" fn smartscope_toggle_distortion_correction() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.toggle_distortion_correction() {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("切换畸变校正失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 设置畸变校正状态
#[no_mangle]
pub extern "C" fn smartscope_set_distortion_correction(enabled: bool) -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.set_distortion_correction(enabled) {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("设置畸变校正失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 获取畸变校正状态
#[no_mangle]
pub extern "C" fn smartscope_get_distortion_correction() -> bool {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return false,
    };

    app_state.get_distortion_correction()
}

// ============================================================================
// 相机参数控制相关C FFI接口
// ============================================================================

use smartscope_core::camera::CameraProperty;

/// C FFI相机属性枚举 - 匹配reference_code/SmartScope
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CCameraProperty {
    Brightness = 0,
    Contrast = 1,
    Saturation = 2,
    Gain = 3,
    ExposureTime = 4,     // 修改：Exposure -> ExposureTime
    WhiteBalance = 5,
    Gamma = 6,
    BacklightCompensation = 7,
    AutoExposure = 8,
    AutoWhiteBalance = 9,
}

impl From<CCameraProperty> for CameraProperty {
    fn from(prop: CCameraProperty) -> Self {
        match prop {
            CCameraProperty::Brightness => CameraProperty::Brightness,
            CCameraProperty::Contrast => CameraProperty::Contrast,
            CCameraProperty::Saturation => CameraProperty::Saturation,
            CCameraProperty::Gain => CameraProperty::Gain,
            CCameraProperty::ExposureTime => CameraProperty::Exposure,  // 修改：映射到内部Exposure
            CCameraProperty::WhiteBalance => CameraProperty::WhiteBalance,
            CCameraProperty::Gamma => CameraProperty::Gamma,
            CCameraProperty::BacklightCompensation => CameraProperty::BacklightCompensation,
            CCameraProperty::AutoExposure => CameraProperty::AutoExposure,
            CCameraProperty::AutoWhiteBalance => CameraProperty::AutoWhiteBalance,
        }
    }
}

/// C FFI参数范围结构
#[repr(C)]
pub struct CCameraParameterRange {
    pub min: i32,
    pub max: i32,
    pub step: i32,
    pub default_value: i32,
    pub current: i32,
}

/// 设置左相机参数
#[no_mangle]
pub extern "C" fn smartscope_set_left_camera_parameter(property: CCameraProperty, value: i32) -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.set_left_camera_parameter(&rust_property, value) {
        Ok(_) => {
            tracing::debug!("设置左相机参数 {:?} = {}", property, value);
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("设置左相机参数失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 设置右相机参数
#[no_mangle]
pub extern "C" fn smartscope_set_right_camera_parameter(property: CCameraProperty, value: i32) -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.set_right_camera_parameter(&rust_property, value) {
        Ok(_) => {
            tracing::debug!("设置右相机参数 {:?} = {}", property, value);
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("设置右相机参数失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 设置单相机参数
#[no_mangle]
pub extern "C" fn smartscope_set_single_camera_parameter(property: CCameraProperty, value: i32) -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.set_single_camera_parameter(&rust_property, value) {
        Ok(_) => {
            tracing::debug!("设置单相机参数 {:?} = {}", property, value);
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("设置单相机参数失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 获取左相机参数
#[no_mangle]
pub extern "C" fn smartscope_get_left_camera_parameter(property: CCameraProperty) -> i32 {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return -1,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.get_left_camera_parameter(&rust_property) {
        Ok(value) => value,
        Err(e) => {
            tracing::error!("获取左相机参数失败: {}", e);
            -1
        }
    }
}

/// 获取右相机参数
#[no_mangle]
pub extern "C" fn smartscope_get_right_camera_parameter(property: CCameraProperty) -> i32 {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return -1,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.get_right_camera_parameter(&rust_property) {
        Ok(value) => value,
        Err(e) => {
            tracing::error!("获取右相机参数失败: {}", e);
            -1
        }
    }
}

/// 获取单相机参数
#[no_mangle]
pub extern "C" fn smartscope_get_single_camera_parameter(property: CCameraProperty) -> i32 {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return -1,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.get_single_camera_parameter(&rust_property) {
        Ok(value) => value,
        Err(e) => {
            tracing::error!("获取单相机参数失败: {}", e);
            -1
        }
    }
}

/// 获取左相机参数范围
#[no_mangle]
pub extern "C" fn smartscope_get_left_camera_parameter_range(
    property: CCameraProperty,
    range_out: *mut CCameraParameterRange,
) -> c_int {
    if range_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.get_left_camera_parameter_range(&rust_property) {
        Ok(range) => {
            unsafe {
                (*range_out) = CCameraParameterRange {
                    min: range.min,
                    max: range.max,
                    step: range.step,
                    default_value: range.default,
                    current: range.current,
                };
            }
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("获取左相机参数范围失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 获取右相机参数范围
#[no_mangle]
pub extern "C" fn smartscope_get_right_camera_parameter_range(
    property: CCameraProperty,
    range_out: *mut CCameraParameterRange,
) -> c_int {
    if range_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.get_right_camera_parameter_range(&rust_property) {
        Ok(range) => {
            unsafe {
                (*range_out) = CCameraParameterRange {
                    min: range.min,
                    max: range.max,
                    step: range.step,
                    default_value: range.default,
                    current: range.current,
                };
            }
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("获取右相机参数范围失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 获取单相机参数范围
#[no_mangle]
pub extern "C" fn smartscope_get_single_camera_parameter_range(
    property: CCameraProperty,
    range_out: *mut CCameraParameterRange,
) -> c_int {
    if range_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let rust_property: CameraProperty = property.into();
    match app_state.get_single_camera_parameter_range(&rust_property) {
        Ok(range) => {
            unsafe {
                (*range_out) = CCameraParameterRange {
                    min: range.min,
                    max: range.max,
                    step: range.step,
                    default_value: range.default,
                    current: range.current,
                };
            }
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("获取单相机参数范围失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 重置左相机参数到默认值
#[no_mangle]
pub extern "C" fn smartscope_reset_left_camera_parameters() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.reset_left_camera_parameters() {
        Ok(_) => {
            tracing::info!("重置左相机参数成功");
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("重置左相机参数失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 重置右相机参数到默认值
#[no_mangle]
pub extern "C" fn smartscope_reset_right_camera_parameters() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.reset_right_camera_parameters() {
        Ok(_) => {
            tracing::info!("重置右相机参数成功");
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("重置右相机参数失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 重置单相机参数到默认值
#[no_mangle]
pub extern "C" fn smartscope_reset_single_camera_parameters() -> c_int {
    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    match app_state.reset_single_camera_parameters() {
        Ok(_) => {
            tracing::info!("重置单相机参数成功");
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("重置单相机参数失败: {}", e);
            ErrorCode::Error as c_int
        }
    }
}
