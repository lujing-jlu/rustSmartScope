//! RKNN推理库
//!
//! 提供YOLOv8模型的RKNN推理功能，支持Rockchip NPU硬件加速。
//!
//! # 特性
//!
//! - 高性能RKNN NPU推理（实测可达37+ FPS）
//! - 内存安全的Rust接口
//! - 完整的预处理和后处理支持
//! - 多线程推理服务支持
//! - 零拷贝内存管理
//!
//! # 快速开始
//!
//! ## 单线程推理
//!
//! ```rust
//! use rknn_inference::{Yolov8Detector, ImageBuffer};
//!
//! // 创建检测器
//! let mut detector = Yolov8Detector::new("model.rknn")?;
//!
//! // 加载图片并预处理
//! let img = image::open("test.jpg")?.to_rgb8();
//! let image_buffer = preprocess_image(&img);
//!
//! // 推理
//! let results = detector.detect(&image_buffer)?;
//!
//! println!("检测到 {} 个对象", results.len());
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ## 高性能异步推理（推荐）
//!
//! ```rust
//! use rknn_inference::{MultiDetectorInferenceService, ImagePreprocessor};
//!
//! // 创建6个detector的异步推理服务，队列长度限制为6
//! let service = MultiDetectorInferenceService::new("model.rknn", 6)?;
//! let preprocessor = ImagePreprocessor::new();
//!
//! // 异步提交多个推理任务
//! let mut task_ids = Vec::new();
//! for _ in 0..5 {
//!     let img = image::open("test.jpg")?.to_rgb8();
//!     let image_buffer = preprocessor.preprocess(&img);
//!     let task_id = service.submit_inference(&image_buffer)?;
//!     task_ids.push(task_id);
//! }
//!
//! // 异步获取结果
//! while let Some((task_id, results)) = service.try_get_result() {
//!     match results {
//!         Ok(detections) => println!("任务 {} 检测到 {} 个对象", task_id, detections.len()),
//!         Err(e) => println!("任务 {} 推理失败: {:?}", task_id, e),
//!     }
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ## 同步推理（向后兼容）
//!
//! ```rust
//! use rknn_inference::MultiDetectorInferenceService;
//!
//! // 创建6个detector的推理服务
//! let service = MultiDetectorInferenceService::new("model.rknn", 6)?;
//!
//! // 加载图片
//! let img = image::open("test.jpg")?.to_rgb8();
//!
//! // 同步推理（内部使用异步机制）
//! let results = service.inference(&img)?;
//!
//! println!("检测到 {} 个对象", results.len());
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! # 性能建议
//!
//! - 使用异步推理接口获得最佳性能（输入和输出都是队列，不会阻塞）
//! - 使用`MultiDetectorInferenceService`获得最佳性能（推荐6个工作线程）
//! - 队列长度限制为6，防止内存积压，适合实时应用
//! - 对于实时应用，推荐使用6个工作线程以获得最佳并发效率
//! - 预处理在主线程进行，避免RGA多线程冲突
//! - 可以持续输入图像，按需获取检测结果，最大化NPU利用率

pub mod ffi;
mod error;
mod types;
pub mod service;
pub mod simple_service;
pub mod multi_detector_service;

pub use error::{RknnError, Result};
pub use types::*;
pub use service::{InferenceService};
pub use simple_service::{SimpleInferenceService};
pub use multi_detector_service::{MultiDetectorInferenceService, ImagePreprocessor};
pub use multi_detector_service::ServiceStats;

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
