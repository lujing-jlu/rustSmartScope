//! C FFI interface for video recording

use crate::{RecorderConfig, RecordingService, VideoCodec, HardwareAccelType, X11ScreenCapturer};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::sync::{Mutex, atomic::{AtomicBool, Ordering}};
use std::thread;

lazy_static::lazy_static! {
    static ref RECORDING_SERVICE: Mutex<Option<RecordingService>> = Mutex::new(None);
    static ref SCREEN_CAPTURER: Mutex<Option<X11ScreenCapturer>> = Mutex::new(None);
    static ref LAST_ERROR: Mutex<Option<CString>> = Mutex::new(None);
    static ref SCREENREC_THREAD: Mutex<Option<std::thread::JoinHandle<()>>> = Mutex::new(None);
}

static SCREENREC_RUNNING: AtomicBool = AtomicBool::new(false);

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

/// Initialize screen capturer (Rust-callable; C export via c-ffi)
pub fn smartscope_recorder_init_capturer() -> i32 {
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
pub fn smartscope_recorder_get_screen_size(width: *mut u32, height: *mut u32) -> i32 {
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
pub fn smartscope_recorder_init(config: *const RecorderConfigC) -> i32 {
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
pub fn smartscope_recorder_start() -> i32 {
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
pub fn smartscope_recorder_init_simple(
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
pub fn smartscope_recorder_capture_frame() -> i32 {
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
pub fn smartscope_recorder_submit_rgb888(
    data: *const u8,
    data_len: usize,
    width: u32,
    height: u32,
    timestamp_us: i64,
) -> i32 {
    if data.is_null() { return -1; }
    let guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = guard.as_ref() {
        // 校验分辨率与服务配置一致，否则丢弃该帧以避免FFmpeg原始帧尺寸不匹配
        let cfg = service.config();
        if width != cfg.width || height != cfg.height {
            log::warn!(
                "Drop mismatched frame: input={}x{}, expected={}x{}",
                width, height, cfg.width, cfg.height
            );
            return 0;
        }
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
pub fn smartscope_recorder_stop() -> i32 {
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
pub fn smartscope_recorder_get_stats(stats: *mut RecorderStatsC) -> i32 {
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
pub fn smartscope_recorder_is_recording() -> i32 {
    let service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = service_guard.as_ref() {
        if service.is_running() { 1 } else { 0 }
    } else {
        0
    }
}

/// Get current queue size
pub fn smartscope_recorder_get_queue_size() -> i32 {
    let service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = service_guard.as_ref() {
        service.queue_size() as i32
    } else {
        -1
    }
}

/// Get last error message
pub fn smartscope_recorder_get_last_error() -> *const c_char {
    let error_guard = LAST_ERROR.lock().unwrap();
    if let Some(error) = error_guard.as_ref() {
        error.as_ptr()
    } else {
        std::ptr::null()
    }
}

/// Shutdown recorder and free resources
pub fn smartscope_recorder_shutdown() {
    let mut service_guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(mut service) = service_guard.take() {
        let _ = service.stop();
    }

    let mut capturer_guard = SCREEN_CAPTURER.lock().unwrap();
    *capturer_guard = None;

    log::info!("Recorder shutdown");
}

/// Start background X11 screen recording with given parameters (non-blocking)
pub fn smartscope_screenrec_start(output_path: *const c_char, fps: u32, bitrate: u64) -> i32 {
    if output_path.is_null() { set_last_error("Null output path"); return -1; }
    if SCREENREC_RUNNING.swap(true, Ordering::SeqCst) { set_last_error("Screen recorder already running"); return -1; }

    let path = unsafe { CStr::from_ptr(output_path).to_string_lossy().into_owned() };

    // Initialize capturer locally
    let capturer = match X11ScreenCapturer::new() {
        Ok(c) => c,
        Err(e) => { set_last_error(&format!("Init capturer failed: {}", e)); SCREENREC_RUNNING.store(false, Ordering::SeqCst); return -1; }
    };
    let (w, h) = capturer.dimensions();

    // Build config and service
    let config = RecorderConfig {
        width: w,
        height: h,
        // 不强制帧率：传入0则采用自适应采样，名义值仍会以15作为输入r
        fps: if fps == 0 { 15 } else { fps },
        bitrate: if bitrate == 0 { 4_000_000 } else { bitrate },
        codec: VideoCodec::H264,
        // Try HW if env says so; otherwise default None and auto-fallback inside ffmpeg wrapper
        hardware_accel: match std::env::var("VR_HW").ok().as_deref() { Some("vaapi") => HardwareAccelType::VaApi, Some("rkmpp") => HardwareAccelType::RkMpp, _ => HardwareAccelType::None },
        max_queue_size: 120, // 4s @ 30fps
        output_path: path,
    };

    let mut service = RecordingService::new(config);
    if let Err(e) = service.start() {
        set_last_error(&format!("Start service failed: {}", e));
        SCREENREC_RUNNING.store(false, Ordering::SeqCst);
        return -1;
    }

    // Put service into global
    *RECORDING_SERVICE.lock().unwrap() = Some(service);

    // Spawn background capture loop
    let handle = thread::spawn(move || {
        let mut cap = capturer;
        let base_interval = std::time::Duration::from_micros(1_000_000 / (fps.max(1)) as u64);
        // 自适应采样间隔：根据队列占用率放慢采样，避免长期丢帧导致视频时长很短
        let mut interval = base_interval;
        while SCREENREC_RUNNING.load(Ordering::SeqCst) {
            let start = std::time::Instant::now();
            match cap.capture_frame() {
                Ok(frame) => {
                    let guard = RECORDING_SERVICE.lock().unwrap();
                    if let Some(service) = guard.as_ref() {
                        if let Err(e) = service.submit_frame(frame) {
                            log::error!("submit_frame failed: {}", e);
                        }
                        // 基于队列长度调整采样间隔
                        let qlen = service.queue_size() as f64;
                        let qmax = service.config().max_queue_size as f64;
                        let occ = if qmax > 0.0 { qlen / qmax } else { 0.0 };
                        let scale = if occ >= 0.7 { 3.0 } else if occ >= 0.5 { 2.0 } else if occ >= 0.3 { 1.5 } else { 1.0 };
                        // 轻微平滑（避免跳变）：interval = 0.8*interval + 0.2*(base*scale)
                        let target = base_interval.mul_f64(scale);
                        let cur = interval.as_secs_f64();
                        let tgt = target.as_secs_f64();
                        let blended = cur * 0.8 + tgt * 0.2;
                        interval = std::time::Duration::from_secs_f64(blended);
                    } else {
                        log::warn!("Recording service missing; stopping screen loop");
                        break;
                    }
                }
                Err(e) => {
                    log::error!("capture_frame failed: {}", e);
                }
            }
            let elapsed = start.elapsed();
            if elapsed < interval { thread::sleep(interval - elapsed); }
        }
    });

    *SCREENREC_THREAD.lock().unwrap() = Some(handle);
    0
}

/// Stop background screen recording
pub fn smartscope_screenrec_stop() -> i32 {
    if !SCREENREC_RUNNING.swap(false, Ordering::SeqCst) {
        return 0;
    }

    // Join capture thread
    if let Some(handle) = SCREENREC_THREAD.lock().unwrap().take() {
        let _ = handle.join();
    }

    // Stop service
    let mut guard = RECORDING_SERVICE.lock().unwrap();
    if let Some(service) = guard.as_mut() {
        if let Err(e) = service.stop() { set_last_error(&format!("Stop service failed: {}", e)); return -1; }
    }
    0
}
