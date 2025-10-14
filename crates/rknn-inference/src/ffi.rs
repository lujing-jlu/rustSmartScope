use crate::types::{DetectionResult, ImageRect};

// RKNN API types (opaque pointer)
pub type RknnContext = usize;

// Maximum number of detected objects
const OBJ_NUMB_MAX_SIZE: usize = 128;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct RknnInputOutputNum {
    pub n_input: u32,
    pub n_output: u32,
}

impl Default for RknnInputOutputNum {
    fn default() -> Self {
        Self {
            n_input: 0,
            n_output: 0,
        }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct RknnTensorAttr {
    _opaque: [u8; 256], // Placeholder for RKNN tensor attributes
}

impl Default for RknnTensorAttr {
    fn default() -> Self {
        Self { _opaque: [0; 256] }
    }
}

#[repr(C)]
pub struct RknnAppContext {
    pub rknn_ctx: RknnContext,
    pub io_num: RknnInputOutputNum,
    pub input_attrs: *mut RknnTensorAttr,
    pub output_attrs: *mut RknnTensorAttr,
    pub model_channel: i32,
    pub model_width: i32,
    pub model_height: i32,
    pub is_quant: bool,
}

impl Default for RknnAppContext {
    fn default() -> Self {
        Self {
            rknn_ctx: 0,
            io_num: RknnInputOutputNum::default(),
            input_attrs: std::ptr::null_mut(),
            output_attrs: std::ptr::null_mut(),
            model_channel: 0,
            model_width: 0,
            model_height: 0,
            is_quant: false,
        }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct CImageRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl From<ImageRect> for CImageRect {
    fn from(rect: ImageRect) -> Self {
        Self {
            left: rect.left,
            top: rect.top,
            right: rect.right,
            bottom: rect.bottom,
        }
    }
}

impl From<CImageRect> for ImageRect {
    fn from(rect: CImageRect) -> Self {
        Self {
            left: rect.left,
            top: rect.top,
            right: rect.right,
            bottom: rect.bottom,
        }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ObjectDetectResult {
    pub bbox: CImageRect,
    pub prop: f32,
    pub cls_id: i32,
}

#[repr(C)]
pub struct ObjectDetectResultList {
    pub id: i32,
    pub count: i32,
    pub results: [ObjectDetectResult; OBJ_NUMB_MAX_SIZE],
}

impl Default for ObjectDetectResultList {
    fn default() -> Self {
        Self {
            id: 0,
            count: 0,
            results: [ObjectDetectResult {
                bbox: CImageRect {
                    left: 0,
                    top: 0,
                    right: 0,
                    bottom: 0,
                },
                prop: 0.0,
                cls_id: 0,
            }; OBJ_NUMB_MAX_SIZE],
        }
    }
}

impl ObjectDetectResultList {
    pub fn to_detection_results(&self) -> Vec<DetectionResult> {
        (0..self.count as usize)
            .map(|i| {
                let res = &self.results[i];
                DetectionResult::new(res.bbox.into(), res.prop, res.cls_id)
            })
            .collect()
    }
}

#[repr(C)]
pub struct CImageBuffer {
    pub width: i32,
    pub height: i32,
    pub width_stride: i32,
    pub height_stride: i32,
    pub format: i32,
    pub virt_addr: *mut u8,
    pub size: i32,
    pub fd: i32,
}

pub struct FfiImageBuffer {
    inner: CImageBuffer,
    _data: Vec<u8>, // Keep ownership of data
}

impl FfiImageBuffer {
    pub fn from_image_buffer(img: &crate::types::ImageBuffer) -> Self {
        let mut data = img.data.clone();
        let inner = CImageBuffer {
            width: img.width,
            height: img.height,
            width_stride: img.width,
            height_stride: img.height,
            format: img.format as i32,
            virt_addr: data.as_mut_ptr(),
            size: data.len() as i32,
            fd: -1,
        };

        Self { inner, _data: data }
    }

    pub fn as_ptr(&mut self) -> *mut CImageBuffer {
        &mut self.inner as *mut CImageBuffer
    }
}

impl std::ops::Deref for FfiImageBuffer {
    type Target = CImageBuffer;

    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

impl std::ops::DerefMut for FfiImageBuffer {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.inner
    }
}

// External C functions
extern "C" {
    pub fn init_yolov8_model_wrapper(model_path: *const libc::c_char, app_ctx: *mut RknnAppContext)
        -> i32;

    pub fn release_yolov8_model_wrapper(app_ctx: *mut RknnAppContext) -> i32;

    pub fn inference_yolov8_model_wrapper(
        app_ctx: *mut RknnAppContext,
        img: *mut CImageBuffer,
        od_results: *mut ObjectDetectResultList,
    ) -> i32;

    pub fn init_post_process_wrapper() -> i32;

    pub fn deinit_post_process_wrapper();

    pub fn coco_cls_to_name_wrapper(cls_id: i32) -> *const libc::c_char;

    // Separate step functions for benchmarking
    pub fn yolov8_preprocess_wrapper(
        app_ctx: *mut RknnAppContext,
        img: *mut CImageBuffer,
        dst_img: *mut CImageBuffer,
        letter_box: *mut LetterBox,
    ) -> i32;

    pub fn yolov8_inference_wrapper(
        app_ctx: *mut RknnAppContext,
        dst_img: *mut CImageBuffer,
        outputs: *mut RknnOutput,
    ) -> i32;

    pub fn yolov8_postprocess_wrapper(
        app_ctx: *mut RknnAppContext,
        outputs: *mut RknnOutput,
        letter_box: *mut LetterBox,
        od_results: *mut ObjectDetectResultList,
    ) -> i32;

    pub fn yolov8_release_outputs_wrapper(app_ctx: *mut RknnAppContext, outputs: *mut RknnOutput);
}

// Letterbox structure for preprocessing
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct LetterBox {
    pub x_pad: i32,
    pub y_pad: i32,
    pub scale: f32,
}

impl Default for LetterBox {
    fn default() -> Self {
        Self {
            x_pad: 0,
            y_pad: 0,
            scale: 1.0,
        }
    }
}

// RKNN Output structure matching rknn_api.h
#[repr(C)]
#[derive(Debug)]
pub struct RknnOutput {
    pub want_float: u8,
    pub is_prealloc: u8,
    pub index: u32,
    pub buf: *mut libc::c_void,
    pub size: u32,
}

impl Default for RknnOutput {
    fn default() -> Self {
        Self {
            want_float: 0,
            is_prealloc: 0,
            index: 0,
            buf: std::ptr::null_mut(),
            size: 0,
        }
    }
}

// Convenience wrappers
#[inline]
pub unsafe fn init_yolov8_model(model_path: *const libc::c_char, app_ctx: *mut RknnAppContext) -> i32 {
    init_yolov8_model_wrapper(model_path, app_ctx)
}

#[inline]
pub unsafe fn release_yolov8_model(app_ctx: *mut RknnAppContext) -> i32 {
    release_yolov8_model_wrapper(app_ctx)
}

#[inline]
pub unsafe fn inference_yolov8_model(
    app_ctx: *mut RknnAppContext,
    img: *mut CImageBuffer,
    od_results: *mut ObjectDetectResultList,
) -> i32 {
    inference_yolov8_model_wrapper(app_ctx, img, od_results)
}

#[inline]
pub unsafe fn init_post_process() -> i32 {
    init_post_process_wrapper()
}

#[inline]
pub unsafe fn deinit_post_process() {
    deinit_post_process_wrapper()
}

#[inline]
pub unsafe fn coco_cls_to_name(cls_id: i32) -> *const libc::c_char {
    coco_cls_to_name_wrapper(cls_id)
}

// Separate step wrappers for benchmarking
#[inline]
pub unsafe fn yolov8_preprocess(
    app_ctx: *mut RknnAppContext,
    img: *mut CImageBuffer,
    dst_img: *mut CImageBuffer,
    letter_box: *mut LetterBox,
) -> i32 {
    yolov8_preprocess_wrapper(app_ctx, img, dst_img, letter_box)
}

#[inline]
pub unsafe fn yolov8_inference(
    app_ctx: *mut RknnAppContext,
    dst_img: *mut CImageBuffer,
    outputs: *mut RknnOutput,
) -> i32 {
    yolov8_inference_wrapper(app_ctx, dst_img, outputs)
}

#[inline]
pub unsafe fn yolov8_postprocess(
    app_ctx: *mut RknnAppContext,
    outputs: *mut RknnOutput,
    letter_box: *mut LetterBox,
    od_results: *mut ObjectDetectResultList,
) -> i32 {
    yolov8_postprocess_wrapper(app_ctx, outputs, letter_box, od_results)
}

#[inline]
pub unsafe fn yolov8_release_outputs(app_ctx: *mut RknnAppContext, outputs: *mut RknnOutput) {
    yolov8_release_outputs_wrapper(app_ctx, outputs)
}
