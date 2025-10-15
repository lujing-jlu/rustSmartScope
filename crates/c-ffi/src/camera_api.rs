use std::os::raw::c_int;
use std::sync::Mutex;

use crate::state::get_app_state;
use crate::types::{CCameraFrame, CCameraMode, CCameraStatus, ErrorCode};

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

    if let Some(frame) = app_state.get_processed_left_frame() {
        let timestamp = frame
            .timestamp
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default();

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

    if let Some(frame) = app_state.get_processed_right_frame() {
        let timestamp = frame
            .timestamp
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default();

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

