#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

mod bindings;
mod buffer;
mod context;
mod error;
mod frame;
mod packet;

pub use buffer::MppBuffer;
pub use context::MppContext;
pub use error::{MppError, MppResult};
pub use frame::MppFrame;
pub use packet::MppPacket;

// 重新导出底层绑定
pub use bindings::*;

// 版本信息
pub const MPP_VERSION: &str = env!("CARGO_PKG_VERSION");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mpp_version() {
        println!("MPP Rust binding version: {}", MPP_VERSION);
        assert!(!MPP_VERSION.is_empty());
    }
}
