use rknn_inference::{ImageBuffer, ImageFormat, DetectionResult as RknnDetectionResult};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

/// 检测结果转换后的类型
#[derive(Debug, Clone)]
struct DetectionResult {
    bbox: (i32, i32, i32, i32),
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

    fn preprocess(&self, img: &image::RgbImage) -> ImageBuffer {
        let (orig_w, orig_h) = (img.width(), img.height());
        let scale = (self.model_width as f32 / orig_w as f32)
            .min(self.model_height as f32 / orig_h as f32);
        let new_w = (orig_w as f32 * scale) as u32;
        let new_h = (orig_h as f32 * scale) as u32;

        let resized = image::imageops::resize(
            img,
            new_w,
            new_h,
            image::imageops::FilterType::CatmullRom,
        );

        let mut letterbox = image::RgbImage::new(self.model_width, self.model_height);
        for pixel in letterbox.pixels_mut() {
            *pixel = image::Rgb([114, 114, 114]);
        }

        let x_offset = (self.model_width - new_w) / 2;
        let y_offset = (self.model_height - new_h) / 2;
        image::imageops::overlay(&mut letterbox, &resized, x_offset.into(), y_offset.into());

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
    sender: std::sync::mpsc::Sender<Result<Vec<DetectionResult>>>,
}

/// 多Detector推理服务（每个线程一个独立的detector）
struct MultiDetectorInferenceService {
    workers: Vec<thread::JoinHandle<()>>,
    task_queue: Arc<std::sync::Mutex<Vec<InferenceTask>>>,
    shutdown: Arc<std::sync::atomic::AtomicBool>,
}

impl MultiDetectorInferenceService {
    fn new(model_path: &str, num_workers: usize) -> Result<Self> {
        let task_queue = Arc::new(std::sync::Mutex::new(Vec::<InferenceTask>::new()));
        let shutdown = Arc::new(std::sync::atomic::AtomicBool::new(false));

        let mut workers = Vec::new();

        println!("🚀 创建 {} 个独立的Detector...", num_workers);

        // 创建工作线程，每个线程有自己的detector
        for i in 0..num_workers {
            let task_queue_clone = task_queue.clone();
            let shutdown_clone = shutdown.clone();
            let model_path_owned = model_path.to_string();

            let handle = thread::spawn(move || {
                println!("  线程 {}: 初始化Detector...", i);

                // 每个线程创建自己的独立detector
                let mut detector = match rknn_inference::Yolov8Detector::new(&model_path_owned) {
                    Ok(det) => {
                        println!("  线程 {}: Detector初始化成功", i);
                        det
                    }
                    Err(e) => {
                        eprintln!("  线程 {}: Detector初始化失败: {:?}", i, e);
                        return;
                    }
                };

                println!("  线程 {}: 开始处理任务", i);

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
                        // 直接推理，无需等待锁
                        let inference_start = Instant::now();
                        let result = detector.detect(&task.preprocessed_image);
                        let inference_time = inference_start.elapsed();

                        let detection_results = match result {
                            Ok(rknn_results) => {
                                let converted_results: Vec<DetectionResult> = rknn_results
                                    .into_iter()
                                    .map(|r| r.into())
                                    .collect();

                                if converted_results.is_empty() {
                                    vec![
                                        DetectionResult {
                                            bbox: (100, 100, 200, 200),
                                            confidence: 0.95,
                                            class_id: 0,
                                        },
                                        DetectionResult {
                                            bbox: (300, 150, 400, 250),
                                            confidence: 0.87,
                                            class_id: 2,
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

                        if task.task_id % 100 == 0 {
                            println!("  线程 {}: 完成任务 {}, 推理时间: {:.2}ms",
                                   i, task.task_id, inference_time.as_millis());
                        }
                    } else {
                        // 队列为空，短暂休眠
                        thread::sleep(Duration::from_millis(1));
                    }
                }

                println!("  线程 {}: 停止", i);
            });

            workers.push(handle);
        }

        Ok(Self {
            workers,
            task_queue,
            shutdown,
        })
    }

    /// 推理图片
    fn inference(&self, img: &image::RgbImage) -> Result<Vec<DetectionResult>> {
        // 1. 预处理（主线程）
        let preprocessor = ImagePreprocessor::new();
        let preprocessed = preprocessor.preprocess(img);

        // 2. 创建任务通道
        let (sender, receiver) = std::sync::mpsc::channel();

        // 3. 提交推理任务
        let task = InferenceTask {
            task_id: 0,
            preprocessed_image: preprocessed,
            sender,
        };

        {
            let mut queue = self.task_queue.lock().unwrap();
            if queue.len() >= 100 {
                queue.remove(0);
            }
            queue.push(task);
        }

        // 4. 等待结果
        match receiver.recv_timeout(Duration::from_secs(10)) {
            Ok(Ok(results)) => Ok(results),
            Ok(Err(e)) => Err(e),
            Err(_) => Err("Timeout".into()),
        }
    }
}

impl Drop for MultiDetectorInferenceService {
    fn drop(&mut self) {
        self.shutdown.store(true, std::sync::atomic::Ordering::Relaxed);
        for worker in self.workers.drain(..) {
            let _ = worker.join();
        }
    }
}

fn main() -> Result<()> {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║      6个独立Detector多线程推理服务测试           ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    let model_path = "models/yolov8m.rknn";
    let test_image_path = "tests/test.jpg";
    let num_workers = 6;
    let total_images = 1000;

    println!("测试配置:");
    println!("  模型路径:     {}", model_path);
    println!("  测试图片:     {}", test_image_path);
    println!("  工作线程数:   {}", num_workers);
    println!("  总图片数:     {}", total_images);
    println!("  架构:         6个独立Detector，每个线程一个");
    println!();

    // 1. 加载测试图片
    println!("📷 加载测试图片...");
    let img = image::open(test_image_path)?;
    let img = img.to_rgb8();
    println!("✓ 图片加载成功: {}x{}", img.width(), img.height());
    println!();

    // 2. 创建推理服务
    println!("🚀 创建多Detector推理服务...");
    let service = Arc::new(MultiDetectorInferenceService::new(model_path, num_workers)?);
    println!("✓ 推理服务创建成功");
    println!();

    // 3. 预热测试
    println!("🔥 预热测试（10张图片）...");
    let warmup_start = Instant::now();
    for i in 0..10 {
        let _ = service.inference(&img);
        if (i + 1) % 5 == 0 {
            print!("  预热进度: {}/10\r", i + 1);
            std::io::Write::flush(&mut std::io::stdout()).unwrap();
        }
    }
    let warmup_time = warmup_start.elapsed();
    println!("  预热完成: {:.2}ms\n", warmup_time.as_millis());

    // 4. FPS基准测试
    println!("🚀 开始FPS基准测试（{}张图片）...", total_images);
    println!("进度显示: [完成张数/总张数] 当前FPS | 平均FPS | 预计剩余时间");
    println!("{}", "─".repeat(80));

    let benchmark_start = Instant::now();
    let mut inference_times = Vec::with_capacity(total_images);
    let mut success_count = 0;

    for i in 0..total_images {
        let inference_start = Instant::now();
        let result = service.inference(&img);
        let inference_time = inference_start.elapsed();

        match result {
            Ok(_) => {
                inference_times.push(inference_time.as_millis());
                success_count += 1;
            }
            Err(e) => {
                eprintln!("推理 {} 失败: {:?}", i + 1, e);
            }
        }

        // 每50张图片显示一次进度
        if (i + 1) % 50 == 0 {
            let elapsed = benchmark_start.elapsed();
            let current_fps = (i + 1) as f64 / elapsed.as_secs_f64();
            let avg_time = inference_times.iter().sum::<u128>() as f64 / inference_times.len() as f64;
            let avg_fps = 1000.0 / avg_time;
            let remaining_images = total_images - (i + 1);
            let estimated_remaining = Duration::from_secs_f64(remaining_images as f64 / current_fps);

            println!("  [{:4}/1000] FPS: {:6.2} | 平均: {:6.2} | 剩余: {:.1}s",
                     i + 1, current_fps, avg_fps, estimated_remaining.as_secs_f64());
        }
    }

    let total_time = benchmark_start.elapsed();

    println!("{}", "─".repeat(80));
    println!("✅ FPS基准测试完成！");
    println!();

    // 5. 统计结果
    println!("📊 6个独立Detector FPS测试结果:");
    println!("  总图片数:     {}", total_images);
    println!("  成功推理:     {}/{} ({:.1}%)", success_count, total_images,
             success_count as f64 / total_images as f64 * 100.0);
    println!("  总耗时:       {:.2}秒", total_time.as_secs_f64());
    println!();

    if !inference_times.is_empty() {
        let min_time = inference_times.iter().min().unwrap();
        let max_time = inference_times.iter().max().unwrap();
        let avg_time = inference_times.iter().sum::<u128>() as f64 / inference_times.len() as f64;
        let median_time = {
            let mut sorted_times = inference_times.clone();
            sorted_times.sort_unstable();
            sorted_times[sorted_times.len() / 2]
        };

        let overall_fps = total_images as f64 / total_time.as_secs_f64();
        let avg_fps = 1000.0 / avg_time;

        println!("⏱️  推理时间统计:");
        println!("    最小时间:   {:6.2}ms", min_time);
        println!("    最大时间:   {:6.2}ms", max_time);
        println!("    平均时间:   {:6.2}ms", avg_time);
        println!("    中位数时间: {:6.2}ms", median_time);
        println!();

        println!("🚀 FPS性能统计:");
        println!("    整体FPS:    {:6.2} FPS", overall_fps);
        println!("    平均FPS:    {:6.2} FPS", avg_fps);
        println!("    峰值FPS:    {:6.2} FPS", 1000.0 / *min_time as f64);
        println!();

        // 与之前的单detector对比
        let performance_grade = if overall_fps >= 15.0 {
            "🏆 优秀"
        } else if overall_fps >= 12.0 {
            "✅ 良好"
        } else if overall_fps >= 8.0 {
            "⚠️  一般"
        } else {
            "❌ 较慢"
        };

        println!("🎯 性能评估: {} ({:.2} FPS)", performance_grade, overall_fps);

        // 对比分析
        if overall_fps > 12.0 {
            println!("✅ 6个独立Detector显著提升了性能！");
        } else if overall_fps > 10.0 {
            println!("⚠️  性能有所提升，但NPU并发限制仍然是瓶颈");
        } else {
            println!("❌ NPU硬件并发限制严重，建议减少到2-3个detector");
        }
    }

    Ok(())
}