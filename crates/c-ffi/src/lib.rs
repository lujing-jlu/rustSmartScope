//! SmartScope Minimal C FFI Interface
//!
//! 提供最基础的C兼容接口，用于演示Rust-C++桥接框架

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};
use std::sync::{Mutex, Once};

use smartscope_core::config::SmartScopeConfig;
use smartscope_core::{
    init_global_logger, log_from_cpp, AppState, CameraMode, CameraStatus, LogLevel, LogRotation,
    LoggerConfig, SmartScopeError, VideoFrame,
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
pub extern "C" fn smartscope_shutdown() -> c_int {
    unsafe {
        if let Some(mut state) = APP_STATE.take() {
            state.shutdown();
        }
    }

    ErrorCode::Success as c_int
}

/// 获取应用状态指针（内部使用）
fn get_app_state() -> Result<&'static mut AppState, SmartScopeError> {
    unsafe {
        APP_STATE
            .as_mut()
            .ok_or_else(|| SmartScopeError::Unknown("SmartScope not initialized".to_string()))
    }
}

/// 检查系统是否已初始化
#[no_mangle]
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

/// 获取左相机最新帧
#[no_mangle]
pub extern "C" fn smartscope_get_left_frame(frame_out: *mut CCameraFrame) -> c_int {
    if frame_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    if let Some(frame) = app_state.get_left_camera_frame() {
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

/// 获取右相机最新帧
#[no_mangle]
pub extern "C" fn smartscope_get_right_frame(frame_out: *mut CCameraFrame) -> c_int {
    if frame_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    if let Some(frame) = app_state.get_right_camera_frame() {
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
