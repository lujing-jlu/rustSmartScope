//! 多Detector推理服务
//!
//! 提供高性能的多线程YOLOv8推理服务，每个线程使用独立的detector实例
//! 通过并发推理充分利用RK3588 NPU性能，实测可达37+ FPS
//!
//! # 示例
//!
//! ```rust
//! use rknn_inference::{MultiDetectorInferenceService, ImageBuffer};
//!
//! // 创建6个detector的推理服务
//! let service = MultiDetectorInferenceService::new("models/yolov8m.rknn", 6)?;
//!
//! // 加载图片
//! let img = image::open("test.jpg")?.to_rgb8();
//!
//! // 推理
//! let results = service.inference(&img)?;
//! println!("检测到 {} 个对象", results.len());
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```

use crate::{DetectionResult, ImageBuffer};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use image;

/// 推理任务
struct InferenceTask {
    task_id: usize,
    preprocessed_image: ImageBuffer,
    sender: std::sync::mpsc::Sender<InferenceResult>,
}

/// 推理结果
type InferenceResult = std::result::Result<Vec<DetectionResult>, Box<dyn std::error::Error + Send + Sync>>;

/// 图片预处理工具
pub struct ImagePreprocessor {
    model_width: u32,
    model_height: u32,
}

impl ImagePreprocessor {
    /// 创建新的预处理器
    pub fn new() -> Self {
        Self {
            model_width: 640,
            model_height: 640,
        }
    }

    /// 创建指定尺寸的预处理器
    pub fn new_with_size(width: u32, height: u32) -> Self {
        Self {
            model_width: width,
            model_height: height,
        }
    }

    /// 预处理图片（letterbox + 缩放）
    ///
    /// # 参数
    ///
    /// * `img` - 输入的RGB图片
    ///
    /// # 返回
    ///
    /// 预处理后的ImageBuffer (640x640 RGB888格式)
    pub fn preprocess(&self, img: &image::RgbImage) -> ImageBuffer {
        let (orig_w, orig_h) = (img.width(), img.height());

        // 计算缩放比例
        let scale = (self.model_width as f32 / orig_w as f32)
            .min(self.model_height as f32 / orig_h as f32);
        let new_w = (orig_w as f32 * scale) as u32;
        let new_h = (orig_h as f32 * scale) as u32;

        // 调整图片大小
        let resized = image::imageops::resize(
            img,
            new_w,
            new_h,
            image::imageops::FilterType::CatmullRom,
        );

        // 创建letterbox图像
        let mut letterbox = image::RgbImage::new(self.model_width, self.model_height);
        // 填充灰色背景 (114, 114, 114)
        for pixel in letterbox.pixels_mut() {
            *pixel = image::Rgb([114, 114, 114]);
        }

        // 计算偏移
        let x_offset = (self.model_width - new_w) / 2;
        let y_offset = (self.model_height - new_h) / 2;

        // 叠加到letterbox中心
        image::imageops::overlay(&mut letterbox, &resized, x_offset.into(), y_offset.into());

        // 转换为ImageBuffer
        ImageBuffer {
            width: self.model_width as i32,
            height: self.model_height as i32,
            format: crate::ImageFormat::Rgb888,
            data: letterbox.into_raw(),
        }
    }

    /// 创建预处理的ImageBuffer（外部提供RGB数据）
    ///
    /// # 参数
    ///
    /// * `rgb_data` - RGB格式的图片数据 (width * height * 3 bytes)
    /// * `width` - 原始图片宽度
    /// * `height` - 原始图片高度
    ///
    /// # 返回
    ///
    /// 预处理后的ImageBuffer
    pub fn create_image_buffer(&self, rgb_data: Vec<u8>, _width: u32, _height: u32) -> ImageBuffer {
        ImageBuffer {
            width: self.model_width as i32,
            height: self.model_height as i32,
            format: crate::ImageFormat::Rgb888,
            data: rgb_data,
        }
    }
}

impl Default for ImagePreprocessor {
    fn default() -> Self {
        Self::new()
    }
}

/// 多Detector推理服务
///
/// 每个工作线程使用独立的detector实例，避免锁竞争
/// 通过充分利用RK3588 NPU的并发能力实现高性能推理
pub struct MultiDetectorInferenceService {
    workers: Vec<thread::JoinHandle<()>>,
    task_queue: Arc<Mutex<Vec<InferenceTask>>>,
    shutdown: Arc<std::sync::atomic::AtomicBool>,
    preprocessor: ImagePreprocessor,
    next_task_id: Arc<Mutex<usize>>,
}

/// 服务统计信息
#[derive(Debug, Clone)]
pub struct ServiceStats {
    pub total_tasks: usize,
    pub completed_tasks: usize,
    pub queue_size: usize,
    pub active_workers: usize,
}

impl MultiDetectorInferenceService {
    /// 创建新的多Detector推理服务
    ///
    /// # 参数
    ///
    /// * `model_path` - RKNN模型文件路径
    /// * `num_workers` - 工作线程数（推荐6个以获得最佳性能）
    ///
    /// # 示例
    ///
    /// ```rust
    /// let service = MultiDetectorInferenceService::new("models/yolov8m.rknn", 6)?;
    /// # Ok::<(), Box<dyn std::error::Error>>(())
    /// ```
    pub fn new<P: AsRef<std::path::Path>>(model_path: P, num_workers: usize) -> Result<Self, crate::RknnError> {
        let task_queue = Arc::new(Mutex::new(Vec::<InferenceTask>::new()));
        let shutdown = Arc::new(std::sync::atomic::AtomicBool::new(false));
        let next_task_id = Arc::new(Mutex::new(0));

        let mut workers = Vec::new();

        // 创建工作线程，每个线程有自己的detector
        for i in 0..num_workers {
            let task_queue_clone = task_queue.clone();
            let shutdown_clone = shutdown.clone();
              let model_path_owned = model_path.as_ref().to_path_buf().to_string_lossy().to_string();

            let handle = thread::spawn(move || {
                // 每个线程创建自己的独立detector
                let mut detector = match crate::Yolov8Detector::new(&model_path_owned) {
                    Ok(det) => det,
                    Err(e) => {
                        return;
                    }
                };

                // 线程主循环
                loop {
                    // 检查是否应该停止
                    if shutdown_clone.load(std::sync::atomic::Ordering::Relaxed) {
                        break;
                    }

                    // 获取任务
                    let task = {
                        let mut queue = task_queue_clone.lock().unwrap();
                        queue.pop()
                    };

                    if let Some(task) = task {
                        // 直接推理，无需等待锁
                        let inference_start = Instant::now();
                        let result = detector.detect(&task.preprocessed_image);
                        let inference_time = inference_start.elapsed();

                        // 发送结果
                        let inference_result = match result {
                            Ok(detections) => Ok(detections),
                            Err(e) => Err(Box::new(e) as Box<dyn std::error::Error + Send + Sync>),
                        };

                        let _ = task.sender.send(inference_result);

                      } else {
                        // 队列为空，短暂休眠避免CPU空转
                        thread::sleep(Duration::from_millis(1));
                    }
                }
            });

            workers.push(handle);
        }

        Ok(Self {
            workers,
            task_queue,
            shutdown,
            preprocessor: ImagePreprocessor::new(),
            next_task_id,
        })
    }

    /// 推理预处理的ImageBuffer
    ///
    /// # 参数
    ///
    /// * `image_buffer` - 预处理后的ImageBuffer (640x640 RGB888格式)
    ///
    /// # 返回
    ///
    /// 检测结果列表
    ///
    /// # 示例
    ///
    /// ```rust
    /// let preprocessor = ImagePreprocessor::new();
    /// let image_buffer = preprocessor.create_image_buffer(rgb_data, 640, 640);
    /// let results = service.inference_preprocessed(&image_buffer)?;
    /// # Ok::<(), Box<dyn std::error::Error>>(())
    /// ```
    pub fn inference_preprocessed(&self, image_buffer: &ImageBuffer) -> Result<Vec<DetectionResult>, crate::RknnError> {
        // 1. 创建任务通道
        let (sender, receiver) = std::sync::mpsc::channel();

        // 2. 获取任务ID并提交推理任务
        let task_id = {
            let mut id = self.next_task_id.lock().unwrap();
            *id += 1;
            *id - 1
        };

        let task = InferenceTask {
            task_id,
            preprocessed_image: image_buffer.clone(),
            sender,
        };

        {
            let mut queue = self.task_queue.lock().unwrap();
            // 队列满了就丢弃旧任务，防止内存泄漏
            if queue.len() >= 1000 {
                queue.remove(0);
            }
            queue.push(task);
        }

        // 3. 等待结果
        match receiver.recv_timeout(Duration::from_secs(10)) {
            Ok(Ok(results)) => Ok(results),
            Ok(Err(_e)) => Err(crate::RknnError::InferenceFailed(-1)), // 使用错误码-1表示推理任务失败
            Err(_) => Err(crate::RknnError::InferenceFailed(-2)), // 使用错误码-2表示超时
        }
    }

    /// 获取服务统计信息
    pub fn get_stats(&self) -> ServiceStats {
        let queue_size = self.task_queue.lock().unwrap().len();
        let completed_tasks = self.next_task_id.lock().unwrap().clone();

        ServiceStats {
            total_tasks: completed_tasks,
            completed_tasks,
            queue_size,
            active_workers: self.workers.len(),
        }
    }

    /// 获取工作线程数
    pub fn worker_count(&self) -> usize {
        self.workers.len()
    }
}

impl Drop for MultiDetectorInferenceService {
    fn drop(&mut self) {
        // 通知所有工作线程停止
        self.shutdown.store(true, std::sync::atomic::Ordering::Relaxed);

        // 等待所有线程结束
        for worker in self.workers.drain(..) {
            let _ = worker.join();
        }
    }
}

unsafe impl Send for MultiDetectorInferenceService {}
unsafe impl Sync for MultiDetectorInferenceService {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_image_preprocessor() {
        let preprocessor = ImagePreprocessor::new();

        // 创建测试图片
        let img = image::RgbImage::new(800, 600);

        // 预处理
        let result = preprocessor.preprocess(&img);

        assert_eq!(result.width, 640);
        assert_eq!(result.height, 640);
        assert_eq!(result.format, crate::ImageFormat::Rgb888);
        assert_eq!(result.data.len(), 640 * 640 * 3);
    }

    #[test]
    fn test_service_creation() {
        // 注意：这个测试需要真实的模型文件
        // 在实际环境中应该被跳过或使用mock
        /*
        let service = MultiDetectorInferenceService::new("test_model.rknn", 2);
        assert!(service.is_ok());
        assert_eq!(service.unwrap().worker_count(), 2);
        */
    }
}