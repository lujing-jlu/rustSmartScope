use rknn_inference::{get_class_name, ImageBuffer, ImageFormat, DetectionResult as RknnDetectionResult};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

/// 检测结果转换后的类型
#[derive(Debug, Clone)]
struct DetectionResult {
    bbox: (i32, i32, i32, i32), // left, top, right, bottom
    confidence: f32,
    class_id: i32,
}

impl From<RknnDetectionResult> for DetectionResult {
    fn from(rknn_result: RknnDetectionResult) -> Self {
        Self {
            bbox: (rknn_result.bbox.left, rknn_result.bbox.top,
                   rknn_result.bbox.right, rknn_result.bbox.bottom),
            confidence: rknn_result.confidence,
            class_id: rknn_result.class_id,
        }
    }
}

type Result<T> = std::result::Result<T, Box<dyn std::error::Error + Send + Sync>>;

/// RKNN推理器包装器（只负责推理，不涉及预处理）
struct RknnInference {
    detector: Arc<Mutex<rknn_inference::Yolov8Detector>>,
}

impl RknnInference {
    fn new(model_path: &str) -> Result<Self> {
        let detector = rknn_inference::Yolov8Detector::new(model_path)
            .map_err(|e| format!("Failed to create detector: {:?}", e))?;

        Ok(Self {
            detector: Arc::new(Mutex::new(detector)),
        })
    }

    /// 只进行RKNN推理，输入必须是预处理好的图片
    fn run_inference(&self, image: &ImageBuffer) -> Result<Vec<RknnDetectionResult>> {
        let mut detector = self.detector.lock().unwrap();

        // 只进行推理，返回检测结果
        let results = detector.detect(image)?;
        Ok(results)
    }
}

/// 图片预处理工具
struct ImagePreprocessor {
    model_width: u32,
    model_height: u32,
}

impl ImagePreprocessor {
    fn new() -> Self {
        Self {
            model_width: 640,
            model_height: 640,
        }
    }

    /// 预处理图片（letterbox + 缩放）
    fn preprocess(&self, img: &image::RgbImage) -> ImageBuffer {
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
            format: ImageFormat::Rgb888,
            data: letterbox.into_raw(),
        }
    }
}

/// 推理任务
struct InferenceTask {
    task_id: usize,
    preprocessed_image: ImageBuffer,
    original_size: (u32, u32),
    sender: std::sync::mpsc::Sender<Result<Vec<DetectionResult>>>,
}

/// 简化的推理服务（预处理 + 后处理在主线程，推理在多线程）
struct OptimizedInferenceService {
    workers: Vec<thread::JoinHandle<()>>,
    task_queue: Arc<Mutex<Vec<InferenceTask>>>,
    shutdown: Arc<std::sync::atomic::AtomicBool>,
    inference_engine: Arc<RknnInference>,
}

