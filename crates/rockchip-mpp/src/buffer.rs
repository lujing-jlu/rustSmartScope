use crate::bindings::*;
use crate::error::{MppError, MppResult};
use std::ptr;

#[derive(Debug)]
pub struct MppBuffer {
    buffer: crate::bindings::MppBuffer, // This is *mut c_void
}

impl MppBuffer {
    pub fn from_ptr(buffer: crate::bindings::MppBuffer) -> Self {
        MppBuffer { buffer }
    }

    pub fn as_ptr(&self) -> crate::bindings::MppBuffer {
        self.buffer
    }

    pub fn new(group: crate::bindings::MppBufferGroup, size: usize) -> MppResult<Self> {
        let mut buffer = ptr::null_mut();
        unsafe {
            let tag = std::ffi::CString::new("rust_buffer").unwrap();
            let caller = std::ffi::CString::new("rust_buffer").unwrap();
            let ret =
                mpp_buffer_get_with_tag(group, &mut buffer, size, tag.as_ptr(), caller.as_ptr());
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(MppBuffer { buffer })
        }
    }

    pub fn get_size(&self) -> usize {
        unsafe {
            let caller = std::ffi::CString::new("rust_buffer").unwrap();
            mpp_buffer_get_size_with_caller(self.buffer, caller.as_ptr())
        }
    }

    pub fn get_data(&self) -> &[u8] {
        unsafe {
            let caller = std::ffi::CString::new("rust_buffer").unwrap();
            let data = mpp_buffer_get_ptr_with_caller(self.buffer, caller.as_ptr());
            let size = mpp_buffer_get_size_with_caller(self.buffer, caller.as_ptr());

            if data.is_null() || size == 0 {
                return &[];
            }

            std::slice::from_raw_parts(data as *const u8, size)
        }
    }

    pub fn get_data_mut(&mut self) -> &mut [u8] {
        unsafe {
            let caller = std::ffi::CString::new("rust_buffer").unwrap();
            let data = mpp_buffer_get_ptr_with_caller(self.buffer, caller.as_ptr());
            let size = mpp_buffer_get_size_with_caller(self.buffer, caller.as_ptr());

            if data.is_null() || size == 0 {
                return &mut [];
            }

            std::slice::from_raw_parts_mut(data as *mut u8, size)
        }
    }

    pub fn copy_from_slice(&mut self, data: &[u8]) -> MppResult<()> {
        let buffer_size = self.get_size();
        if data.len() > buffer_size {
            return Err(MppError::InvalidParameter(format!(
                "Data size {} exceeds buffer size {}",
                data.len(),
                buffer_size
            )));
        }

        let buffer_data = self.get_data_mut();
        buffer_data[..data.len()].copy_from_slice(data);
        Ok(())
    }

    pub fn copy_to_slice(&self, data: &mut [u8]) -> MppResult<()> {
        let buffer_size = self.get_size();
        if data.len() < buffer_size {
            return Err(MppError::InvalidParameter(format!(
                "Target slice size {} is smaller than buffer size {}",
                data.len(),
                buffer_size
            )));
        }

        let buffer_data = self.get_data();
        data[..buffer_size].copy_from_slice(buffer_data);
        Ok(())
    }
}

impl Drop for MppBuffer {
    fn drop(&mut self) {
        unsafe {
            let caller = std::ffi::CString::new("rust_buffer").unwrap();
            let _ = mpp_buffer_put_with_caller(self.buffer, caller.as_ptr());
        }
    }
}
