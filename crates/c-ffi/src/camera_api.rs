use std::os::raw::c_int;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::path::Path;
use std::fs::{self, File};
use std::io::Write;
use std::time::{Duration, Instant};
use std::thread;
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
    // Stereo snapshot dedicated buffers to avoid races with other getters
    static ref SNAP_LEFT_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());
    static ref SNAP_RIGHT_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());
    static ref SNAP_RAW_LEFT_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());
    static ref SNAP_RAW_RIGHT_BUFFER: Mutex<Vec<u8>> = Mutex::new(Vec::new());
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

fn save_rgb_png(dir: &Path, name: &str, width: u32, height: u32, rgb: &[u8]) -> Result<(), String> {
    let mut img = match image::RgbImage::from_raw(width, height, rgb.to_vec()) {
        Some(i) => i,
        None => return Err("invalid rgb buffer".into()),
    };
    let dynimg = image::DynamicImage::ImageRgb8(std::mem::take(&mut img));
    let path = dir.join(name);
    dynimg
        .save(&path)
        .map_err(|e| format!("save png failed: {}", e))
}

fn save_raw_frame(dir: &Path, stem: &str, frame: &smartscope_core::camera::VideoFrame) -> Result<(), String> {
    fs::create_dir_all(dir).map_err(|e| format!("mkdir failed: {}", e))?;
    if frame.format == 0 {
        // RGB888
        save_rgb_png(dir, &format!("{}{}.png", stem, ""), frame.width, frame.height, &frame.data)
    } else {
        // Assume MJPEG or compressed; dump as .jpg
        let path = dir.join(format!("{}{}.jpg", stem, ""));
        let mut f = File::create(&path).map_err(|e| format!("create jpg failed: {}", e))?;
        f.write_all(&frame.data)
            .map_err(|e| format!("write jpg failed: {}", e))
    }
}

fn process_and_save(dir: &Path, name: &str, raw: &smartscope_core::camera::VideoFrame, is_left: bool, app_state: &mut smartscope_core::AppState) -> Result<(), String> {
    let pipeline_opt = app_state.image_pipeline.as_ref().cloned();
    if let Some(p) = pipeline_opt {
        if let Ok(mut pipeline) = p.lock() {
            let distortion_enabled = *app_state.distortion_correction_enabled.read().unwrap();
            pipeline.set_distortion_correction_enabled(distortion_enabled);
            match pipeline.process_mjpeg_frame(&raw.data, is_left) {
                Ok((rgb, w, h)) => {
                    return save_rgb_png(dir, name, w, h, &rgb);
                }
                Err(e) => {
                    return Err(format!("pipeline failed: {}", e));
                }
            }
        }
    }
    Err("pipeline not available".into())
}

/// Capture stereo RAW and processed frames and save to directory.
#[no_mangle]
pub extern "C" fn smartscope_capture_stereo_to_dir(dir: *const c_char) -> c_int {
    if dir.is_null() {
        return ErrorCode::Error as c_int;
    }
    let cstr = unsafe { CStr::from_ptr(dir) };
    let dir_str = match cstr.to_str() { Ok(s) => s, Err(_) => return ErrorCode::Error as c_int };
    let path = Path::new(dir_str);
    let app_state = match get_app_state() { Ok(s) => s, Err(_) => return ErrorCode::Error as c_int };

    // Check camera availability
    let status_opt = app_state.get_camera_status();
    let (left_avail, right_avail) = status_opt
        .map(|st| (st.left_camera_connected, st.right_camera_connected))
        .unwrap_or((false, false));

    if !left_avail && !right_avail {
        tracing::error!("stereo capture: both cameras unavailable");
        return ErrorCode::Error as c_int;
    }

    // Wait for frames if connected
    let start = Instant::now();
    let timeout = Duration::from_millis(2000);
    let mut left_raw = None;
    let mut right_raw = None;

    while start.elapsed() < timeout {
        // Pump camera frames while UI thread is blocked in this FFI call
        app_state.process_camera_frames();
        if left_avail && left_raw.is_none() { left_raw = app_state.get_left_camera_frame(); }
        if right_avail && right_raw.is_none() { right_raw = app_state.get_right_camera_frame(); }
        let left_ok = !left_avail || left_raw.is_some();
        let right_ok = !right_avail || right_raw.is_some();
        if left_ok && right_ok { break; }
        thread::sleep(Duration::from_millis(20));
    }

    // If a connected side still missing, fail
    if left_avail && left_raw.is_none() {
        tracing::error!("stereo capture: left frame not ready within timeout");
        return ErrorCode::Error as c_int;
    }
    if right_avail && right_raw.is_none() {
        tracing::error!("stereo capture: right frame not ready within timeout");
        return ErrorCode::Error as c_int;
    }

    // Save what is available according to availability
    let mut saved = 0usize;
    if let Some(ref lf) = left_raw {
        if save_raw_frame(path, "left_original", lf).is_ok() { saved += 1; }
        if process_and_save(path, "left_processed.png", lf, true, app_state).is_ok() { saved += 1; }
    }
    if let Some(ref rf) = right_raw {
        if save_raw_frame(path, "right_original", rf).is_ok() { saved += 1; }
        if process_and_save(path, "right_processed.png", rf, false, app_state).is_ok() { saved += 1; }
    }

    if saved == 0 { ErrorCode::Error as c_int } else { ErrorCode::Success as c_int }
}