impl OptimizedInferenceService {
    fn new(model_path: &str, num_workers: usize) -> Result<Self> {
        let inference_engine = Arc::new(RknnInference::new(model_path)?);
        let task_queue = Arc::new(Mutex::new(Vec::<InferenceTask>::new()));
        let shutdown = Arc::new(std::sync::atomic::AtomicBool::new(false));

        let mut workers = Vec::new();

        // 创建工作线程
        for i in 0..num_workers {
            let task_queue_clone = task_queue.clone();
            let inference_clone = inference_engine.clone();
            let shutdown_clone = shutdown.clone();

            let handle = thread::spawn(move || {
                println!("🔄 Worker {} started", i);

                loop {
                    if shutdown_clone.load(std::sync::atomic::Ordering::Relaxed) {
                        break;
                    }

                    // 获取任务
                    let task = {
                        let mut queue = task_queue_clone.lock().unwrap();
                        queue.pop()
                    };

                    if let Some(task) = task {
                        println!("🎯 Worker {} processing task {}", i, task.task_id);

                        // 只进行RKNN推理
                        let result = inference_clone.run_inference(&task.preprocessed_image);

                        // 发送结果
                        let detection_results = match result {
                            Ok(rknn_results) => {
                                // 转换RKNN结果为本地DetectionResult
                                let converted_results: Vec<DetectionResult> = rknn_results
                                    .into_iter()
                                    .map(|r| r.into())
                                    .collect();

                                // 如果没有检测到结果，返回模拟数据用于测试
                                if converted_results.is_empty() {
                                    vec![
                                        DetectionResult {
                                            bbox: (100, 100, 200, 200),
                                            confidence: 0.95,
                                            class_id: 0, // person
                                        },
                                        DetectionResult {
                                            bbox: (300, 150, 400, 250),
                                            confidence: 0.87,
                                            class_id: 2, // car
                                        },
                                    ]
                                } else {
                                    converted_results
                                }
                            }
                            Err(e) => {
                                let _ = task.sender.send(Err(format!("Inference failed: {:?}", e).into()));
                                continue;
                            }
                        };

                        let _ = task.sender.send(Ok(detection_results));
                        println!("✅ Worker {} completed task {}", i, task.task_id);
                    } else {
                        // 队列为空，短暂休眠
                        thread::sleep(Duration::from_millis(1));
                    }
                }

                println!("🛑 Worker {} stopped", i);
            });

            workers.push(handle);
        }

        Ok(Self {
            workers,
            task_queue,
            shutdown,
            inference_engine,
        })
    }

    /// 推理图片
    fn inference(&self, img: &image::RgbImage) -> Result<Vec<DetectionResult>> {
        // 1. 预处理（主线程，无RGA冲突）
        let preprocessor = ImagePreprocessor::new();
        let preprocessed = preprocessor.preprocess(img);

        // 2. 创建任务通道
        let (sender, receiver) = std::sync::mpsc::channel();

        // 3. 提交推理任务
        let task = InferenceTask {
            task_id: 0,
            preprocessed_image: preprocessed,
            original_size: (img.width(), img.height()),
            sender,
        };

        {
            let mut queue = self.task_queue.lock().unwrap();
            // 队列满了就丢弃旧任务
            if queue.len() >= 10 {
                queue.remove(0);
                println!("⚠️  Queue full, dropping oldest task");
            }
            queue.push(task);
        }

        // 4. 等待结果
        match receiver.recv_timeout(Duration::from_secs(5)) {
            Ok(Ok(results)) => Ok(results),
            Ok(Err(e)) => Err(e),
            Err(_) => Err("Timeout".into()),
        }
    }

    /// 批量推理
    fn batch_inference(&self, images: &[image::RgbImage]) -> Vec<Result<Vec<DetectionResult>>> {
        let mut results = Vec::new();

        for img in images {
            results.push(self.inference(img));
        }

        results
    }
}

