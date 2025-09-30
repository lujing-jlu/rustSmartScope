//! SmartScope错误处理

use thiserror::Error;

/// SmartScope错误类型
#[derive(Error, Debug)]
pub enum SmartScopeError {
    #[error("Instance already initialized: {0}")]
    AlreadyInitialized(String),

    #[error("Instance not initialized: {0}")]
    NotInitialized(String),

    #[error("Instance already started: {0}")]
    AlreadyStarted(String),

    #[error("Instance not started: {0}")]
    NotStarted(String),

    #[error("Camera error: {0}")]
    CameraError(String),

    #[error("USB camera error: {0}")]
    UsbCameraError(String),

    #[error("Configuration error: {0}")]
    ConfigError(String),

    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),

    #[error("Serialization error: {0}")]
    SerdeError(#[from] serde_json::Error),

    #[error("Invalid parameter: {0}")]
    InvalidParameter(String),

    #[error("Frame not available: {0}")]
    FrameNotAvailable(String),

    #[error("Frame capture failed: {0}")]
    FrameCaptureFailed(String),

    #[error("Camera not found: {0}")]
    CameraNotFound(String),

    #[error("Camera start failed: {0}")]
    CameraStartFailed(String),

    #[error("Device not found: {0}")]
    DeviceNotFound(String),

    #[error("Operation timeout")]
    Timeout,

    #[error("Internal error: {0}")]
    InternalError(String),
}

// 从USB相机错误转换
impl From<usb_camera::CameraError> for SmartScopeError {
    fn from(err: usb_camera::CameraError) -> Self {
        SmartScopeError::UsbCameraError(err.to_string())
    }
}

/// SmartScope结果类型
pub type SmartScopeResult<T> = Result<T, SmartScopeError>;

