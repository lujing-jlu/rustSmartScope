use thiserror::Error;

#[derive(Error, Debug)]
pub enum RecorderError {
    #[error("Backend not available: {0}")]
    BackendNotAvailable(String),

    #[error("Invalid configuration: {0}")]
    InvalidConfig(String),

    #[error("Recorder not started")]
    NotStarted,

    #[error("Recorder already running")]
    AlreadyRunning,

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("Thread join error")]
    ThreadJoin,
}

pub type Result<T> = std::result::Result<T, RecorderError>;
