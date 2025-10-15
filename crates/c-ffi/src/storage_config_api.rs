use std::ffi::CStr;
use std::os::raw::{c_char, c_int};

use crate::state::get_app_state;
use crate::types::ErrorCode;

/// 与SmartScopeConfig.storage相关的FFI接口

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CStorageLocation {
    Internal = 0,
    External = 1,
}

/// 获取存储配置（JSON字符串）
#[no_mangle]
pub extern "C" fn smartscope_storage_get_config_json() -> *mut c_char {
    let app_state = match get_app_state() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let storage = app_state.get_storage_config();
    let json = match serde_json::to_string(&storage) {
        Ok(s) => s,
        Err(_) => "{}".to_string(),
    };
    match std::ffi::CString::new(json) {
        Ok(c) => c.into_raw(),
        Err(_) => std::ffi::CString::new("{}").unwrap().into_raw(),
    }
}

/// 设置存储位置
#[no_mangle]
pub extern "C" fn smartscope_storage_set_location(location: u32) -> c_int {
    let app_state = match get_app_state() {
        Ok(s) => s,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let loc = if location == CStorageLocation::External as u32 { smartscope_core::config::StorageLocation::External } else { smartscope_core::config::StorageLocation::Internal };

    let _ = app_state.set_storage_location(loc);

    // 自动保存（如果已设置路径）
    if let Some(path) = &app_state.config_path {
        let cfg = app_state.config.read().unwrap().clone();
        if let Err(e) = cfg.save_to_file(path) {
            tracing::warn!("save storage config failed: {}", e);
        }
    }

    ErrorCode::Success as c_int
}

/// 设置外置设备（设备路径，如 /dev/sda1）
#[no_mangle]
pub extern "C" fn smartscope_storage_set_external_device(device_path: *const c_char) -> c_int {
    if device_path.is_null() { return ErrorCode::Error as c_int; }
    let app_state = match get_app_state() { Ok(s)=>s, Err(_)=> return ErrorCode::Error as c_int };

    let path_str = unsafe { match CStr::from_ptr(device_path).to_str() { Ok(s)=>s.to_string(), Err(_)=> return ErrorCode::Error as c_int } };

    let _ = app_state.set_storage_external_device(Some(path_str));

    if let Some(path) = &app_state.config_path {
        let cfg = app_state.config.read().unwrap().clone();
        if let Err(e) = cfg.save_to_file(path) {
            tracing::warn!("save storage config failed: {}", e);
        }
    }
    ErrorCode::Success as c_int
}

/// 设置机内存储基准目录
#[no_mangle]
pub extern "C" fn smartscope_storage_set_internal_base_path(path: *const c_char) -> c_int {
    if path.is_null() { return ErrorCode::Error as c_int; }
    let app_state = match get_app_state() { Ok(s)=>s, Err(_)=> return ErrorCode::Error as c_int };
    let s = unsafe { match CStr::from_ptr(path).to_str() { Ok(s)=>s.to_string(), Err(_)=> return ErrorCode::Error as c_int } };
    let _ = app_state.set_storage_internal_base_path(s);
    if let Some(p) = &app_state.config_path {
        let cfg = app_state.config.read().unwrap().clone();
        let _ = cfg.save_to_file(p);
    }
    ErrorCode::Success as c_int
}

/// 设置外置相对路径
#[no_mangle]
pub extern "C" fn smartscope_storage_set_external_relative_path(path: *const c_char) -> c_int {
    if path.is_null() { return ErrorCode::Error as c_int; }
    let app_state = match get_app_state() { Ok(s)=>s, Err(_)=> return ErrorCode::Error as c_int };
    let s = unsafe { match CStr::from_ptr(path).to_str() { Ok(s)=>s.to_string(), Err(_)=> return ErrorCode::Error as c_int } };
    let _ = app_state.set_storage_external_relative_path(s);
    if let Some(p) = &app_state.config_path {
        let cfg = app_state.config.read().unwrap().clone();
        let _ = cfg.save_to_file(p);
    }
    ErrorCode::Success as c_int
}

/// 设置是否自动恢复到外置存储
#[no_mangle]
pub extern "C" fn smartscope_storage_set_auto_recover(enabled: bool) -> c_int {
    let app_state = match get_app_state() { Ok(s)=>s, Err(_)=> return ErrorCode::Error as c_int };
    {
        let mut cfg = app_state.config.write().unwrap();
        cfg.storage.auto_recover = enabled;
    }
    if let Some(p) = &app_state.config_path {
        let cfg = app_state.config.read().unwrap().clone();
        if let Err(e) = cfg.save_to_file(p) {
            tracing::warn!("save storage config failed: {}", e);
        }
    }
    ErrorCode::Success as c_int
}
