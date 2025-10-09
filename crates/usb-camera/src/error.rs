//! Error handling module
//! 
//! This module provides error types and handling for the USB camera library.

use thiserror::Error;

/// USB Camera library error types
#[derive(Error, Debug)]
pub enum CameraError {
    #[error("Device not found: {0}")]
    DeviceNotFound(String),
    
    #[error("Permission denied: {0}")]
    PermissionDenied(String),
    
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
    
    #[error("V4L2 error: {0}")]
    V4L2Error(String),
    
    #[error("Invalid device path: {0}")]
    InvalidDevicePath(String),
    
    #[error("Device enumeration failed: {0}")]
    DeviceEnumerationFailed(String),
    
    #[error("Unsupported device type: {0}")]
    UnsupportedDeviceType(String),
    
    #[error("Configuration error: {0}")]
    ConfigurationError(String),
    
    #[error("Device operation failed: {0}")]
    DeviceOperationFailed(String),
}

/// Result type for USB camera operations
pub type CameraResult<T> = Result<T, CameraError>;
