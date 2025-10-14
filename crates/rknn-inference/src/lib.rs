pub mod ffi;
mod error;
mod types;

pub use error::{RknnError, Result};
pub use types::*;

use std::ffi::CString;
use std::path::Path;

/// YOLOv8 detection model context
pub struct Yolov8Detector {
    ctx: ffi::RknnAppContext,
}

/// Get COCO class name from class ID
pub fn get_class_name(class_id: i32) -> Option<String> {
    unsafe {
        let c_str = ffi::coco_cls_to_name(class_id);
        if c_str.is_null() {
            return None;
        }
        let c_str = std::ffi::CStr::from_ptr(c_str);
        c_str.to_str().ok().map(|s| s.to_string())
    }
}

impl Yolov8Detector {
    /// Create a new YOLOv8 detector from a model file
    pub fn new<P: AsRef<Path>>(model_path: P) -> Result<Self> {
        let path_str = model_path.as_ref().to_str()
            .ok_or(RknnError::InvalidPath)?;
        let c_path = CString::new(path_str)?;

        let mut ctx = ffi::RknnAppContext::default();

        unsafe {
            let ret = ffi::init_yolov8_model(c_path.as_ptr(), &mut ctx as *mut _);
            if ret < 0 {
                return Err(RknnError::InitFailed(ret));
            }
        }

        // Initialize post-processing
        unsafe {
            let ret = ffi::init_post_process();
            if ret < 0 {
                return Err(RknnError::PostProcessInitFailed(ret));
            }
        }

        Ok(Self { ctx })
    }

    /// Run inference on an image
    pub fn detect(&mut self, image: &ImageBuffer) -> Result<Vec<DetectionResult>> {
        let mut img_buf = ffi::FfiImageBuffer::from_image_buffer(image);
        let mut od_results = ffi::ObjectDetectResultList::default();

        unsafe {
            let ret = ffi::inference_yolov8_model(
                &mut self.ctx as *mut _,
                img_buf.as_ptr(),
                &mut od_results as *mut _,
            );

            if ret < 0 {
                return Err(RknnError::InferenceFailed(ret));
            }
        }

        Ok(od_results.to_detection_results())
    }

    /// Get model input dimensions
    pub fn model_size(&self) -> (u32, u32) {
        (self.ctx.model_width as u32, self.ctx.model_height as u32)
    }
}

impl Drop for Yolov8Detector {
    fn drop(&mut self) {
        unsafe {
            ffi::release_yolov8_model(&mut self.ctx as *mut _);
            ffi::deinit_post_process();
        }
    }
}

// Thread safety: RKNN context is not thread-safe by default
// Users should manage thread safety externally if needed
unsafe impl Send for Yolov8Detector {}