/// Capture single RAW and processed frames and save to directory.
#[no_mangle]
pub extern "C" fn smartscope_capture_single_to_dir(dir: *const c_char) -> c_int {
    if dir.is_null() {
        return ErrorCode::Error as c_int;
    }
    let cstr = unsafe { CStr::from_ptr(dir) };
    let dir_str = match cstr.to_str() { Ok(s) => s, Err(_) => return ErrorCode::Error as c_int };
    let path = Path::new(dir_str);
    let app_state = match get_app_state() { Ok(s) => s, Err(_) => return ErrorCode::Error as c_int };

    // Wait for single frame
    let start = Instant::now();
    let timeout = Duration::from_millis(1500);
    let mut raw = app_state.get_single_camera_frame();
    while raw.is_none() && start.elapsed() < timeout {
        app_state.process_camera_frames();
        thread::sleep(Duration::from_millis(20));
        raw = app_state.get_single_camera_frame();
    }

    let mut saved = 0usize;
    if let Some(ref rf) = raw {
        if save_raw_frame(path, "single_original", rf).is_ok() { saved += 1; }
        if process_and_save(path, "single_processed.png", rf, true, app_state).is_ok() { saved += 1; }
    }
    if saved == 0 { ErrorCode::Error as c_int } else { ErrorCode::Success as c_int }
}
/// Atomically grab current processed frames from left and right cameras (RGB), suitable for capture.
/// Returns 0 on success, non-zero on failure (e.g., not in stereo mode or missing frames).
#[no_mangle]
pub extern "C" fn smartscope_stereo_snapshot(left_out: *mut CCameraFrame, right_out: *mut CCameraFrame) -> c_int {
    if left_out.is_null() || right_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    // Fetch processed frames (RGB) from both sides
    let left = match app_state.get_processed_left_frame() {
        Some(f) => f,
        None => return ErrorCode::Error as c_int,
    };
    let right = match app_state.get_processed_right_frame() {
        Some(f) => f,
        None => return ErrorCode::Error as c_int,
    };

    // Copy to stable buffers
    let mut lb = SNAP_LEFT_BUFFER.lock().unwrap();
    lb.clear();
    lb.extend_from_slice(&left.data);

    let mut rb = SNAP_RIGHT_BUFFER.lock().unwrap();
    rb.clear();
    rb.extend_from_slice(&right.data);

    let lts = left
        .timestamp
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();
    let rts = right
        .timestamp
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();

    unsafe {
        (*left_out) = CCameraFrame {
            data: lb.as_ptr(),
            data_len: lb.len(),
            width: left.width,
            height: left.height,
            format: left.format,
            timestamp_sec: lts.as_secs(),
            timestamp_nsec: lts.subsec_nanos(),
        };
        (*right_out) = CCameraFrame {
            data: rb.as_ptr(),
            data_len: rb.len(),
            width: right.width,
            height: right.height,
            format: right.format,
            timestamp_sec: rts.as_secs(),
            timestamp_nsec: rts.subsec_nanos(),
        };
    }

    ErrorCode::Success as c_int
}

/// Atomically grab current RAW frames (as captured, e.g., MJPEG) for left and right cameras.
/// Returns 0 on success.
#[no_mangle]
pub extern "C" fn smartscope_stereo_raw_snapshot(left_out: *mut CCameraFrame, right_out: *mut CCameraFrame) -> c_int {
    if left_out.is_null() || right_out.is_null() {
        return ErrorCode::Error as c_int;
    }

    let app_state = match get_app_state() {
        Ok(state) => state,
        Err(_) => return ErrorCode::Error as c_int,
    };

    let left = match app_state.get_left_camera_frame() {
        Some(f) => f,
        None => return ErrorCode::Error as c_int,
    };
    let right = match app_state.get_right_camera_frame() {
        Some(f) => f,
        None => return ErrorCode::Error as c_int,
    };

    let mut lb = SNAP_RAW_LEFT_BUFFER.lock().unwrap();
    lb.clear();
    lb.extend_from_slice(&left.data);
    let mut rb = SNAP_RAW_RIGHT_BUFFER.lock().unwrap();
    rb.clear();
    rb.extend_from_slice(&right.data);

    let lts = left
        .timestamp
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();
    let rts = right
        .timestamp
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();

    unsafe {
        (*left_out) = CCameraFrame {
            data: lb.as_ptr(),
            data_len: lb.len(),
            width: left.width,
            height: left.height,
            format: left.format,
            timestamp_sec: lts.as_secs(),
            timestamp_nsec: lts.subsec_nanos(),
        };
        (*right_out) = CCameraFrame {
            data: rb.as_ptr(),
            data_len: rb.len(),
            width: right.width,
            height: right.height,
            format: right.format,
            timestamp_sec: rts.as_secs(),
            timestamp_nsec: rts.subsec_nanos(),
        };
    }

    ErrorCode::Success as c_int
}