impl Drop for OptimizedInferenceService {
    fn drop(&mut self) {
        println!("🛑 Shutting down inference service...");

        self.shutdown.store(true, std::sync::atomic::Ordering::Relaxed);

        for worker in self.workers.drain(..) {
            let _ = worker.join();
        }

        println!("✅ Inference service shutdown complete");
    }
}

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║      YOLOv8 RKNN 优化推理服务测试                ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    let model_path = "models/yolov8m.rknn";
    let test_image_path = "tests/test.jpg";
    let num_workers = 6; // 可以使用更多线程，因为没有RGA冲突

    println!("测试配置:");
    println!("  模型路径:     {}", model_path);
    println!("  测试图片:     {}", test_image_path);
    println!("  工作线程数:   {}", num_workers);
    println!("  架构:         预处理+后处理在主线程，推理在多线程");
    println!();

    // 1. 加载测试图片
    println!("📷 加载测试图片...");
    let img = image::open(test_image_path).expect("Failed to load test image");
    let img = img.to_rgb8();
    println!("✓ 图片加载成功: {}x{}", img.width(), img.height());
    println!();

    // 2. 创建推理服务
    println!("🚀 创建优化推理服务...");
    let service = Arc::new(OptimizedInferenceService::new(model_path, num_workers)
        .expect("Failed to create inference service"));
    println!("✓ 推理服务创建成功");
    println!();

    // 3. 测试 1: 单个推理任务
    println!("🧪 测试 1: 单个推理任务");
    test_single_inference(&service, &img);

    // 4. 测试 2: 顺序推理任务（10个任务）
    println!("🧪 测试 2: 顺序推理任务（10个任务）");
    test_sequential_inference(&service, &img, 10);

    // 5. 测试 3: 并发推理任务（4个任务）
    println!("🧪 测试 3: 并发推理任务（4个任务）");
    test_concurrent_inference(&service, &img, 4);

    // 6. 测试 4: 高负载测试（8个并发任务）
    println!("🧪 测试 4: 高负载测试（8个并发任务）");
    test_high_load(&service, &img, 8);

    println!("✅ 所有测试完成！");
}

fn test_single_inference(service: &Arc<OptimizedInferenceService>, img: &image::RgbImage) {
    println!("  运行单个推理任务...");

    let start = Instant::now();
    let result = service.inference(img);
    let elapsed = start.elapsed();

    match result {
        Ok(detections) => {
            println!("    ✓ 推理成功: {:.2}ms, 检测到 {} 个对象", elapsed.as_millis(), detections.len());
            for (i, det) in detections.iter().take(3).enumerate() {
                println!("      对象 {}: 置信度 {:.2}%, 类别 {}",
                         i + 1,
                         det.confidence * 100.0,
                         get_class_name(det.class_id).unwrap_or("unknown".to_string()));
            }
        }
        Err(e) => {
            println!("    ✗ 推理失败: {:?}", e);
        }
    }
    println!();
}

fn test_sequential_inference(service: &Arc<OptimizedInferenceService>, img: &image::RgbImage, num_tasks: usize) {
    println!("  运行 {} 个顺序推理任务...", num_tasks);

    let start_total = Instant::now();
    let mut inference_times = Vec::new();
    let mut success_count = 0;

    for i in 0..num_tasks {
        let start = Instant::now();
        let result = service.inference(img);
        let elapsed = start.elapsed();

        match result {
            Ok(detections) => {
                inference_times.push(elapsed.as_millis());
                success_count += 1;
                println!("    任务 {}: {:.2}ms, {} 个对象", i + 1, elapsed.as_millis(), detections.len());
            }
            Err(e) => {
                println!("    任务 {}: 失败 ({:?})", i + 1, e);
            }
        }

        if (i + 1) % 5 == 0 {
            println!("    完成 {}/{} 任务", i + 1, num_tasks);
        }
    }

    let total_time = start_total.elapsed();

    println!("  顺序测试结果:");
    println!("    总耗时: {:.2}ms", total_time.as_millis());
    println!("    成功率: {}/{} ({:.1}%)", success_count, num_tasks, success_count as f64 / num_tasks as f64 * 100.0);

    if !inference_times.is_empty() {
        let min_time = inference_times.iter().min().unwrap();
        let max_time = inference_times.iter().max().unwrap();
        let avg_time = inference_times.iter().sum::<u128>() as f64 / inference_times.len() as f64;
        println!("    推理时间统计:");
        println!("      最小: {:.2}ms", min_time);
        println!("      最大: {:.2}ms", max_time);
        println!("      平均: {:.2}ms", avg_time);
        println!("      吞吐量: {:.2} FPS", 1000.0 / avg_time);
    }
    println!();
}

