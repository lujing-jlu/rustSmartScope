//! USB Camera management library for SmartScope
//!
//! This library provides high-level abstractions for managing USB cameras,
//! including device discovery, configuration, and image capture.

pub mod config;
pub mod control;
pub mod cpp_interface;
pub mod decoder;
pub mod device;
pub mod error;
pub mod monitor;
pub mod stream;
mod turbojpeg;

pub use config::*;
pub use control::*;
pub use cpp_interface::*;
pub use decoder::*;
pub use device::*;
pub use error::*;
pub use monitor::*;
pub use stream::*;
