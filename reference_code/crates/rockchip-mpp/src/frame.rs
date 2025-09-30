use crate::bindings::*;

#[derive(Debug)]
pub struct MppFrame {
    frame: crate::bindings::MppFrame, // This is *mut c_void
}

impl MppFrame {
    pub fn from_ptr(frame: crate::bindings::MppFrame) -> Self {
        MppFrame { frame }
    }

    pub fn as_ptr(&self) -> crate::bindings::MppFrame {
        self.frame
    }

    pub fn get_width(&self) -> u32 {
        unsafe { mpp_frame_get_width(self.frame) }
    }

    pub fn get_height(&self) -> u32 {
        unsafe { mpp_frame_get_height(self.frame) }
    }

    pub fn get_format(&self) -> MppFrameFormat {
        unsafe { mpp_frame_get_fmt(self.frame) }
    }

    pub fn get_buffer(&self) -> Option<crate::buffer::MppBuffer> {
        unsafe {
            let buffer = mpp_frame_get_buffer(self.frame);
            if buffer.is_null() {
                None
            } else {
                Some(crate::buffer::MppBuffer::from_ptr(buffer))
            }
        }
    }

    pub fn get_timestamp(&self) -> i64 {
        unsafe { mpp_frame_get_pts(self.frame) }
    }

    pub fn set_timestamp(&mut self, timestamp: i64) {
        unsafe {
            mpp_frame_set_pts(self.frame, timestamp);
        }
    }
}

impl Drop for MppFrame {
    fn drop(&mut self) {
        unsafe {
            let _ = mpp_frame_deinit(&mut self.frame as *mut _);
        }
    }
}

// 重新导出类型
pub use crate::bindings::MppFrameFormat;
