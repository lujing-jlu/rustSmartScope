use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};

use smartscope_core::config::SmartScopeConfig;

use crate::state::get_app_state;
use crate::types::ErrorCode;

/// 检查系统是否已初始化
#[no_mangle]
#[allow(static_mut_refs)]
pub extern "C" fn smartscope_is_initialized() -> bool {
    unsafe {
        // 通过 get_app_state 无法区分未初始化与错误，这里直接访问内部状态更直观
        extern crate smartscope_core;
        // 访问 APP_STATE 需与 state 模块一致
        use crate::state::APP_STATE;
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
            app_state.config_path = Some(path_str.to_string());
            tracing::info!("Configuration loaded from: {}", path_str);
            ErrorCode::Success as c_int
        }
        Err(e) => {
            // 配置缺失或损坏：写入默认配置并继续
            tracing::warn!("配置加载失败，将写入默认配置: {}", e);
            let default_cfg = SmartScopeConfig::default();
            *app_state.config.write().unwrap() = default_cfg.clone();
            app_state.config_path = Some(path_str.to_string());
            match default_cfg.save_to_file(path_str) {
                Ok(_) => {
                    tracing::info!("默认配置已写入: {}", path_str);
                    ErrorCode::Success as c_int
                }
                Err(e2) => {
                    tracing::error!("写入默认配置失败: {}", e2);
                    ErrorCode::from(e2) as c_int
                }
            }
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

/// 启用配置文件热重载
#[no_mangle]
pub extern "C" fn smartscope_enable_config_hot_reload(config_path: *const c_char) -> c_int {
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

    match app_state.enable_config_hot_reload(path_str) {
        Ok(_) => {
            tracing::info!("配置热重载已启用: {}", path_str);
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("启用配置热重载失败: {}", e);
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
