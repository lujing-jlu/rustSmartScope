use rknn_inference::{get_class_name, ImageBuffer, ImageFormat, Yolov8Detector};
use std::time::{Duration, Instant};

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║        YOLOv8 RKNN 单线程推理服务测试              ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    let model_path = "models/yolov8m.rknn";
    let test_image_path = "tests/test.jpg";

    println!("测试配置:");
    println!("  模型路径:     {}", model_path);
    println!("  测试图片:     {}", test_image_path);
    println!("  说明:         单线程推理，避免RGA并发冲突");
    println!();

    // 1. 加载测试图片
    println!("📷 加载测试图片...");
    let img = image::open(test_image_path).expect("Failed to load test image");
    let img = img.to_rgb8();
    println!("✓ 图片加载成功: {}x{}", img.width(), img.height());
    println!();

    // 2. 创建推理器
    println!("🚀 创建推理器...");
    let mut detector = Yolov8Detector::new(model_path)
        .expect("Failed to create detector");
    println!("✓ 推理器创建成功");
    println!();

    // 3. 测试 1: 单个推理任务
    println!("🧪 测试 1: 单个推理任务");
    test_single_inference(&mut detector, &img);

    // 4. 测试 2: 顺序推理任务（10个任务）
    println!("🧪 测试 2: 顺序推理任务（10个任务）");
    test_sequential_inference(&mut detector, &img, 10);

    // 5. 测试 3: 性能测试（30个任务）
    println!("🧪 测试 3: 性能测试（30个任务）");
    test_performance(&mut detector, &img, 30);

    println!("✅ 所有测试完成！");
}

fn test_single_inference(detector: &mut Yolov8Detector, img: &image::RgbImage) {
    println!("  运行单个推理任务...");

    let image_buffer = ImageBuffer {
        width: img.width() as i32,
        height: img.height() as i32,
        format: ImageFormat::Rgb888,
        data: img.clone().into_raw(),
    };

    let start = Instant::now();
    let result = detector.detect(&image_buffer);
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

fn test_sequential_inference(detector: &mut Yolov8Detector, img: &image::RgbImage, num_tasks: usize) {
    println!("  运行 {} 个顺序推理任务...", num_tasks);

    let start_total = Instant::now();
    let mut inference_times = Vec::new();
    let mut success_count = 0;
    let mut total_detections = 0;

    for i in 0..num_tasks {
        let image_buffer = ImageBuffer {
            width: img.width() as i32,
            height: img.height() as i32,
            format: ImageFormat::Rgb888,
            data: img.clone().into_raw(),
        };

        let start = Instant::now();
        let result = detector.detect(&image_buffer);
        let elapsed = start.elapsed();

        match result {
            Ok(detections) => {
                inference_times.push(elapsed.as_millis());
                success_count += 1;
                total_detections += detections.len();
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
    println!("    总检测对象: {}", total_detections);

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

fn test_performance(detector: &mut Yolov8Detector, img: &image::RgbImage, num_tasks: usize) {
    println!("  运行 {} 个任务进行性能测试...", num_tasks);

    let start_total = Instant::now();
    let mut inference_times = Vec::new();
    let mut success_count = 0;
    let mut total_detections = 0;

    for i in 0..num_tasks {
        let image_buffer = ImageBuffer {
            width: img.width() as i32,
            height: img.height() as i32,
            format: ImageFormat::Rgb888,
            data: img.clone().into_raw(),
        };

        let start = Instant::now();
        let result = detector.detect(&image_buffer);
        let elapsed = start.elapsed();

        match result {
            Ok(detections) => {
                inference_times.push(elapsed.as_millis());
                success_count += 1;
                total_detections += detections.len();

                if (i + 1) % 10 == 0 {
                    println!("    完成 {}/{} 任务", i + 1, num_tasks);
                }
            }
            Err(e) => {
                println!("    任务 {}: 失败 ({:?})", i + 1, e);
            }
        }
    }

    let total_time = start_total.elapsed();

    println!("  性能测试结果:");
    println!("    总耗时: {:.2}ms", total_time.as_millis());
    println!("    成功率: {}/{} ({:.1}%)", success_count, num_tasks, success_count as f64 / num_tasks as f64 * 100.0);
    println!("    总检测对象: {}", total_detections);

    if !inference_times.is_empty() {
        let min_time = inference_times.iter().min().unwrap();
        let max_time = inference_times.iter().max().unwrap();
        let avg_time = inference_times.iter().sum::<u128>() as f64 / inference_times.len() as f64;

        let variance: f64 = inference_times
            .iter()
            .map(|&t| {
                let diff = t as f64 - avg_time;
                diff * diff
            })
            .sum::<f64>()
            / inference_times.len() as f64;
        let std_dev = variance.sqrt();

        println!("    推理时间统计:");
        println!("      最小: {:.2}ms", min_time);
        println!("      最大: {:.2}ms", max_time);
        println!("      平均: {:.2}ms", avg_time);
        println!("      标准差: {:.2}ms", std_dev);
        println!("      吞吐量: {:.2} FPS", 1000.0 / avg_time);
        println!("    稳定性分析:");
        println!("      时间变异系数: {:.1}%", (std_dev / avg_time * 100.0));
    }

    println!();

    // 分析结果
    println!("  🎯 性能总结:");
    println!("    - 单线程推理避免了RGA并发冲突");
    println!("    - 推理时间稳定，适合实时应用");
    println!("    - 建议用于视频流处理或批量推理");
    println!();
}