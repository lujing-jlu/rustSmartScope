use rknn_inference::{ImageBuffer, ImageFormat};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error + Send + Sync>>;

/// 诊断版本：测试1个detector vs 多个detector的性能差异

fn test_single_detector_performance() -> Result<()> {
    println!("🧪 测试1: 单个Detector + 6线程 (当前架构)");

    let model_path = "models/yolov8m.rknn";
    let detector = rknn_inference::Yolov8Detector::new(model_path)?;
    let shared_detector = Arc::new(Mutex::new(detector));

    // 创建测试图片
    let img = image::open("tests/test.jpg")?.to_rgb8();
    let preprocessor = ImagePreprocessor::new();
    let preprocessed = preprocessor.preprocess(&img);

    let start = Instant::now();
    let mut handles = Vec::new();

    // 6个线程并发推理
    for i in 0..6 {
        let detector_clone = shared_detector.clone();
        let img_clone = preprocessed.clone();

        let handle = thread::spawn(move || {
            let thread_start = Instant::now();

            // 测量获取锁的时间
            let lock_start = Instant::now();
            let mut detector = detector_clone.lock().unwrap();
            let lock_time = lock_start.elapsed();

            // 测量推理时间
            let inference_start = Instant::now();
            let _result = detector.detect(&img_clone);
            let inference_time = inference_start.elapsed();

            let total_time = thread_start.elapsed();

            (i, lock_time, inference_time, total_time)
        });

        handles.push(handle);
    }

    let mut total_lock_time = Duration::new(0, 0);
    let mut total_inference_time = Duration::new(0, 0);
    let mut total_time = Duration::new(0, 0);

    for handle in handles {
        let (thread_id, lock_time, inference_time, thread_total) = handle.join().unwrap();
        println!("  线程 {}: 锁等待={:.2}ms, 推理={:.2}ms, 总计={:.2}ms",
                thread_id,
                lock_time.as_millis(),
                inference_time.as_millis(),
                thread_total.as_millis());

        total_lock_time += lock_time;
        total_inference_time += inference_time;
        total_time += thread_total;
    }

    let overall_time = start.elapsed();
    println!("  结果分析:");
    println!("    平均锁等待: {:.2}ms", total_lock_time.as_millis() / 6);
    println!("    平均推理时间: {:.2}ms", total_inference_time.as_millis() / 6);
    println!("    平均线程时间: {:.2}ms", total_time.as_millis() / 6);
    println!("    实际总耗时: {:.2}ms", overall_time.as_millis());
    println!("    并发效率: {:.1}%", (total_time.as_millis() as f64 / 6.0 / overall_time.as_millis() as f64) * 100.0);
    println!();

    Ok(())
}

fn test_multiple_detectors_performance() -> Result<()> {
    println!("🧪 测试2: 多个独立Detector (优化架构)");

    let model_path = "models/yolov8m.rknn";

    // 创建6个独立的detector
    let mut detectors = Vec::new();
    for i in 0..6 {
        println!("  创建Detector {}...", i);
        let detector = rknn_inference::Yolov8Detector::new(model_path)?;
        detectors.push(detector);
    }

    // 创建测试图片
    let img = image::open("tests/test.jpg")?.to_rgb8();
    let preprocessor = ImagePreprocessor::new();
    let preprocessed = preprocessor.preprocess(&img);

    let start = Instant::now();
    let mut handles = Vec::new();

    // 6个线程各自使用独立的detector
    for i in 0..6 {
        let mut detector = detectors.remove(0); // 取出一个detector
        let img_clone = preprocessed.clone();

        let handle = thread::spawn(move || {
            let thread_start = Instant::now();

            // 直接推理，无需等待锁
            let inference_start = Instant::now();
            let _result = detector.detect(&img_clone);
            let inference_time = inference_start.elapsed();

            let total_time = thread_start.elapsed();

            (i, inference_time, total_time)
        });

        handles.push(handle);
    }

    let mut total_inference_time = Duration::new(0, 0);
    let mut total_time = Duration::new(0, 0);

    for handle in handles {
        let (thread_id, inference_time, thread_total) = handle.join().unwrap();
        println!("  线程 {}: 推理={:.2}ms, 总计={:.2}ms",
                thread_id,
                inference_time.as_millis(),
                thread_total.as_millis());

        total_inference_time += inference_time;
        total_time += thread_total;
    }

    let overall_time = start.elapsed();
    println!("  结果分析:");
    println!("    平均推理时间: {:.2}ms", total_inference_time.as_millis() / 6);
    println!("    平均线程时间: {:.2}ms", total_time.as_millis() / 6);
    println!("    实际总耗时: {:.2}ms", overall_time.as_millis());
    println!("    并发效率: {:.1}%", (total_time.as_millis() as f64 / 6.0 / overall_time.as_millis() as f64) * 100.0);
    println!();

    Ok(())
}

fn test_npu_concurrency_limit() -> Result<()> {
    println!("🧪 测试3: NPU并发限制测试");

    let model_path = "models/yolov8m.rknn";
    let img = image::open("tests/test.jpg")?.to_rgb8();
    let preprocessor = ImagePreprocessor::new();
    let preprocessed = preprocessor.preprocess(&img);

    // 测试1-6个detector的同时推理
    for num_detectors in 1..=6 {
        println!("  测试 {} 个并发Detector:", num_detectors);

        let start = Instant::now();
        let mut handles = Vec::new();

        for i in 0..num_detectors {
            let mut detector = rknn_inference::Yolov8Detector::new(model_path)?;
            let img_clone = preprocessed.clone();

            let handle = thread::spawn(move || {
                let inference_start = Instant::now();
                let _result = detector.detect(&img_clone);
                let inference_time = inference_start.elapsed();

                (i, inference_time)
            });

            handles.push(handle);
        }

        let mut total_inference_time = Duration::new(0, 0);
        let mut min_time = Duration::new(u64::MAX, 0);
        let mut max_time = Duration::new(0, 0);

        for handle in handles {
            let (_thread_id, inference_time) = handle.join().unwrap();
            total_inference_time += inference_time;
            min_time = min_time.min(inference_time);
            max_time = max_time.max(inference_time);
        }

        let overall_time = start.elapsed();
        let avg_time = total_inference_time.as_millis() / num_detectors;
        let theoretical_time = max_time;

        println!("    总耗时: {:.2}ms", overall_time.as_millis());
        println!("    平均推理: {:.2}ms", avg_time);
        println!("    最快推理: {:.2}ms", min_time.as_millis());
        println!("    最慢推理: {:.2}ms", max_time.as_millis());
        println!("    理论时间: {:.2}ms", theoretical_time.as_millis());
        println!("    并发效率: {:.1}%", (theoretical_time.as_millis() as f64 / overall_time.as_millis() as f64) * 100.0);
        println!();
    }

    Ok(())
}

/// 图片预处理工具（简化版）
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

fn main() -> Result<()> {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║        RKNN多线程性能诊断测试                      ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    // 测试1: 当前架构的问题
    test_single_detector_performance()?;

    // 测试2: 优化架构
    test_multiple_detectors_performance()?;

    // 测试3: NPU并发限制
    test_npu_concurrency_limit()?;

    println!("🎯 诊断结论:");
    println!("1. 如果测试1的锁等待时间很长，说明Mutex是瓶颈");
    println!("2. 如果测试2的效率显著高于测试1，说明需要多个detector");
    println!("3. 如果测试3显示超过某个数量后效率下降，说明NPU有并发限制");

    Ok(())
}