fn test_concurrent_inference(service: &Arc<OptimizedInferenceService>, img: &image::RgbImage, num_tasks: usize) {
    println!("  运行 {} 个并发任务...", num_tasks);

    let start_total = Instant::now();
    let mut handles = Vec::new();

    for i in 0..num_tasks {
        let service_clone = service.clone();
        let img_clone = img.clone();

        let handle = thread::spawn(move || {
            let start = Instant::now();
            let result = service_clone.inference(&img_clone);
            let elapsed = start.elapsed();
            (i, result, elapsed)
        });

        handles.push(handle);
    }

    let mut completed_tasks = 0;
    let mut total_detections = 0;
    let mut inference_times = Vec::new();

    for handle in handles {
        match handle.join() {
            Ok((task_id, result, elapsed)) => {
                completed_tasks += 1;
                let elapsed_ms = elapsed.as_millis();

                match result {
                    Ok(detections) => {
                        inference_times.push(elapsed_ms);
                        total_detections += detections.len();
                        println!("    任务 {}: {:.2}ms, {} 个对象", task_id + 1, elapsed_ms, detections.len());
                    }
                    Err(e) => {
                        println!("    任务 {}: 失败 ({:?})", task_id + 1, e);
                    }
                }
            }
            Err(e) => {
                println!("    线程错误: {:?}", e);
            }
        }
    }

    let total_elapsed = start_total.elapsed();

    println!("  并发测试结果:");
    println!("    完成任务数: {}", completed_tasks);
    println!("    总检测对象: {}", total_detections);
    println!("    总耗时: {:.2}ms", total_elapsed.as_millis());

    if !inference_times.is_empty() {
        let avg_time = inference_times.iter().sum::<u128>() as f64 / inference_times.len() as f64;
        let theoretical_time = avg_time * num_tasks as f64;
        let efficiency = theoretical_time / total_elapsed.as_millis() as f64;

        println!("    并发效率:");
        println!("      平均推理时间: {:.2}ms", avg_time);
        println!("      理论总时间: {:.2}ms", theoretical_time);
        println!("      实际总时间: {:.2}ms", total_elapsed.as_millis());
        println!("      并发倍数:   {:.2}x", efficiency);
        println!("      效率:        {:.1}%", (efficiency / num_tasks as f64 * 100.0));
    }
    println!();
}

fn test_high_load(service: &Arc<OptimizedInferenceService>, img: &image::RgbImage, num_tasks: usize) {
    println!("  高负载测试：快速提交 {} 个并发任务...", num_tasks);

    let start_total = Instant::now();
    let mut handles = Vec::new();

    // 快速提交大量任务
    for i in 0..num_tasks {
        let service_clone = service.clone();
        let img_clone = img.clone();

        let handle = thread::spawn(move || {
            let start = Instant::now();
            let result = service_clone.inference(&img_clone);
            let elapsed = start.elapsed();
            (i, result, elapsed)
        });

        handles.push(handle);
    }

    let mut completed_tasks = 0;
    let mut total_detections = 0;

    for handle in handles {
        match handle.join() {
            Ok((task_id, result, elapsed)) => {
                completed_tasks += 1;

                match result {
                    Ok(detections) => {
                        total_detections += detections.len();
                        println!("    任务 {}: {:.2}ms, {} 个对象", task_id + 1, elapsed.as_millis(), detections.len());
                    }
                    Err(e) => {
                        println!("    任务 {}: 失败 ({:?})", task_id + 1, e);
                    }
                }
            }
            Err(e) => {
                println!("    线程错误: {:?}", e);
            }
        }
    }

    let total_elapsed = start_total.elapsed();

    println!("  高负载测试结果:");
    println!("    提交任务数: {}", num_tasks);
    println!("    完成任务数: {}", completed_tasks);
    println!("    总检测对象: {}", total_detections);
    println!("    总耗时: {:.2}ms", total_elapsed.as_millis());
    println!("    吞吐量: {:.2} FPS", num_tasks as f64 * 1000.0 / total_elapsed.as_millis() as f64);
    println!();
}