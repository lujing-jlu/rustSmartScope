use thiserror::Error;

#[derive(Error, Debug)]
pub enum RgaError {
    #[error("RGA operation failed with code: {0}")]
    RgaError(i32),
    
    #[error("Invalid parameter: {0}")]
    InvalidParameter(String),
    
    #[error("Operation not supported")]
    NotSupported,
    
    #[error("Resource busy")]
    ResourceBusy,
    
    #[error("Out of memory")]
    OutOfMemory,
    
    #[error("Timeout")]
    Timeout,
    
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
    
    #[error("Unknown error: {0}")]
    Unknown(String),
}

impl From<i32> for RgaError {
    fn from(code: i32) -> Self {
        if code == 0 {
            return RgaError::Unknown("Unexpected success code".to_string());
        }
        RgaError::RgaError(code)
    }
}

impl From<RgaError> for i32 {
    fn from(err: RgaError) -> Self {
        match err {
            RgaError::RgaError(code) => code,
            RgaError::InvalidParameter(_) => -1,
            RgaError::NotSupported => -2,
            RgaError::ResourceBusy => -3,
            RgaError::OutOfMemory => -4,
            RgaError::Timeout => -5,
            RgaError::IoError(_) => -6,
            RgaError::Unknown(_) => -99,
        }
    }
}

pub type RgaResult<T> = Result<T, RgaError>;

