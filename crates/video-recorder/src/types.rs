use std::sync::Arc;

/// Video pixel format
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VideoPixelFormat {
    RGB888,
    BGR888,
    RGBA8888,
    BGRA8888,
    YUV420P,
    NV12,
}

/// Video codec type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VideoCodec {
    H264,
    H265,
    VP8,
    VP9,
}

/// Hardware acceleration type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum HardwareAccelType {
    None,
    RkMpp,  // Rockchip MPP
    VaApi,  // VA-API (Intel/AMD)
}

/// Video frame with metadata
#[derive(Clone)]
pub struct VideoFrame {
    pub data: Arc<Vec<u8>>,
    pub width: u32,
    pub height: u32,
    pub format: VideoPixelFormat,
    pub timestamp_us: i64,
    pub frame_number: u64,
}

impl VideoFrame {
    pub fn new(data: Vec<u8>, width: u32, height: u32, format: VideoPixelFormat) -> Self {
        Self {
            data: Arc::new(data),
            width,
            height,
            format,
            timestamp_us: chrono::Utc::now().timestamp_micros(),
            frame_number: 0,
        }
    }
}

/// Recording configuration
#[derive(Debug, Clone)]
pub struct RecorderConfig {
    pub width: u32,
    pub height: u32,
    pub fps: u32,
    pub bitrate: u64,
    pub codec: VideoCodec,
    pub hardware_accel: HardwareAccelType,
    pub max_queue_size: usize,
    pub output_path: String,
}

impl Default for RecorderConfig {
    fn default() -> Self {
        Self {
            width: 1920,
            height: 1080,
            fps: 30,
            bitrate: 4_000_000,
            codec: VideoCodec::H264,
            hardware_accel: HardwareAccelType::RkMpp,
            max_queue_size: 60,
            output_path: "recording.mp4".to_string(),
        }
    }
}

/// Recording statistics
#[derive(Debug, Default, Clone)]
pub struct RecorderStats {
    pub frames_encoded: u64,
    pub frames_dropped: u64,
    pub bytes_written: u64,
    pub duration_ms: u64,
    pub is_recording: bool,
}
