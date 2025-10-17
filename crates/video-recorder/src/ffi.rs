//! C FFI interface for screen recording

use crate::ScreenRecorder;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::sync::Mutex;

lazy_static::lazy_static! {
    static ref RECORDER: Mutex<Option<ScreenRecorder>> = Mutex::new(None);
    static ref LAST_ERROR: Mutex<Option<CString>> = Mutex::new(None);
}

fn set_last_error(msg: &str) {
    let c_msg = CString::new(msg).unwrap_or_else(|_| CString::new("Unknown error").unwrap());
    *LAST_ERROR.lock().unwrap() = Some(c_msg);
    log::error!("Screen recorder error: {}", msg);
}

/// Initialize screen recorder
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_init() -> i32 {
    match ScreenRecorder::new() {
        Ok(recorder) => {
            *RECORDER.lock().unwrap() = Some(recorder);
            log::info!("Screen recorder initialized");
            0
        }
        Err(e) => {
            set_last_error(&format!("Failed to initialize recorder: {}", e));
            -1
        }
    }
}

/// Set screen dimensions
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_set_dimensions(width: u32, height: u32) -> i32 {
    let mut recorder_guard = RECORDER.lock().unwrap();
    if let Some(ref mut recorder) = recorder_guard.as_mut() {
        recorder.set_dimensions(width, height);
        0
    } else {
        set_last_error("Recorder not initialized");
        -1
    }
}

/// Set frame rate
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_set_fps(fps: u32) -> i32 {
    let mut recorder_guard = RECORDER.lock().unwrap();
    if let Some(ref mut recorder) = recorder_guard.as_mut() {
        recorder.set_fps(fps);
        0
    } else {
        set_last_error("Recorder not initialized");
        -1
    }
}

/// Start recording with auto-generated output path
/// root_dir: root directory path (e.g., "/home/user/data")
/// Returns 0 on success, -1 on error
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_start(root_dir: *const c_char) -> i32 {
    if root_dir.is_null() {
        set_last_error("Null root directory");
        return -1;
    }

    let root_dir_str = unsafe {
        CStr::from_ptr(root_dir).to_string_lossy().into_owned()
    };

    let mut recorder_guard = RECORDER.lock().unwrap();
    if let Some(ref mut recorder) = recorder_guard.as_mut() {
        let output_path = recorder.generate_output_path(&root_dir_str);

        match recorder.start(output_path) {
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
        set_last_error("Recorder not initialized");
        -1
    }
}

/// Stop recording
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_stop() -> i32 {
    let mut recorder_guard = RECORDER.lock().unwrap();
    if let Some(ref mut recorder) = recorder_guard.as_mut() {
        match recorder.stop() {
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
        set_last_error("Recorder not initialized");
        -1
    }
}

/// Check if recording is active
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_is_recording() -> i32 {
    let recorder_guard = RECORDER.lock().unwrap();
    if let Some(recorder) = recorder_guard.as_ref() {
        if recorder.is_recording() { 1 } else { 0 }
    } else {
        0
    }
}

/// Get elapsed recording time in seconds
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_elapsed_seconds() -> u64 {
    let recorder_guard = RECORDER.lock().unwrap();
    if let Some(recorder) = recorder_guard.as_ref() {
        recorder.elapsed_seconds()
    } else {
        0
    }
}

/// Get output file path (caller must NOT free the returned pointer)
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_get_output_path() -> *const c_char {
    let recorder_guard = RECORDER.lock().unwrap();
    if let Some(recorder) = recorder_guard.as_ref() {
        if let Some(path_str) = recorder.output_path().to_str() {
            if let Ok(c_string) = CString::new(path_str) {
                // Leak the string to keep it valid
                return c_string.into_raw();
            }
        }
    }
    std::ptr::null()
}

/// Get backend name
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_get_backend_name() -> *const c_char {
    let recorder_guard = RECORDER.lock().unwrap();
    if let Some(recorder) = recorder_guard.as_ref() {
        let name = recorder.backend().name();
        if let Ok(c_string) = CString::new(name) {
            return c_string.into_raw();
        }
    }
    std::ptr::null()
}

/// Get last error message
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_get_last_error() -> *const c_char {
    let error_guard = LAST_ERROR.lock().unwrap();
    if let Some(error) = error_guard.as_ref() {
        error.as_ptr()
    } else {
        std::ptr::null()
    }
}

/// Shutdown recorder
#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_shutdown() {
    let mut recorder_guard = RECORDER.lock().unwrap();
    if let Some(ref mut recorder) = recorder_guard.take() {
        let _ = recorder.stop();
    }
    log::info!("Screen recorder shutdown");
}
