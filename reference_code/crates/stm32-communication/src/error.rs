//! Error types for STM32 communication

use thiserror::Error;

/// STM32 communication error types
#[derive(Error, Debug, Clone)]
pub enum Stm32Error {
    #[error("HID device not found")]
    DeviceNotFound,
    
    #[error("HID device not connected")]
    DeviceNotConnected,
    
    #[error("Permission denied: {0}")]
    PermissionDenied(String),
    
    #[error("IO error: {0}")]
    IoError(String),
    
    #[error("HID API error: {0}")]
    HidApiError(String),
    
    #[error("Communication timeout")]
    Timeout,
    
    #[error("Invalid response data")]
    InvalidResponse,
    
    #[error("CRC check failed")]
    CrcError,
    
    #[error("Protocol error: {0}")]
    ProtocolError(String),
    
    #[error("Configuration error: {0}")]
    ConfigurationError(String),
    
    #[error("Device initialization failed: {0}")]
    InitializationError(String),
}

/// Result type for STM32 operations
pub type Stm32Result<T> = Result<T, Stm32Error>;
