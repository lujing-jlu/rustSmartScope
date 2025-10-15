use std::ffi::CStr;
use std::os::raw::c_int;
use std::os::raw::c_char;

use smartscope_core::log_from_cpp;
use smartscope_core::LogLevel;

use crate::types::{CLogLevel, ErrorCode};

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

/// 设置日志级别（占位，保持接口不变）
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

