//! C FFI interface for video recording

use crate::{RecorderConfig, RecordingService, VideoCodec, HardwareAccelType, X11ScreenCapturer};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::sync::Mutex;

lazy_static::lazy_static! {
    static ref RECORDING_SERVICE: Mutex<Option<RecordingService>> = Mutex::new(None);
    static ref SCREEN_CAPTURER: Mutex<Option<X11ScreenCapturer>> = Mutex::new(None);
    static ref LAST_ERROR: Mutex<Option<CString>> = Mutex::new(None);
}

fn set_last_error(msg: &str) {
    let c_msg = CString::new(msg).unwrap_or_else(|_| CString::new("Unknown error").unwrap());
    *LAST_ERROR.lock().unwrap() = Some(c_msg);
    log::error!("Recorder error: {}", msg);
}

#[repr(C)]
pub struct RecorderConfigC {
    pub width: u32,
    pub height: u32,
    pub fps: u32,
    pub bitrate: u64,
    pub codec: u32,           // 0=H264, 1=H265, 2=VP8, 3=VP9
    pub hardware_accel: u32,  // 0=None, 1=RkMpp, 2=VaApi
    pub max_queue_size: u32,
    pub output_path: *const c_char,
}

#[repr(C)]
pub struct RecorderStatsC {
    pub frames_encoded: u64,
    pub frames_dropped: u64,
    pub bytes_written: u64,
    pub duration_ms: u64,
    pub is_recording: u32,
}

/// Initialize screen capturer
#[no_mangle]
pub extern "C" fn smartscope_recorder_init_capturer() -> i32 {
    match X11ScreenCapturer::new() {
        Ok(capturer) => {
            *SCREEN_CAPTURER.lock().unwrap() = Some(capturer);
            log::info!("Screen capturer initialized");
            0
        }
        Err(e) => {
            set_last_error(&format!("Failed to initialize screen capturer: {}", e));
            -1
        }
    }
}

/// Get screen dimensions
#[no_mangle]
pub extern "C" fn smartscope_recorder_get_screen_size(width: *mut u32, height: *mut u32) -> i32 {
    let capturer_guard = SCREEN_CAPTURER.lock().unwrap();
    if let Some(capturer) = capturer_guard.as_ref() {
        let (w, h) = capturer.dimensions();
        unsafe {
            if !width.is_null() {
                *width = w;
            }
            if !height.is_null() {
                *height = h;
            }
        }
        0
    } else {
        set_last_error("Screen capturer not initialized");
        -1
    }
}

/// Initialize recording service with configuration
#[no_mangle]
pub extern "C" fn smartscope_recorder_init(config: *const RecorderConfigC) -> i32 {
    if config.is_null() {
        set_last_error("Null config pointer");
        return -1;
    }

    let config_c = unsafe { &*config };

    let output_path = if config_c.output_path.is_null() {
        "recording.mp4".to_string()
    } else {
        unsafe {
            CStr::from_ptr(config_c.output_path)
                .to_string_lossy()
                .into_owned()
        }
    };

    let codec = match config_c.codec {
        0 => VideoCodec::H264,
        1 => VideoCodec::H265,
        2 => VideoCodec::VP8,
        3 => VideoCodec::VP9,
        _ => VideoCodec::H264,
    };

    let hardware_accel = match config_c.hardware_accel {
        0 => HardwareAccelType::None,
        1 => HardwareAccelType::RkMpp,
        2 => HardwareAccelType::VaApi,
        _ => HardwareAccelType::RkMpp,
    };

    let config = RecorderConfig {
        width: config_c.width,
        height: config_c.height,
        fps: config_c.fps,
        bitrate: config_c.bitrate,
        codec,
        hardware_accel,
        max_queue_size: config_c.max_queue_size as usize,
        output_path,
    };

    let service = RecordingService::new(config);
    *RECORDING_SERVICE.lock().unwrap() = Some(service);

    log::info!("Recording service initialized");
    0
}

/// Start recording
#[no_mangle]
pub extern "C" fn smartscope_recorder_start() -> i32 {
    let mut service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = service_guard.as_mut() {
        match service.start() {
            Ok(_) => {
                log::info!("Recording started");
                0
            }
            Err(e) => {
                set_last_error(&format!("Failed to start recording: {}", e));
                -1
            }
        }
    } else {
        set_last_error("Recording service not initialized");
        -1
    }
}

