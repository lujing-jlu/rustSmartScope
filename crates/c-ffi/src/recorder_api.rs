//! 视频录制 C FFI 接口
//!
//! 提供屏幕录制功能的 C 接口，使用外部进程 (wf-recorder/ffmpeg) 录制

use std::os::raw::c_char;

// Re-export video_recorder FFI functions to ensure symbols are exported from root crate

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_init() -> i32 {
    video_recorder::ffi::smartscope_screen_recorder_init()
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_set_dimensions(width: u32, height: u32) -> i32 {
    video_recorder::ffi::smartscope_screen_recorder_set_dimensions(width, height)
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_set_fps(fps: u32) -> i32 {
    video_recorder::ffi::smartscope_screen_recorder_set_fps(fps)
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_start(root_dir: *const c_char) -> i32 {
    video_recorder::ffi::smartscope_screen_recorder_start(root_dir)
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_stop() -> i32 {
    video_recorder::ffi::smartscope_screen_recorder_stop()
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_is_recording() -> i32 {
    video_recorder::ffi::smartscope_screen_recorder_is_recording()
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_elapsed_seconds() -> u64 {
    video_recorder::ffi::smartscope_screen_recorder_elapsed_seconds()
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_get_output_path() -> *const c_char {
    video_recorder::ffi::smartscope_screen_recorder_get_output_path()
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_get_backend_name() -> *const c_char {
    video_recorder::ffi::smartscope_screen_recorder_get_backend_name()
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_get_last_error() -> *const c_char {
    video_recorder::ffi::smartscope_screen_recorder_get_last_error()
}

#[no_mangle]
pub extern "C" fn smartscope_screen_recorder_shutdown() {
    video_recorder::ffi::smartscope_screen_recorder_shutdown()
}
