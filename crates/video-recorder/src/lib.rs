//! Video recording module with X11 screen capture support
//!
//! Provides non-blocking video recording functionality using FFmpeg
//! and X11 screen capture on Linux systems.

pub mod error;
pub mod types;
pub mod capture;
pub mod ffmpeg_wrapper;
pub mod mpp_encoder;
pub mod service;
pub mod ffi;

pub use error::{RecorderError, Result};
pub use types::{VideoFrame, RecorderConfig, RecorderStats, VideoPixelFormat, VideoCodec, HardwareAccelType};
pub use capture::X11ScreenCapturer;
pub use ffmpeg_wrapper::FFmpegRecorder;
// Backward-compat alias so modules importing `crate::VideoEncoder` keep working
pub use ffmpeg_wrapper::FFmpegRecorder as VideoEncoder;
pub use mpp_encoder::MppEncoder;
pub use service::RecordingService;