/// Convenience init without struct from C/C++
#[no_mangle]
pub extern "C" fn smartscope_recorder_init_simple(
    output_path: *const c_char,
    width: u32,
    height: u32,
    fps: u32,
    bitrate: u64,
) -> i32 {
    if output_path.is_null() { return -1; }

    let path = unsafe { CStr::from_ptr(output_path).to_string_lossy().into_owned() };

    let config = RecorderConfig {
        width,
        height,
        fps,
        bitrate,
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::RkMpp,
        max_queue_size: 60,
        output_path: path,
    };

    let service = RecordingService::new(config);
    *RECORDING_SERVICE.lock().unwrap() = Some(service);
    0
}

/// Capture and submit a frame from screen
#[no_mangle]
pub extern "C" fn smartscope_recorder_capture_frame() -> i32 {
    let mut capturer_guard = SCREEN_CAPTURER.lock().unwrap();
    let service_guard = RECORDING_SERVICE.lock().unwrap();

    if let (Some(capturer), Some(service)) = (capturer_guard.as_mut(), service_guard.as_ref()) {
        match capturer.capture_frame() {
            Ok(frame) => {
                match service.submit_frame(frame) {
                    Ok(_) => 0,
                    Err(e) => {
                        set_last_error(&format!("Failed to submit frame: {}", e));
                        -1
                    }
                }
            }
            Err(e) => {
                set_last_error(&format!("Failed to capture frame: {}", e));
                -1
            }
        }
    } else {
        set_last_error("Capturer or service not initialized");
        -1
    }
}

/// Submit an RGB888 frame from external producer (e.g., camera path)
#[no_mangle]
pub extern "C" fn smartscope_recorder_submit_rgb888(
    data: *const u8,
    data_len: usize,
    width: u32,
    height: u32,
    timestamp_us: i64,
) -> i32 {
    if data.is_null() { return -1; }
    let guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = guard.as_ref() {
        // Copy into Vec<u8> (contiguous RGB24 expected: width*height*3)
        let slice = unsafe { std::slice::from_raw_parts(data, data_len) };
        let mut v = Vec::with_capacity((width as usize) * (height as usize) * 3);
        v.extend_from_slice(slice);

        let frame = crate::VideoFrame {
            data: std::sync::Arc::new(v),
            width,
            height,
            format: crate::VideoPixelFormat::RGB888,
            timestamp_us,
            frame_number: 0,
        };

        match service.submit_frame(frame) {
            Ok(_) => 0,
            Err(e) => {
                set_last_error(&format!("Failed to submit frame: {}", e));
                -1
            }
        }
    } else {
        set_last_error("Recording service not initialized");
        -1
    }
}

/// Stop recording
#[no_mangle]
pub extern "C" fn smartscope_recorder_stop() -> i32 {
    let mut service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = service_guard.as_mut() {
        match service.stop() {
            Ok(_) => {
                log::info!("Recording stopped");
                0
            }
            Err(e) => {
                set_last_error(&format!("Failed to stop recording: {}", e));
                -1
            }
        }
    } else {
        set_last_error("Recording service not initialized");
        -1
    }
}

/// Get recording statistics
#[no_mangle]
pub extern "C" fn smartscope_recorder_get_stats(stats: *mut RecorderStatsC) -> i32 {
    if stats.is_null() {
        set_last_error("Null stats pointer");
        return -1;
    }

    let service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = service_guard.as_ref() {
        let s = service.stats();
        unsafe {
            (*stats).frames_encoded = s.frames_encoded;
            (*stats).frames_dropped = s.frames_dropped;
            (*stats).bytes_written = s.bytes_written;
            (*stats).duration_ms = s.duration_ms;
            (*stats).is_recording = if s.is_recording { 1 } else { 0 };
        }
        0
    } else {
        set_last_error("Recording service not initialized");
        -1
    }
}

/// Check if recording is active
#[no_mangle]
pub extern "C" fn smartscope_recorder_is_recording() -> i32 {
    let service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = service_guard.as_ref() {
        if service.is_running() { 1 } else { 0 }
    } else {
        0
    }
}

/// Get current queue size
#[no_mangle]
pub extern "C" fn smartscope_recorder_get_queue_size() -> i32 {
    let service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = service_guard.as_ref() {
        service.queue_size() as i32
    } else {
        -1
    }
}

/// Get last error message
#[no_mangle]
pub extern "C" fn smartscope_recorder_get_last_error() -> *const c_char {
    let error_guard = LAST_ERROR.lock().unwrap();
    if let Some(error) = error_guard.as_ref() {
        error.as_ptr()
    } else {
        std::ptr::null()
    }
}

/// Shutdown recorder and free resources
#[no_mangle]
pub extern "C" fn smartscope_recorder_shutdown() {
    let mut service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(mut service) = service_guard.take() {
        let _ = service.stop();
    }

    let mut capturer_guard = SCREEN_CAPTURER.lock().unwrap();
    *capturer_guard = None;

    log::info!("Recorder shutdown");
}
