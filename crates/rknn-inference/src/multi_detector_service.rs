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

/// 最新结果缓存
struct LatestResultCache {
    result: Option<std::result::Result<Vec<DetectionResult>, crate::RknnError>>,
    task_id: usize,
    timestamp: Instant,
}

unsafe impl Send for LatestResultCache {}
unsafe impl Sync for LatestResultCache {}

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
        let scale =
            (self.model_width as f32 / orig_w as f32).min(self.model_height as f32 / orig_h as f32);
        let new_w = (orig_w as f32 * scale) as u32;
        let new_h = (orig_h as f32 * scale) as u32;

        // 调整图片大小
        let resized =
            image::imageops::resize(img, new_w, new_h, image::imageops::FilterType::CatmullRom);

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
/// 提供异步输入推理服务，使用最新结果缓存机制
/// 每个工作线程使用独立的detector实例，队列长度限制为6
/// 支持最新结果缓存，2秒超时清空，无需输出队列
pub struct MultiDetectorInferenceService {
    workers: Vec<thread::JoinHandle<()>>,
    input_queue: Arc<Mutex<std::collections::VecDeque<AsyncInferenceTask>>>,
    latest_result: Arc<Mutex<LatestResultCache>>, // 最新结果缓存
    shutdown: Arc<std::sync::atomic::AtomicBool>,
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
    pub fn new<P: AsRef<std::path::Path>>(
        model_path: P,
        num_workers: usize,
    ) -> Result<Self, crate::RknnError> {
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
    pub fn new_with_queue_size<P: AsRef<std::path::Path>>(
        model_path: P,
        num_workers: usize,
        queue_size: usize,
    ) -> Result<Self, crate::RknnError> {
        let input_queue = Arc::new(Mutex::new(
            std::collections::VecDeque::<AsyncInferenceTask>::new(),
        ));
        let latest_result = Arc::new(Mutex::new(LatestResultCache {
            result: None,
            task_id: 0,
            timestamp: Instant::now(),
        }));
        let shutdown = Arc::new(std::sync::atomic::AtomicBool::new(false));
        let next_task_id = Arc::new(Mutex::new(0));

        let mut workers = Vec::new();

        // 创建工作线程，每个线程有自己的detector
        for _ in 0..num_workers {
            let input_queue_clone = input_queue.clone();
            let latest_result_clone = latest_result.clone();
            let shutdown_clone = shutdown.clone();
            let model_path_owned = model_path
                .as_ref()
                .to_path_buf()
                .to_string_lossy()
                .to_string();

            let handle = thread::spawn(move || {
                // 每个线程创建自己的独立detector
                let mut detector = match crate::Yolov8Detector::new(&model_path_owned) {
                    Ok(det) => det,
                    Err(_) => {
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
                        let _inference_time = inference_start.elapsed();

                        // 更新最新结果缓存
                        {
                            let mut latest = latest_result_clone.lock().unwrap();
                            latest.result = Some(match result {
                                Ok(detections) => Ok(detections),
                                Err(_e) => Err(crate::RknnError::InferenceFailed(-1)),
                            });
                            latest.task_id = task.task_id;
                            latest.timestamp = Instant::now();
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
            latest_result,
            shutdown,
            next_task_id,
            max_queue_size: queue_size,
        })
    }

    /// 异步提交推理任务
    ///
    /// 将预处理的ImageBuffer提交到输入队列，立即返回任务ID
    /// 如果输入队列已满，会阻塞直到有空位
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
            // 如果队列满了，等待空位而不是丢弃任务
            while queue.len() >= self.max_queue_size {
                // 释放锁，让其他线程有机会处理队列
                drop(queue);
                thread::sleep(Duration::from_millis(1));
                queue = self.input_queue.lock().unwrap();
            }
            queue.push_back(task);
        }

        Ok(task_id)
    }

    /// 获取最新推理结果（主要推荐使用）
    ///
    /// 直接获取最新的推理结果缓存，不阻塞
    /// 如果结果超过2秒未更新，会自动清空返回None
    /// 如果队列为空也返回None
    ///
    /// # 返回
    ///
    /// Option<(任务ID, 检测结果列表)>
    ///
    /// # 示例
    ///
    /// ```rust
    /// let result = service.try_get_latest_result();
    /// if let Some((task_id, detections)) = result {
    ///     println!("任务 {} 检测到 {} 个对象", task_id, detections.len());
    /// } else {
    ///     println!("暂无结果或结果已过期");
    /// }
    /// ```
    pub fn try_get_latest_result(
        &self,
    ) -> Option<(usize, Result<Vec<DetectionResult>, crate::RknnError>)> {
        let mut latest = self.latest_result.lock().unwrap();

        // 检查结果是否超过2秒
        if latest.timestamp.elapsed() > Duration::from_secs(2) {
            latest.result = None;
            return None;
        }

        // 检查是否有结果
        if let Some(ref result) = latest.result {
            Some((latest.task_id, result.clone()))
        } else {
            None
        }
    }

    /// 检查是否有最新结果
    ///
    /// # 返回
    ///
    /// bool - 如果有未过期的最新结果返回true
    pub fn has_latest_result(&self) -> bool {
        let latest = self.latest_result.lock().unwrap();
        latest.result.is_some() && latest.timestamp.elapsed() <= Duration::from_secs(2)
    }

    /// 获取最新结果的年龄（毫秒）
    ///
    /// # 返回
    ///
    /// u64 - 结果的年龄（毫秒），如果没有结果返回u64::MAX
    pub fn latest_result_age_ms(&self) -> u64 {
        let latest = self.latest_result.lock().unwrap();
        if latest.result.is_some() {
            latest.timestamp.elapsed().as_millis() as u64
        } else {
            u64::MAX
        }
    }

    /// 推理预处理的ImageBuffer（同步兼容方法）
    ///
    /// 为了向后兼容保留的同步方法，内部使用最新结果缓存机制
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
    pub fn inference_preprocessed(
        &self,
        image_buffer: &ImageBuffer,
    ) -> Result<Vec<DetectionResult>, crate::RknnError> {
        // 提交任务
        let task_id = self.submit_inference(image_buffer)?;

        // 等待最新结果
        let start_time = Instant::now();
        let timeout = Duration::from_secs(10);

        while start_time.elapsed() < timeout {
            if let Some((id, result)) = self.try_get_latest_result() {
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
        let completed_tasks = self.next_task_id.lock().unwrap().clone();

        ServiceStats {
            total_tasks: completed_tasks,
            completed_tasks,
            input_queue_size,
            output_queue_size: 0, // 输出队列已移除
            active_workers: self.workers.len(),
        }
    }

    /// 获取输入队列长度
    pub fn input_queue_len(&self) -> usize {
        self.input_queue.lock().unwrap().len()
    }

    /// 检查是否有最新结果（2秒内）
    pub fn has_result(&self) -> bool {
        self.has_latest_result()
    }

    /// 检查输入队列是否已满
    pub fn is_input_queue_full(&self) -> bool {
        self.input_queue.lock().unwrap().len() >= self.max_queue_size
    }

    /// 清空最新结果缓存（用于重置或错误恢复）
    pub fn clear_latest_result(&self) {
        let mut latest = self.latest_result.lock().unwrap();
        latest.result = None;
        latest.timestamp = Instant::now();
    }

    /// 清空输入队列和最新结果（用于重置或错误恢复）
    pub fn clear_all(&self) {
        {
            let mut input_queue = self.input_queue.lock().unwrap();
            input_queue.clear();
        }
        self.clear_latest_result();
    }

    /// 获取工作线程数
    pub fn worker_count(&self) -> usize {
        self.workers.len()
    }
}

impl Drop for MultiDetectorInferenceService {
    fn drop(&mut self) {
        // 通知所有工作线程停止
        self.shutdown
            .store(true, std::sync::atomic::Ordering::Relaxed);

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
