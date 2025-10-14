//! 多Detector推理服务
//!
//! 提供高性能的多线程YOLOv8推理服务，每个线程使用独立的detector实例
//! 通过并发推理充分利用RK3588 NPU性能，实测可达37+ FPS
//!
//! 现在支持异步输入和输出，输入和输出都是队列，避免阻塞
//!
//! # 异步使用示例
//!
//! ```rust
//! use rknn_inference::{MultiDetectorInferenceService, ImageBuffer};
//!
//! // 创建6个detector的推理服务，队列长度限制为6
//! let service = MultiDetectorInferenceService::new("models/yolov8m.rknn", 6)?;
//!
//! // 加载并预处理图片
//! let img = image::open("test.jpg")?.to_rgb8();
//! let preprocessor = ImagePreprocessor::new();
//! let image_buffer = preprocessor.preprocess(&img);
//!
//! // 异步提交多个推理任务
//! let mut task_ids = Vec::new();
//! for i in 0..5 {
//!     let task_id = service.submit_inference(&image_buffer)?;
//!     task_ids.push(task_id);
//!     println!("提交任务 {}", task_id);
//! }
//!
//! // 异步获取结果
//! loop {
//!     if let Some((task_id, results)) = service.try_get_result() {
//!         match results {
//!             Ok(detections) => println!("任务 {} 检测到 {} 个对象", task_id, detections.len()),
//!             Err(e) => println!("任务 {} 推理失败: {:?}", task_id, e),
//!         }
//!
//!         // 如果所有任务都完成了，退出循环
//!         if task_ids.contains(&task_id) {
//!             task_ids.retain(|&id| id != task_id);
//!         }
//!         if task_ids.is_empty() {
//!             break;
//!         }
//!     } else {
//!         // 暂无结果，可以处理其他事情
//!         std::thread::sleep(std::time::Duration::from_millis(10));
//!     }
//! }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! # 同步兼容示例
//!
//! 为了向后兼容，保留了原有的同步接口：
//!
//! ```rust
//! use rknn_inference::{MultiDetectorInferenceService, ImageBuffer};
//!
//! let service = MultiDetectorInferenceService::new("models/yolov8m.rknn", 6)?;
//! let img = image::open("test.jpg")?.to_rgb8();
//! let preprocessor = ImagePreprocessor::new();
//! let image_buffer = preprocessor.preprocess(&img);
//!
//! // 同步推理（内部使用异步机制）
//! let results = service.inference_preprocessed(&image_buffer)?;
//! println!("检测到 {} 个对象", results.len());
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```

use crate::{DetectionResult, ImageBuffer};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use image;

/// 异步推理任务
struct AsyncInferenceTask {
    task_id: usize,
    preprocessed_image: ImageBuffer,
}

/// 异步推理结果
struct AsyncInferenceResult {
    task_id: usize,
    result: std::result::Result<Vec<DetectionResult>, Box<dyn std::error::Error + Send + Sync>>,
}

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

/// 异步多Detector推理服务
///
/// 提供异步输入和输出的推理服务，输入和输出都是队列，避免阻塞
/// 每个工作线程使用独立的detector实例，队列长度限制为6
pub struct MultiDetectorInferenceService {
    workers: Vec<thread::JoinHandle<()>>,
    input_queue: Arc<Mutex<std::collections::VecDeque<AsyncInferenceTask>>>,
    output_queue: Arc<Mutex<std::collections::VecDeque<AsyncInferenceResult>>>,
    shutdown: Arc<std::sync::atomic::AtomicBool>,
    preprocessor: ImagePreprocessor,
    next_task_id: Arc<Mutex<usize>>,
    max_queue_size: usize,
}

/// 服务统计信息
#[derive(Debug, Clone)]
pub struct ServiceStats {
    pub total_tasks: usize,
    pub completed_tasks: usize,
    pub input_queue_size: usize,
    pub output_queue_size: usize,
    pub active_workers: usize,
}

impl MultiDetectorInferenceService {
    /// 创建新的异步多Detector推理服务
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
        Self::new_with_queue_size(model_path, num_workers, 6)
    }

    /// 创建新的异步多Detector推理服务，指定队列大小
    ///
    /// # 参数
    ///
    /// * `model_path` - RKNN模型文件路径
    /// * `num_workers` - 工作线程数（推荐6个以获得最佳性能）
    /// * `queue_size` - 队列最大长度（默认6）
    ///
    /// # 示例
    ///
    /// ```rust
    /// let service = MultiDetectorInferenceService::new_with_queue_size("models/yolov8m.rknn", 6, 6)?;
    /// # Ok::<(), Box<dyn std::error::Error>>(())
    /// ```
    pub fn new_with_queue_size<P: AsRef<std::path::Path>>(model_path: P, num_workers: usize, queue_size: usize) -> Result<Self, crate::RknnError> {
        let input_queue = Arc::new(Mutex::new(std::collections::VecDeque::<AsyncInferenceTask>::new()));
        let output_queue = Arc::new(Mutex::new(std::collections::VecDeque::<AsyncInferenceResult>::new()));
        let shutdown = Arc::new(std::sync::atomic::AtomicBool::new(false));
        let next_task_id = Arc::new(Mutex::new(0));

        let mut workers = Vec::new();

        // 创建工作线程，每个线程有自己的detector
        for i in 0..num_workers {
            let input_queue_clone = input_queue.clone();
            let output_queue_clone = output_queue.clone();
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
                        let mut queue = input_queue_clone.lock().unwrap();
                        queue.pop_front()
                    };

                    if let Some(task) = task {
                        // 直接推理，无需等待锁
                        let inference_start = Instant::now();
                        let result = detector.detect(&task.preprocessed_image);
                        let inference_time = inference_start.elapsed();

                        // 将结果放入输出队列
                        let inference_result = AsyncInferenceResult {
                            task_id: task.task_id,
                            result: match result {
                                Ok(detections) => Ok(detections),
                                Err(e) => Err(Box::new(e) as Box<dyn std::error::Error + Send + Sync>),
                            },
                        };

                        {
                            let mut output_queue = output_queue_clone.lock().unwrap();
                            // 限制输出队列大小，防止内存积压
                            if output_queue.len() >= queue_size {
                                output_queue.pop_front(); // 丢弃最旧的结果
                            }
                            output_queue.push_back(inference_result);
                        }

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
            input_queue,
            output_queue,
            shutdown,
            preprocessor: ImagePreprocessor::new(),
            next_task_id,
            max_queue_size: queue_size,
        })
    }

    /// 异步提交推理任务
    ///
    /// 将预处理的ImageBuffer提交到输入队列，立即返回任务ID
    /// 如果输入队列已满，会丢弃最旧的任务
    ///
    /// # 参数
    ///
    /// * `image_buffer` - 预处理后的ImageBuffer (640x640 RGB888格式)
    ///
    /// # 返回
    ///
    /// 任务ID，用于后续获取结果
    ///
    /// # 示例
    ///
    /// ```rust
    /// let preprocessor = ImagePreprocessor::new();
    /// let image_buffer = preprocessor.create_image_buffer(rgb_data, 640, 640);
    /// let task_id = service.submit_inference(&image_buffer)?;
    /// # Ok::<(), Box<dyn std::error::Error>>(())
    /// ```
    pub fn submit_inference(&self, image_buffer: &ImageBuffer) -> Result<usize, crate::RknnError> {
        // 获取任务ID
        let task_id = {
            let mut id = self.next_task_id.lock().unwrap();
            *id += 1;
            *id - 1
        };

        let task = AsyncInferenceTask {
            task_id,
            preprocessed_image: image_buffer.clone(),
        };

        {
            let mut queue = self.input_queue.lock().unwrap();
            // 限制输入队列大小，防止内存积压
            if queue.len() >= self.max_queue_size {
                queue.pop_front(); // 丢弃最旧的任务
            }
            queue.push_back(task);
        }

        Ok(task_id)
    }

    /// 异步获取推理结果
    ///
    /// 从输出队列获取推理结果，如果队列为空则返回None
    /// 这个方法不会阻塞，调用后立即返回
    ///
    /// # 返回
    ///
    /// Option<(任务ID, 检测结果列表)>
    ///
    /// # 示例
    ///
    /// ```rust
    /// let result = service.try_get_result();
    /// if let Some((task_id, detections)) = result {
    ///     println!("任务 {} 检测到 {} 个对象", task_id, detections.len());
    /// } else {
    ///     println!("暂无结果");
    /// }
    /// ```
    pub fn try_get_result(&self) -> Option<(usize, Result<Vec<DetectionResult>, crate::RknnError>)> {
        let mut queue = self.output_queue.lock().unwrap();
        if let Some(async_result) = queue.pop_front() {
            let task_id = async_result.task_id;
            let result = match async_result.result {
                Ok(detections) => Ok(detections),
                Err(_) => Err(crate::RknnError::InferenceFailed(-1)),
            };
            Some((task_id, result))
        } else {
            None
        }
    }

    /// 获取多个推理结果（直到队列为空）
    ///
    /// 一次性获取输出队列中的所有结果，提高效率
    ///
    /// # 返回
    ///
    /// Vec<(任务ID, 检测结果列表)>
    ///
    /// # 示例
    ///
    /// ```rust
    /// let results = service.get_all_results();
    /// for (task_id, detections) in results {
    ///     println!("任务 {} 检测到 {} 个对象", task_id, detections.len());
    /// }
    /// ```
    pub fn get_all_results(&self) -> Vec<(usize, Result<Vec<DetectionResult>, crate::RknnError>)> {
        let mut queue = self.output_queue.lock().unwrap();
        let mut results = Vec::new();

        while let Some(async_result) = queue.pop_front() {
            let task_id = async_result.task_id;
            let result = match async_result.result {
                Ok(detections) => Ok(detections),
                Err(_) => Err(crate::RknnError::InferenceFailed(-1)),
            };
            results.push((task_id, result));
        }

        results
    }

    /// 推理预处理的ImageBuffer（同步兼容方法）
    ///
    /// 为了向后兼容保留的同步方法，内部使用异步机制
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
        // 提交任务
        let task_id = self.submit_inference(image_buffer)?;

        // 等待结果
        let start_time = Instant::now();
        let timeout = Duration::from_secs(10);

        while start_time.elapsed() < timeout {
            if let Some((id, result)) = self.try_get_result() {
                if id == task_id {
                    return result;
                }
            }
            thread::sleep(Duration::from_millis(1));
        }

        Err(crate::RknnError::InferenceFailed(-2)) // 超时
    }

    /// 获取服务统计信息
    pub fn get_stats(&self) -> ServiceStats {
        let input_queue_size = self.input_queue.lock().unwrap().len();
        let output_queue_size = self.output_queue.lock().unwrap().len();
        let completed_tasks = self.next_task_id.lock().unwrap().clone();

        ServiceStats {
            total_tasks: completed_tasks,
            completed_tasks,
            input_queue_size,
            output_queue_size,
            active_workers: self.workers.len(),
        }
    }

    /// 获取输入队列长度
    pub fn input_queue_len(&self) -> usize {
        self.input_queue.lock().unwrap().len()
    }

    /// 获取输出队列长度
    pub fn output_queue_len(&self) -> usize {
        self.output_queue.lock().unwrap().len()
    }

    /// 检查输入队列是否已满
    pub fn is_input_queue_full(&self) -> bool {
        self.input_queue.lock().unwrap().len() >= self.max_queue_size
    }

    /// 检查输出队列是否为空
    pub fn is_output_queue_empty(&self) -> bool {
        self.output_queue.lock().unwrap().is_empty()
    }

    /// 清空所有队列（用于重置或错误恢复）
    pub fn clear_all_queues(&self) {
        {
            let mut input_queue = self.input_queue.lock().unwrap();
            input_queue.clear();
        }
        {
            let mut output_queue = self.output_queue.lock().unwrap();
            output_queue.clear();
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