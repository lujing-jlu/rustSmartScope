use std::ffi::CStr;
use std::os::raw::{c_char, c_int};
use std::sync::Mutex as StdMutex;
use std::sync::atomic::{AtomicBool, Ordering};

use crate::types::{CDetection, ErrorCode};

use rknn_inference::MultiDetectorInferenceService;

lazy_static! {
    static ref AI_SERVICE: StdMutex<Option<MultiDetectorInferenceService>> = StdMutex::new(None);
    static ref AI_ENABLED: AtomicBool = AtomicBool::new(false);
}

const MAX_DETECTIONS: usize = 128;

/// 初始化AI推理服务（RKNN YOLOv8）
/// model_path: RKNN模型路径，如 "models/yolov8m.rknn"
/// num_workers: 工作线程数量，建议6
#[no_mangle]
pub extern "C" fn smartscope_ai_init(model_path: *const c_char, num_workers: i32) -> c_int {
    if model_path.is_null() || num_workers <= 0 {
        return ErrorCode::Error as c_int;
    }

    let path_str = unsafe {
        match CStr::from_ptr(model_path).to_str() {
            Ok(s) => s,
            Err(_) => return ErrorCode::Error as c_int,
        }
    };

    let mut guard = AI_SERVICE.lock().unwrap();
    match MultiDetectorInferenceService::new(path_str, num_workers as usize) {
        Ok(service) => {
            *guard = Some(service);
            AI_ENABLED.store(false, Ordering::Relaxed);
            tracing::info!(
                "AI inference service initialized: {} workers, model: {}",
                num_workers,
                path_str
            );
            ErrorCode::Success as c_int
        }
        Err(e) => {
            tracing::error!("Failed to init AI service: {:?}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 关闭AI推理服务
#[no_mangle]
pub extern "C" fn smartscope_ai_shutdown() {
    let mut guard = AI_SERVICE.lock().unwrap();
    *guard = None;
    AI_ENABLED.store(false, Ordering::Relaxed);
    tracing::info!("AI inference service shutdown");
}

/// 启用/禁用AI检测
#[no_mangle]
pub extern "C" fn smartscope_ai_set_enabled(enabled: bool) {
    AI_ENABLED.store(enabled, Ordering::Relaxed);
}

/// 查询AI检测是否启用
#[no_mangle]
pub extern "C" fn smartscope_ai_is_enabled() -> bool {
    AI_ENABLED.load(Ordering::Relaxed)
}

/// 提交一帧RGB888图像到AI推理服务（非阻塞）
/// data指向RGB888数据（width*height*3字节）
#[no_mangle]
pub extern "C" fn smartscope_ai_submit_rgb888(
    width: i32,
    height: i32,
    data: *const u8,
    len: usize,
) -> c_int {
    if !AI_ENABLED.load(Ordering::Relaxed) {
        return ErrorCode::Success as c_int; // 检测未启用时忽略
    }

    if width <= 0 || height <= 0 || data.is_null() {
        return ErrorCode::Error as c_int;
    }

    let guard = AI_SERVICE.lock().unwrap();
    let service = match guard.as_ref() {
        Some(s) => s,
        None => return ErrorCode::Error as c_int,
    };

    let data_slice = unsafe {
        std::slice::from_raw_parts(data, len.min((width as usize) * (height as usize) * 3))
    };

    // 使用预处理器进行letterbox+缩放
    let rgb_image = match image::RgbImage::from_raw(width as u32, height as u32, data_slice.to_vec()) {
        Some(img) => img,
        None => return ErrorCode::Error as c_int,
    };

    let preprocessor = rknn_inference::ImagePreprocessor::new();
    let image_buffer = preprocessor.preprocess(&rgb_image);

    match service.submit_inference(&image_buffer) {
        Ok(_) => ErrorCode::Success as c_int,
        Err(e) => {
            tracing::error!("AI submit failed: {:?}", e);
            ErrorCode::Error as c_int
        }
    }
}

/// 尝试获取最新推理结果
/// 返回：检测数量（>=0），无结果或过期返回0，错误返回-1
#[no_mangle]
pub extern "C" fn smartscope_ai_try_get_latest_result(
    results_out: *mut CDetection,
    max_results: i32,
) -> c_int {
    if results_out.is_null() || max_results <= 0 {
        return -1;
    }

    if !AI_ENABLED.load(Ordering::Relaxed) {
        return 0;
    }

    let guard = AI_SERVICE.lock().unwrap();
    let service = match guard.as_ref() {
        Some(s) => s,
        None => return -1,
    };

    if let Some((_task_id, result)) = service.try_get_latest_result() {
        match result {
            Ok(dets) => {
                let count = dets.len().min(max_results as usize).min(MAX_DETECTIONS);
                for i in 0..count {
                    let d = dets[i];
                    unsafe {
                        let out = results_out.add(i);
                        (*out).left = d.bbox.left;
                        (*out).top = d.bbox.top;
                        (*out).right = d.bbox.right;
                        (*out).bottom = d.bbox.bottom;
                        (*out).confidence = d.confidence;
                        (*out).class_id = d.class_id;
                    }
                }
                count as c_int
            }
            Err(_e) => 0,
        }
    } else {
        0
    }
}

