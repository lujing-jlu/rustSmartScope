use thiserror::Error;

#[derive(Error, Debug, Clone)]
pub enum RknnError {
    #[error("Invalid path")]
    InvalidPath,

    #[error("Failed to initialize model: {0}")]
    InitFailed(i32),

    #[error("Failed to initialize post-process: {0}")]
    PostProcessInitFailed(i32),

    #[error("Inference failed: {0}")]
    InferenceFailed(i32),

    #[error("Invalid image format")]
    InvalidImageFormat,

    #[error("Invalid input: {0}")]
    InvalidInput(String),

    #[error("C string conversion error: {0}")]
    CStringError(#[from] std::ffi::NulError),
}

pub type Result<T> = std::result::Result<T, RknnError>;
