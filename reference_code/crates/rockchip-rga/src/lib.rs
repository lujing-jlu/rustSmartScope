#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

mod bindings;
mod error;
mod buffer;
mod context;
mod api;

pub use error::{RgaError, RgaResult};
pub use buffer::{RgaBuffer, RgaRect, RGA_FORMAT_RGBA_8888, RGA_FORMAT_RGBA_5551, RGA_FORMAT_RGBA_4444, RGA_FORMAT_RGB_888, RgaFormat};
pub use context::RgaContext;
pub use api::{RgaProcessor, RgaImage, RgaTransform};

// 重新导出底层绑定
pub use bindings::*;

// 版本信息
pub const RGA_VERSION: &str = env!("CARGO_PKG_VERSION");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_rga_version() {
        println!("RGA Rust binding version: {}", RGA_VERSION);
        assert!(!RGA_VERSION.is_empty());
    }
}
