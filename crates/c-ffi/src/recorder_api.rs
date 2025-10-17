//! 视频录制 C FFI 接口
//!
//! 提供屏幕录制功能的 C 接口，使用 X11 捕获和 FFmpeg 编码

use std::os::raw::{c_char, c_int, c_uchar, c_ulonglong, c_uint, c_ulong};

// 直接包装 video_recorder::ffi 中的实现，以确保符号从根 crate 导出

#[no_mangle]
pub extern "C" fn smartscope_recorder_init_simple(
    output_path: *const c_char,
    width: c_uint,
    height: c_uint,
    fps: c_uint,
    bitrate: c_ulonglong,
) -> c_int {
    video_recorder::ffi::smartscope_recorder_init_simple(output_path, width, height, fps, bitrate)
}

#[no_mangle]
pub extern "C" fn smartscope_recorder_submit_rgb888(
    data: *const c_uchar,
    data_len: usize,
    width: c_uint,
    height: c_uint,
    timestamp_us: i64,
) -> c_int {
    video_recorder::ffi::smartscope_recorder_submit_rgb888(data, data_len, width, height, timestamp_us)
}

#[no_mangle]
pub extern "C" fn smartscope_screenrec_start(
    output_path: *const c_char,
    fps: c_uint,
    bitrate: c_ulonglong,
) -> c_int {
    video_recorder::ffi::smartscope_screenrec_start(output_path, fps, bitrate)
}

#[no_mangle]
pub extern "C" fn smartscope_screenrec_stop() -> c_int {
    video_recorder::ffi::smartscope_screenrec_stop()
}

// 继续透出已存在的录制通用 API（如 start/stop/is_recording 等）
#[no_mangle]
pub extern "C" fn smartscope_recorder_start() -> c_int {
    video_recorder::ffi::smartscope_recorder_start()
}

#[no_mangle]
pub extern "C" fn smartscope_recorder_stop() -> c_int {
    video_recorder::ffi::smartscope_recorder_stop()
}

#[no_mangle]
pub extern "C" fn smartscope_recorder_is_recording() -> c_int {
    video_recorder::ffi::smartscope_recorder_is_recording()
}

#[no_mangle]
pub extern "C" fn smartscope_recorder_get_queue_size() -> c_int {
    video_recorder::ffi::smartscope_recorder_get_queue_size()
}

#[no_mangle]
pub extern "C" fn smartscope_recorder_get_last_error() -> *const c_char {
    video_recorder::ffi::smartscope_recorder_get_last_error()
}
