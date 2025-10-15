use std::os::raw::c_int;

use crate::state::get_app_state;
use crate::types::{CCameraParameterRange, CCameraProperty, ErrorCode};

use smartscope_core::camera::CameraProperty;

/// 设置左相机参数
#[no_mangle]
pub extern "C" fn smartscope_set_left_camera_parameter(
    property: CCameraProperty,
    value: i32,
) -> c_int {
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
pub extern "C" fn smartscope_set_right_camera_parameter(
    property: CCameraProperty,
    value: i32,
) -> c_int {
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
pub extern "C" fn smartscope_set_single_camera_parameter(
    property: CCameraProperty,
    value: i32,
) -> c_int {
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

