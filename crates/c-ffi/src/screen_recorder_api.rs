//! 屏幕录制 C FFI 接口
//!
//! 提供基于 X11 的屏幕录制功能，使用 FFmpeg 进程进行编码

// Re-export video-recorder FFI functions
pub use video_recorder::ffi::*;