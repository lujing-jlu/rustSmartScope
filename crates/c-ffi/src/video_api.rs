use std::os::raw::c_int;

use crate::state::get_app_state;
use crate::types::ErrorCode;

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

