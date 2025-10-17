//! Screen recording module using external processes
//!
//! This module provides screen recording functionality by launching external
//! processes (wf-recorder for Wayland, ffmpeg for X11), avoiding complex
//! in-process encoding logic.

pub mod error;
pub mod recorder;
pub mod ffi;

pub use error::{RecorderError, Result};
pub use recorder::ScreenRecorder;
