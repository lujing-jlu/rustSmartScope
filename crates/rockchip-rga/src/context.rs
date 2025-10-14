use crate::bindings::*;
use crate::buffer::{RgaBuffer, RgaRect};
use crate::error::{RgaError, RgaResult};

#[derive(Debug)]
pub struct RgaContext {
    // RGA上下文，用于管理RGA操作
}

impl RgaContext {
    pub fn new() -> RgaResult<Self> {
        Ok(RgaContext {})
    }

    /// 执行简单的RGA操作
    pub fn process_simple(
        &self,
        src: &RgaBuffer,
        dst: &RgaBuffer,
        src_rect: &RgaRect,
        dst_rect: &RgaRect,
        mode_usage: u32,
    ) -> RgaResult<()> {
        unsafe {
            let ret = improcess(
                src.as_ptr().clone(),
                dst.as_ptr().clone(),
                std::mem::zeroed(), // pat
                dst_rect.to_im_rect(),
                src_rect.to_im_rect(),
                std::mem::zeroed(), // pat_rect
                mode_usage as i32,
            );
            if ret != 0 {
                return Err(RgaError::RgaError(ret));
            }
            Ok(())
        }
    }

    /// 查询RGA信息
    pub fn query_info(&self, name: i32) -> RgaResult<String> {
        unsafe {
            let info = querystring(name);
            if info.is_null() {
                return Err(RgaError::Unknown("Failed to query RGA info".to_string()));
            }

            let c_str = std::ffi::CStr::from_ptr(info);
            let info_str = c_str.to_string_lossy().into_owned();
            Ok(info_str)
        }
    }

    /// 检查RGA操作参数
    pub fn check_operation(
        &self,
        src: &RgaBuffer,
        dst: &RgaBuffer,
        src_rect: &RgaRect,
        dst_rect: &RgaRect,
    ) -> RgaResult<()> {
        unsafe {
            let ret = imcheck_t(
                src.as_ptr().clone(),
                dst.as_ptr().clone(),
                std::mem::zeroed(), // pat
                src_rect.to_im_rect(),
                dst_rect.to_im_rect(),
                std::mem::zeroed(), // pat_rect
                0,                  // mode_usage
            );
            if ret != 0 {
                return Err(RgaError::RgaError(ret));
            }
            Ok(())
        }
    }

    /// 获取RGA版本信息
    pub fn get_version(&self) -> RgaResult<String> {
        Ok(format!(
            "{}.{}.{}.{}",
            RGA_API_MAJOR_VERSION,
            RGA_API_MINOR_VERSION,
            RGA_API_REVISION_VERSION,
            RGA_API_BUILD_VERSION
        ))
    }

    /// 获取RGA供应商信息
    pub fn get_vendor(&self) -> RgaResult<String> {
        Ok("Rockchip".to_string())
    }

    /// 获取支持的最大输入分辨率
    pub fn get_max_input(&self) -> RgaResult<String> {
        Ok("8192x8192".to_string())
    }

    /// 获取支持的最大输出分辨率
    pub fn get_max_output(&self) -> RgaResult<String> {
        Ok("8192x8192".to_string())
    }

    /// 获取支持的输入格式
    pub fn get_input_formats(&self) -> RgaResult<String> {
        Ok("RGBA_8888, RGBA_5551, RGBA_4444, etc.".to_string())
    }

    /// 获取支持的输出格式
    pub fn get_output_formats(&self) -> RgaResult<String> {
        Ok("RGBA_8888, RGBA_5551, RGBA_4444, etc.".to_string())
    }
}

impl Default for RgaContext {
    fn default() -> Self {
        Self::new().unwrap_or_else(|_| RgaContext {})
    }
}
