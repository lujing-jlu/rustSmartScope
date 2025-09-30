//! SmartScope Minimal C FFI Interface
//!
//! 提供最基础的C兼容接口，用于演示Rust-C++桥接框架

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};
use std::sync::Once;

use smartscope_core::{AppState, SmartScopeError, AppConfig};

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

/// 初始化SmartScope系统
#[no_mangle]
pub extern "C" fn smartscope_init() -> c_int {
    INIT.call_once(|| {
        // 初始化日志系统
        tracing_subscriber::fmt::init();

        unsafe {
            let mut state = AppState::new();
            match state.initialize() {
                Ok(_) => {
                    APP_STATE = Some(state);
                },
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
        APP_STATE.as_mut().ok_or_else(|| {
            SmartScopeError::Unknown("SmartScope not initialized".to_string())
        })
    }
}

/// 检查系统是否已初始化
#[no_mangle]
pub extern "C" fn smartscope_is_initialized() -> bool {
    unsafe {
        APP_STATE.as_ref().map(|s| s.is_initialized()).unwrap_or(false)
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

    match AppConfig::load_from_file(path_str) {
        Ok(config) => {
            *app_state.config.write().unwrap() = config;
            tracing::info!("Configuration loaded from: {}", path_str);
            ErrorCode::Success as c_int
        },
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
        },
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