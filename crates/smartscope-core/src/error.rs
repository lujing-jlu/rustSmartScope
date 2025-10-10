//! 基础错误处理

use thiserror::Error;

/// SmartScope错误类型
#[derive(Error, Debug)]
pub enum SmartScopeError {
    #[error("配置错误: {0}")]
    Config(String),

    #[error("IO错误: {0}")]
    Io(#[from] std::io::Error),

    #[error("相机错误: {0}")]
    CameraError(String),

    #[error("视频处理错误: {0}")]
    VideoProcessingError(String),

    #[error("未知错误: {0}")]
    Unknown(String),
}

/// C FFI错误码
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorCode {
    /// 成功
    Success = 0,
    /// 一般错误
    Error = -1,
    /// 配置错误
    ConfigError = -3,
    /// IO错误
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

/// SmartScope结果类型
pub type Result<T> = std::result::Result<T, SmartScopeError>;