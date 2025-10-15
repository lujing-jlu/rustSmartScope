use smartscope_core::{CameraMode, LogLevel, SmartScopeError};
use smartscope_core::camera::CameraProperty;

// =========================
// 错误码
// =========================

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorCode {
    Success = 0,
    Error = -1,
    ConfigError = -3,
    IoError = -5,
}

impl From<SmartScopeError> for ErrorCode {
    fn from(error: SmartScopeError) -> Self {
        match error {
            SmartScopeError::Config(_) => ErrorCode::ConfigError,
            SmartScopeError::Io(_) => ErrorCode::IoError,
            _ => ErrorCode::Error,
        }
    }
}

// =========================
// 日志
// =========================

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CLogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
}

impl From<CLogLevel> for LogLevel {
    fn from(level: CLogLevel) -> Self {
        match level {
            CLogLevel::Trace => LogLevel::Trace,
            CLogLevel::Debug => LogLevel::Debug,
            CLogLevel::Info => LogLevel::Info,
            CLogLevel::Warn => LogLevel::Warn,
            CLogLevel::Error => LogLevel::Error,
        }
    }
}

// =========================
// 相机通用类型
// =========================

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CCameraMode {
    NoCamera = 0,
    SingleCamera = 1,
    StereoCamera = 2,
}

impl From<CameraMode> for CCameraMode {
    fn from(mode: CameraMode) -> Self {
        match mode {
            CameraMode::NoCamera => CCameraMode::NoCamera,
            CameraMode::SingleCamera => CCameraMode::SingleCamera,
            CameraMode::StereoCamera => CCameraMode::StereoCamera,
        }
    }
}

#[repr(C)]
pub struct CCameraFrame {
    pub data: *const u8,
    pub data_len: usize,
    pub width: u32,
    pub height: u32,
    pub format: u32,
    pub timestamp_sec: u64,
    pub timestamp_nsec: u32,
}

#[repr(C)]
pub struct CCameraStatus {
    pub running: bool,
    pub mode: u32, // CCameraMode as u32
    pub left_camera_connected: bool,
    pub right_camera_connected: bool,
    pub last_left_frame_sec: u64,
    pub last_right_frame_sec: u64,
}

// =========================
// AI 检测
// =========================

#[repr(C)]
pub struct CDetection {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    pub confidence: f32,
    pub class_id: i32,
}

// =========================
// 相机参数
// =========================

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CCameraProperty {
    Brightness = 0,
    Contrast = 1,
    Saturation = 2,
    Gain = 3,
    ExposureTime = 4,
    WhiteBalance = 5,
    Gamma = 6,
    BacklightCompensation = 7,
    AutoExposure = 8,
    AutoWhiteBalance = 9,
}

impl From<CCameraProperty> for CameraProperty {
    fn from(prop: CCameraProperty) -> Self {
        match prop {
            CCameraProperty::Brightness => CameraProperty::Brightness,
            CCameraProperty::Contrast => CameraProperty::Contrast,
            CCameraProperty::Saturation => CameraProperty::Saturation,
            CCameraProperty::Gain => CameraProperty::Gain,
            CCameraProperty::ExposureTime => CameraProperty::Exposure,
            CCameraProperty::WhiteBalance => CameraProperty::WhiteBalance,
            CCameraProperty::Gamma => CameraProperty::Gamma,
            CCameraProperty::BacklightCompensation => CameraProperty::BacklightCompensation,
            CCameraProperty::AutoExposure => CameraProperty::AutoExposure,
            CCameraProperty::AutoWhiteBalance => CameraProperty::AutoWhiteBalance,
        }
    }
}

#[repr(C)]
pub struct CCameraParameterRange {
    pub min: i32,
    pub max: i32,
    pub step: i32,
    pub default_value: i32,
    pub current: i32,
}

