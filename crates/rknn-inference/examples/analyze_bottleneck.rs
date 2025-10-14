use rknn_inference::{ImageBuffer, ImageFormat};
use std::thread;
use std::time::{Duration, Instant};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error + Send + Sync>>;

fn test_bottleneck() -> Result<()> {
    println!("🔍 分析性能瓶颈");
    println!("测试100张图片，分析每个环节的耗时");
    println!("{}", "─".repeat(60));

    let model_path = "models/yolov8m.rknn";
    let img = image::open("tests/test.jpg")?.to_rgb8();

    // 1. 测试预处理时间
    println!("1. 测试预处理性能（RGA操作）");
    let preprocessor = ImagePreprocessor::new();
    let mut preprocess_times = Vec::new();

    for i in 0..100 {
        let start = Instant::now();
        let _preprocessed = preprocessor.preprocess(&img);
        let elapsed = start.elapsed();
        preprocess_times.push(elapsed.as_millis());

        if (i + 1) % 20 == 0 {
            print!("  预处理进度: {}/100\r", i + 1);
            std::io::Write::flush(&mut std::io::stdout()).unwrap();
        }
    }

    let avg_preprocess = preprocess_times.iter().sum::<u128>() as f64 / preprocess_times.len() as f64;
    let min_preprocess = preprocess_times.iter().min().unwrap();
    let max_preprocess = preprocess_times.iter().max().unwrap();

    println!("\n  预处理统计:");
    println!("    平均时间: {:.2}ms", avg_preprocess);
    println!("    最小时间: {}ms", min_preprocess);
    println!("    最大时间: {}ms", max_preprocess);
    println!();

    // 2. 测试单detector推理时间（无锁竞争）
    println!("2. 测试单Detector推理性能");
    let mut detector = rknn_inference::Yolov8Detector::new(model_path)?;
    let preprocessed = preprocessor.preprocess(&img);
    let mut single_inference_times = Vec::new();

    for i in 0..100 {
        let start = Instant::now();
        let _result = detector.detect(&preprocessed);
        let elapsed = start.elapsed();
        single_inference_times.push(elapsed.as_millis());

        if (i + 1) % 20 == 0 {
            print!("  推理进度: {}/100\r", i + 1);
            std::io::Write::flush(&mut std::io::stdout()).unwrap();
        }
    }

    let avg_single = single_inference_times.iter().sum::<u128>() as f64 / single_inference_times.len() as f64;
    let min_single = single_inference_times.iter().min().unwrap();
    let max_single = single_inference_times.iter().max().unwrap();

    println!("\n  单Detector推理统计:");
    println!("    平均时间: {:.2}ms", avg_single);
    println!("    最小时间: {}ms", min_single);
    println!("    最大时间: {}ms", max_single);
    println!();

    // 3. 测试多detector并发推理（分析NPU瓶颈）
    println!("3. 测试多Detector并发推理性能");

    // 测试不同数量的并发detector
    for num_detectors in [2, 3, 4, 6] {
        println!("  测试 {} 个并发Detector:", num_detectors);

        let start = Instant::now();
        let mut handles = Vec::new();
        let mut inference_times: Vec<u128> = Vec::new();

        for i in 0..num_detectors {
            let mut detector = rknn_inference::Yolov8Detector::new(model_path)?;
            let preprocessed_clone = preprocessed.clone();

            let handle = thread::spawn(move || {
                let thread_start = Instant::now();

                // 每个线程运行 100/num_detectors 次推理
                let iterations = 100 / num_detectors;
                let mut times = Vec::new();

                for _ in 0..iterations {
                    let inference_start = Instant::now();
                    let _result = detector.detect(&preprocessed_clone);
                    let inference_time = inference_start.elapsed();
                    times.push(inference_time.as_millis());
                }

                (i, times, thread_start.elapsed())
            });

            handles.push(handle);
        }

        let mut total_inference_time = Duration::new(0, 0);
        let mut all_times = Vec::new();

        for handle in handles {
            let (_thread_id, times, thread_time) = handle.join().unwrap();
            all_times.extend(times);
            total_inference_time += thread_time;
        }

        let overall_time = start.elapsed();
        let avg_inference = all_times.iter().sum::<u128>() as f64 / all_times.len() as f64;
        let min_inference = all_times.iter().min().unwrap();
        let max_inference = all_times.iter().max().unwrap();

        println!("    总耗时: {:.2}ms", overall_time.as_millis());
        println!("    平均推理: {:.2}ms", avg_inference);
        println!("    最快推理: {}ms", min_inference);
        println!("    最慢推理: {}ms", max_inference);
        println!("    理论总时间: {:.2}ms", avg_inference * 100.0);
        println!("    实际效率: {:.1}%", (avg_inference * 100.0 / overall_time.as_millis() as f64) * 100.0);

        // 计算FPS
        let fps = 100.0 / overall_time.as_secs_f64();
        println!("    实际FPS: {:.2}", fps);
        println!();
    }

    // 4. 分析瓶颈
    println!("4. 瓶颈分析");
    println!("  预处理平均时间: {:.2}ms", avg_preprocess);
    println!("  推理平均时间: {:.2}ms", avg_single);
    println!("  总单次处理时间: {:.2}ms", avg_preprocess + avg_single);
    println!("  理论最大FPS: {:.2}", 1000.0 / (avg_preprocess + avg_single));
    println!();

    // 5. 给出优化建议
    let preprocessing_ratio = avg_preprocess / (avg_preprocess + avg_single);
    let inference_ratio = avg_single / (avg_preprocess + avg_single);

    println!("5. 性能优化建议");
    println!("  预处理占比: {:.1}%", preprocessing_ratio * 100.0);
    println!("  推理占比: {:.1}%", inference_ratio * 100.0);

    if preprocessing_ratio > 0.2 {
        println!("  ⚠️  预处理时间较长，考虑优化RGA操作");
    }

    if inference_ratio > 0.7 {
        println!("  ⚠️  推理是主要瓶颈，NPU已经接近极限");
    }

    if avg_single < 50.0 {
        println!("  ✅ 推理性能很好，可以增加并发数");
    } else if avg_single < 100.0 {
        println!("  ✅ 推理性能良好，建议2-3个并发");
    } else {
        println!("  ❌ 推理性能较差，建议检查模型和NPU配置");
    }

    Ok(())
}

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
    println!("║          RKNN性能瓶颈深度分析                    ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    test_bottleneck()?;

    Ok(())
}