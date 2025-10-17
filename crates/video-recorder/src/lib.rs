//! Video recording module with X11 screen capture support
//!
//! Provides non-blocking video recording functionality using FFmpeg
//! and X11 screen capture on Linux systems.

pub mod error;
pub mod types;
pub mod capture;
pub mod ffmpeg_wrapper;
pub mod encoder_vfr_t3;
pub mod mpp_encoder;
pub mod service;
pub mod ffi;

pub use error::{RecorderError, Result};
pub use types::{VideoFrame, RecorderConfig, RecorderStats, VideoPixelFormat, VideoCodec, HardwareAccelType};
pub use capture::X11ScreenCapturer;
pub use encoder_vfr_t3::FfmpegVfrEncoderT3;
// Default VideoEncoder is in-process VFR encoder based on ffmpeg-the-third
pub type VideoEncoder = encoder_vfr_t3::FfmpegVfrEncoderT3;
pub use mpp_encoder::MppEncoder;
pub use service::RecordingService;
