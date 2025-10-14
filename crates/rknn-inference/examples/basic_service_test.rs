use rknn_inference::{get_class_name, ImageBuffer, ImageFormat, InferenceService};
use std::sync::Arc;
use std::time::{Duration, Instant};

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║        YOLOv8 RKNN 推理服务基础测试             ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    let model_path = "models/yolov8m.rknn";
    let test_image_path = "tests/test.jpg";
    let num_workers = 3;
    let max_queue_size = 3;

    println!("测试配置:");
    println!("  模型路径:     {}", model_path);
    println!("  测试图片:     {}", test_image_path);
    println!("  工作线程数:   {}", num_workers);
    println!("  最大队列大小: {}", max_queue_size);
    println!("  说明:         3线程避免CPU满载");
    println!();

    // 1. 加载测试图片
    println!("📷 加载测试图片...");
    let img = image::open(test_image_path).expect("Failed to load test image");
    let img = img.to_rgb8();
    println!("✓ 图片加载成功: {}x{}", img.width(), img.height());
    println!();

    // 2. 创建推理服务
    println!("🚀 创建推理服务...");
    let service = Arc::new(InferenceService::new(model_path, num_workers, max_queue_size)
        .expect("Failed to create inference service"));
    println!("✓ 推理服务创建成功");
    println!();

    // 3. 测试 1: 单个推理任务
    println!("🧪 测试 1: 单个推理任务");
    test_single_inference(&service, &img);

    // 4. 测试 2: 顺序推理任务
    println!("🧪 测试 2: 顺序推理任务（10个任务）");
    test_sequential_inference(&service, &img, 10);

    // 5. 测试 3: 并发推理任务
    println!("🧪 测试 3: 并发推理任务（3个任务）");
    test_concurrent_inference(&service, &img, 3);

    // 6. 测试 4: 队列满测试
    println!("🧪 测试 4: 队列满测试（快速提交8个任务）");
    test_queue_overflow(&service, &img, 8);

    // 7. 最终统计
    println!("📊 最终统计:");
    let stats = service.get_stats();
    println!("  处理的任务数: {}", stats.processed_tasks);
    println!("  丢弃的任务数: {}", stats.dropped_tasks);
    println!("  队列中剩余:   {}", stats.queue_size);
    println!("  下一个期望ID: {}", stats.next_expected_task_id);
    println!();

    println!("✅ 所有测试完成！");
}

fn test_single_inference(service: &Arc<InferenceService>, img: &image::RgbImage) {
    println!("  运行单个推理任务...");

    let image_buffer = ImageBuffer {
        width: img.width() as i32,
        height: img.height() as i32,
        format: ImageFormat::Rgb888,
        data: img.clone().into_raw(),
    };

    let start = Instant::now();
    let result = service.inference_sync(image_buffer, Duration::from_secs(5));
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

fn test_sequential_inference(service: &Arc<InferenceService>, img: &image::RgbImage, num_tasks: usize) {
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
        let result = service.inference_sync(image_buffer, Duration::from_secs(10));
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

fn test_concurrent_inference(service: &Arc<InferenceService>, img: &image::RgbImage, num_tasks: usize) {
    println!("  运行 {} 个并发任务...", num_tasks);

    let start_total = Instant::now();
    let mut handles = Vec::new();

    // 创建多个并发任务
    for i in 0..num_tasks {
        let image_buffer = ImageBuffer {
            width: img.width() as i32,
            height: img.height() as i32,
            format: ImageFormat::Rgb888,
            data: img.clone().into_raw(),
        };

        let service_clone = service.clone();
        let handle = std::thread::spawn(move || {
            let start = Instant::now();
            let result = service_clone.inference_sync(image_buffer, Duration::from_secs(15));
            let elapsed = start.elapsed();
            (i, result, elapsed)
        });

        handles.push(handle);
    }

    // 等待所有任务完成
    let mut completed_tasks = 0;
    let mut total_detections = 0;
    let mut inference_times = Vec::new();
    let mut success_count = 0;

    for handle in handles {
        match handle.join() {
            Ok((task_id, result, elapsed)) => {
                completed_tasks += 1;
                let elapsed_ms = elapsed.as_millis();

                match result {
                    Ok(detections) => {
                        inference_times.push(elapsed_ms);
                        success_count += 1;
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
    println!("    成功任务数: {}", success_count);
    println!("    总检测对象: {}", total_detections);
    println!("    总耗时: {:.2}ms", total_elapsed.as_millis());

    if !inference_times.is_empty() {
        let min_time = inference_times.iter().min().unwrap();
        let max_time = inference_times.iter().max().unwrap();
        let avg_time = inference_times.iter().sum::<u128>() as f64 / inference_times.len() as f64;

        println!("    推理时间统计:");
        println!("      最小: {:.2}ms", min_time);
        println!("      最大: {:.2}ms", max_time);
        println!("      平均: {:.2}ms", avg_time);

        let theoretical_time = avg_time * num_tasks as f64;
        let efficiency = theoretical_time / total_elapsed.as_millis() as f64;
        println!("    并发效率:");
        println!("      理论总时间: {:.2}ms", theoretical_time);
        println!("      实际总时间: {:.2}ms", total_elapsed.as_millis());
        println!("      并发倍数:   {:.2}x", efficiency);
        println!("      CPU 利用率:  {:.1}%", efficiency / num_tasks as f64 * 100.0);
    }
    println!();
}

fn test_queue_overflow(service: &Arc<InferenceService>, img: &image::RgbImage, num_tasks: usize) {
    println!("  快速提交 {} 个任务（队列大小为3）...", num_tasks);

    let start_total = Instant::now();
    let mut handles = Vec::new();

    // 快速提交大量任务
    for i in 0..num_tasks {
        let image_buffer = ImageBuffer {
            width: img.width() as i32,
            height: img.height() as i32,
            format: ImageFormat::Rgb888,
            data: img.clone().into_raw(),
        };

        let service_clone = service.clone();
        let handle = std::thread::spawn(move || {
            let start = Instant::now();
            let result = service_clone.inference_sync(image_buffer, Duration::from_secs(20));
            let elapsed = start.elapsed();
            (i, result, elapsed)
        });

        handles.push(handle);

        // 立即提交下一个任务，模拟快速请求
        if i < num_tasks - 1 {
            std::thread::sleep(Duration::from_millis(1));
        }
    }

    // 等待所有任务完成
    let mut completed_tasks = 0;
    let mut dropped_tasks = 0;
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
                        if format!("{:?}", e).contains("Timeout") {
                            dropped_tasks += 1;
                            println!("    任务 {}: 超时（可能被丢弃）", task_id + 1);
                        } else {
                            println!("    任务 {}: 失败 ({:?})", task_id + 1, e);
                        }
                    }
                }
            }
            Err(e) => {
                println!("    线程错误: {:?}", e);
            }
        }
    }

    let total_elapsed = start_total.elapsed();

    println!("  队列溢出测试结果:");
    println!("    提交任务数: {}", num_tasks);
    println!("    完成任务数: {}", completed_tasks);
    println!("    成功任务数: {}", completed_tasks - dropped_tasks);
    println!("    丢弃/超时数: {}", dropped_tasks);
    println!("    总检测对象: {}", total_detections);
    println!("    总耗时: {:.2}ms", total_elapsed.as_millis());

    // 检查队列状态
    let stats = service.get_stats();
    println!("    服务统计:");
    println!("      队列丢弃数: {}", stats.dropped_tasks);
    println!("      已处理数:   {}", stats.processed_tasks);
    println!();
}