//! USB Camera management library for SmartScope
//!
//! This library provides high-level abstractions for managing USB cameras,
//! including device discovery, configuration, and image capture.

pub mod config;
pub mod control;
// pub mod cpp_interface;  // 禁用：在smartscope-c-ffi中有专用的FFI层
pub mod decoder;
pub mod device;
pub mod error;
pub mod monitor;
pub mod stream;
mod turbojpeg;

pub use config::*;
pub use control::*;
// pub use cpp_interface::*;  // 禁用
pub use decoder::*;
pub use device::*;
pub use error::*;
pub use monitor::*;
pub use stream::*;
