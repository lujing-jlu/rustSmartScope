//! 视频录制 C FFI 接口
//!
//! 提供屏幕录制功能的 C 接口，使用 X11 捕获和 FFmpeg 编码

// Re-export video-recorder FFI functions
pub use video_recorder::ffi::*;
