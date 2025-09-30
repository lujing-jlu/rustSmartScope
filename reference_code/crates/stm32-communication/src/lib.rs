//! STM32 HID Communication Library for SmartScope
//! 
//! This library provides HID communication with STM32 devices for:
//! - LED light control
//! - Battery level monitoring
//! - Temperature sensor reading
//! - Device status management

pub mod hid;
pub mod device;
pub mod protocol;
pub mod error;
pub mod controller;

pub use hid::*;
pub use device::*;
pub use protocol::*;
pub use error::*;
pub use controller::*;
