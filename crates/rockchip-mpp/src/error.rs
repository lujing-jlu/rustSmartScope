use thiserror::Error;

#[derive(Error, Debug)]
pub enum MppError {
    #[error("MPP operation failed with code: {0}")]
    MppError(i32),
    
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

impl From<i32> for MppError {
    fn from(code: i32) -> Self {
        if code == 0 {
            return MppError::Unknown("Unexpected success code".to_string());
        }
        MppError::MppError(code)
    }
}

impl From<MppError> for i32 {
    fn from(err: MppError) -> Self {
        match err {
            MppError::MppError(code) => code,
            MppError::InvalidParameter(_) => -1,
            MppError::NotSupported => -2,
            MppError::ResourceBusy => -3,
            MppError::OutOfMemory => -4,
            MppError::Timeout => -5,
            MppError::IoError(_) => -6,
            MppError::Unknown(_) => -99,
        }
    }
}

pub type MppResult<T> = Result<T, MppError>;
