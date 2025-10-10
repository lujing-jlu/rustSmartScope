//! Error handling for camera correction operations

use std::io;
use thiserror::Error;

#[cfg(feature = "opencv")]
use opencv;

/// Main error type for camera correction operations
#[derive(Error, Debug)]
pub enum CorrectionError {
    /// I/O error (file reading, etc.)
    #[error("I/O error: {0}")]
    IoError(#[from] io::Error),

    /// Parameter file parsing failed
    #[error("Parameter file parsing error: {0}")]
    ParameterParseError(String),

    /// Camera parameters not loaded
    #[error("Camera parameters not loaded: {0}")]
    ParametersNotLoaded(String),

    /// Invalid image size
    #[error("Invalid image size: {width}x{height}")]
    InvalidImageSize { width: i32, height: i32 },

    /// Invalid camera matrix dimensions
    #[error("Invalid camera matrix dimensions: {rows}x{cols}, expected 3x3")]
    InvalidCameraMatrix { rows: i32, cols: i32 },

    /// Invalid distortion coefficients
    #[error("Invalid distortion coefficients: {count} coefficients, expected 5")]
    InvalidDistortionCoeffs { count: usize },

    /// Invalid rotation matrix dimensions
    #[error("Invalid rotation matrix dimensions: {rows}x{cols}, expected 3x3")]
    InvalidRotationMatrix { rows: i32, cols: i32 },

    /// Invalid translation vector dimensions
    #[error("Invalid translation vector dimensions: {rows}x{cols}, expected 3x1")]
    InvalidTranslationVector { rows: i32, cols: i32 },

    /// Remapping maps not generated
    #[error("Remapping maps not generated: {0}")]
    RemapMapsNotGenerated(String),

    /// Rectification not initialized
    #[error("Rectification not initialized: {0}")]
    RectificationNotInitialized(String),

    /// Image correction failed
    #[error("Image correction failed: {0}")]
    ImageCorrectionError(String),

    /// Generic error with message
    #[error("Camera correction error: {0}")]
    Generic(String),
}

impl From<String> for CorrectionError {
    fn from(err: String) -> Self {
        CorrectionError::Generic(err)
    }
}

#[cfg(feature = "opencv")]
impl From<opencv::Error> for CorrectionError {
    fn from(err: opencv::Error) -> Self {
        CorrectionError::ImageCorrectionError(err.to_string())
    }
}

impl From<&str> for CorrectionError {
    fn from(err: &str) -> Self {
        CorrectionError::Generic(err.to_string())
    }
}

// Map stereo-rectifier error into our error type
#[cfg(feature = "opencv")]
impl From<stereo_rectifier::RectifyError> for CorrectionError {
    fn from(err: stereo_rectifier::RectifyError) -> Self {
        CorrectionError::ImageCorrectionError(err.to_string())
    }
}

/// Result type for camera correction operations
pub type CorrectionResult<T> = std::result::Result<T, CorrectionError>;

/// Helper trait for converting Option to CorrectionResult
pub trait OptionExt<T> {
    /// Convert Option to Result with a custom error message
    fn ok_or_correction_error<F>(self, f: F) -> CorrectionResult<T>
    where
        F: FnOnce() -> String;
}

impl<T> OptionExt<T> for Option<T> {
    fn ok_or_correction_error<F>(self, f: F) -> CorrectionResult<T>
    where
        F: FnOnce() -> String,
    {
        self.ok_or_else(|| CorrectionError::Generic(f()))
    }
}

/// Helper trait for converting bool to CorrectionResult
pub trait BoolExt {
    /// Convert bool to Result with a custom error message
    fn then_some_or_correction_error<T, F>(self, value: T, f: F) -> CorrectionResult<T>
    where
        F: FnOnce() -> String;
}

impl BoolExt for bool {
    fn then_some_or_correction_error<T, F>(self, value: T, f: F) -> CorrectionResult<T>
    where
        F: FnOnce() -> String,
    {
        if self {
            Ok(value)
        } else {
            Err(CorrectionError::Generic(f()))
        }
    }
}
