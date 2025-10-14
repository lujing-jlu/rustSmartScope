//! 异步推理服务演示
//!
//! 展示如何使用新的异步推理接口进行高性能AI检测
//!
//! 运行命令：
//! ```bash
//! cargo run --release -p rknn-inference --example async_inference_demo
//! ```

use rknn_inference::{MultiDetectorInferenceService, ImagePreprocessor, get_class_name};
use std::time::{Duration, Instant};
use std::thread;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("🚀 异步推理服务演示");
    println!();

    // 1. 查找并创建推理服务
    let possible_paths = [
        "models/yolov8m.rknn",
        "../models/yolov8m.rknn",
        "../../models/yolov8m.rknn",
        "../../../models/yolov8m.rknn",
        "crates/rknn-inference/models/yolov8m.rknn",
        &format!("{}/models/yolov8m.rknn", env!("CARGO_MANIFEST_DIR")),
    ];

    let mut model_path = None;
    for path in &possible_paths {
        if std::path::Path::new(path).exists() {
            model_path = Some(path.to_string());
            println!("✓ 找到模型文件: {}", path);
            break;
        }
    }

    let model_path = model_path.ok_or("未找到模型文件，请确保以下路径之一存在模型文件")?;

    // 创建6个detector的推理服务，队列长度限制为6
    println!("📝 创建异步推理服务...");
    let service_start = Instant::now();
    let service = MultiDetectorInferenceService::new(&model_path, 6)?;
    let init_time = service_start.elapsed();
    println!("✓ 服务创建成功: {:.2}ms", init_time.as_millis());
    println!("✓ 工作线程数: {}", service.worker_count());
    println!("✓ 队列长度限制: 6");
    println!();

    // 2. 加载真实图片
    println!("📷 加载测试图片...");
    let possible_image_paths = [
        "tests/test.jpg",
        "../tests/test.jpg",
        "../../tests/test.jpg",
        "examples/test.jpg",
        "../examples/test.jpg",
    ];

    let mut img_path = None;
    for path in &possible_image_paths {
        if std::path::Path::new(path).exists() {
            img_path = Some(path.to_string());
            println!("✓ 找到图片文件: {}", path);
            break;
        }
    }

    let img_path = img_path.ok_or("未找到测试图片，请确保以下路径之一存在测试图片")?;
    let img = image::open(&img_path)?.to_rgb8();
    println!("✓ 图片加载成功: {}x{}", img.width(), img.height());

    let preprocessor = ImagePreprocessor::new();

    // 预处理图片
    println!("🔄 预处理图片...");
    let preprocess_start = Instant::now();
    let image_buffer = preprocessor.preprocess(&img);
    let preprocess_time = preprocess_start.elapsed();
    println!("✓ 图片预处理完成: {}x{} ({:.2}ms)",
        image_buffer.width, image_buffer.height, preprocess_time.as_millis());
    println!();

    // 异步批量推理演示
    println!("🔥 === 异步批量推理演示 ===");
    let mut submitted_tasks = Vec::new();
    let start_time = Instant::now();

    // 连续提交10个推理任务（使用同一张图片进行批量测试）
    println!("📤 异步提交10个推理任务...");
    for _i in 0..10 {
        match service.submit_inference(&image_buffer) {
            Ok(task_id) => {
                submitted_tasks.push(task_id);
                println!("✓ 提交任务 {} (输入队列: {}/{})",
                    task_id, service.input_queue_len(), 6);
            }
            Err(e) => {
                println!("✗ 提交任务失败: {:?}", e);
            }
        }

        // 模拟实时场景的间隔（摄像头帧率约20fps）
        thread::sleep(Duration::from_millis(50));
    }

    println!("✓ 所有任务提交完成，总耗时: {:.2}ms", start_time.elapsed().as_millis());

    // 异步获取结果演示
    println!("📥 异步获取推理结果...");
    let mut completed_tasks = Vec::new();
    let result_start_time = Instant::now();
    let mut total_detections = 0;

    // 持续获取结果直到所有任务完成或超时
    let start_time = Instant::now();
    let timeout = Duration::from_secs(30); // 30秒超时

    while start_time.elapsed() < timeout && completed_tasks.len() < submitted_tasks.len() {
        // 尝试获取单个结果
        if let Some((task_id, results)) = service.try_get_result() {
            match results {
                Ok(detections) => {
                    total_detections += detections.len();
                    if detections.len() > 0 {
                        println!("✓ 任务 {} 完成: 检测到 {} 个对象", task_id, detections.len());

                        // 显示前3个检测结果
                        for (i, detection) in detections.iter().take(3).enumerate() {
                            let class_name = get_class_name(detection.class_id)
                                .unwrap_or_else(|| format!("class_{}", detection.class_id));
                            println!("    {}. {} - 置信度: {:.1}%",
                                i + 1, class_name, detection.confidence * 100.0);
                        }

                        if detections.len() > 3 {
                            println!("    ... 还有 {} 个对象", detections.len() - 3);
                        }
                    } else {
                        println!("✓ 任务 {} 完成: 未检测到对象", task_id);
                    }
                }
                Err(e) => {
                    println!("✗ 任务 {} 失败: {:?}", task_id, e);
                }
            }

            completed_tasks.push(task_id);
        }

        // 批量获取剩余结果（提高效率）
        let batch_results = service.get_all_results();
        if !batch_results.is_empty() {
            println!("📦 批量获取 {} 个结果...", batch_results.len());
            for (task_id, results) in batch_results {
                match results {
                    Ok(detections) => {
                        total_detections += detections.len();
                        if detections.len() > 0 {
                            println!("  ✓ 任务 {} - {} 个对象", task_id, detections.len());
                        } else {
                            println!("  ✓ 任务 {} - 未检测到对象", task_id);
                        }
                    }
                    Err(e) => {
                        println!("  ✗ 任务 {} - 失败: {:?}", task_id, e);
                    }
                }
                completed_tasks.push(task_id);
            }
        }

        // 暂无结果时短暂等待
        if service.is_output_queue_empty() {
            thread::sleep(Duration::from_millis(10));
        }
    }

    let total_result_time = result_start_time.elapsed();
    if completed_tasks.len() < submitted_tasks.len() {
        println!("⚠ 超时，未完成所有任务。完成: {}/{}", completed_tasks.len(), submitted_tasks.len());
    } else {
        println!("✓ 所有结果获取完成！");
    }
    println!("📊 结果获取统计: {:.2}ms, 总检测对象: {} 个", total_result_time.as_millis(), total_detections);

    // 性能统计
    println!("\n📊 === 性能统计 ===");
    let stats = service.get_stats();
    println!("✓ 总任务数: {}", stats.total_tasks);
    println!("✓ 已完成任务数: {}", stats.completed_tasks);
    println!("✓ 输入队列长度: {}", stats.input_queue_size);
    println!("✓ 输出队列长度: {}", stats.output_queue_size);
    println!("✓ 活跃工作线程数: {}", stats.active_workers);

    // 计算平均性能
    let avg_inference_time = total_result_time.as_millis() as f64 / completed_tasks.len() as f64;
    let throughput = completed_tasks.len() as f64 / total_result_time.as_secs_f64();
    println!("✓ 平均推理时间: {:.2}ms/任务", avg_inference_time);
    println!("✓ 吞吐量: {:.1} 任务/秒", throughput);
    println!("✓ 总检测对象: {} 个", total_detections);

    // 实时处理演示（边提交边获取）
    println!("\n🔄 === 实时处理演示 ===");
    println!("模拟实时视频流处理（边提交边获取）");

    // 清空队列开始新的演示
    service.clear_all_queues();
    let real_time_start = Instant::now();
    let mut real_time_submitted = 0;
    let mut real_time_completed = 0;

    for _i in 0..5 {
        // 提交任务
        match service.submit_inference(&image_buffer) {
            Ok(task_id) => {
                real_time_submitted += 1;
                println!("📤 提交实时任务 {}", task_id);
            }
            Err(e) => {
                println!("✗ 提交任务失败: {:?}", e);
            }
        }

        // 模拟帧间隔（约10fps）
        thread::sleep(Duration::from_millis(100));

        // 尝试获取已完成的结果
        while let Some((task_id, results)) = service.try_get_result() {
            real_time_completed += 1;
            match results {
                Ok(detections) => {
                    if detections.len() > 0 {
                        println!("→ ✓ 实时任务 {} 完成: {} 个对象", task_id, detections.len());
                    } else {
                        println!("→ ✓ 实时任务 {} 完成: 无检测", task_id);
                    }
                }
                Err(e) => {
                    println!("→ ✗ 实时任务 {} 失败: {:?}", task_id, e);
                }
            }
        }
    }

    // 获取剩余的结果
    while real_time_completed < real_time_submitted {
        if let Some((task_id, results)) = service.try_get_result() {
            real_time_completed += 1;
            match results {
                Ok(detections) => {
                    println!("→ ✓ 最终任务 {} 完成: {} 个对象", task_id, detections.len());
                }
                Err(e) => {
                    println!("→ ✗ 最终任务 {} 失败: {:?}", task_id, e);
                }
            }
        } else {
            thread::sleep(Duration::from_millis(10));
        }
    }

    let real_time_total = real_time_start.elapsed();
    println!("✓ 实时处理完成: {:.2}ms", real_time_total.as_millis());
    println!("✓ 实时处理吞吐量: {:.1} 任务/秒", real_time_submitted as f64 / real_time_total.as_secs_f64());

    println!("\n🎉 === 演示完成 ===");
    println!("✨ 异步推理服务特点:");
    println!("  🚀 输入和输出都是队列，不会阻塞");
    println!("  🛡️ 队列长度限制为6，防止内存积压");
    println!("  🔄 可以持续输入，按需获取输出");
    println!("  📦 支持批量获取结果，提高效率");
    println!("  🔙 保持向后兼容，支持同步接口");
    println!("  ⚡ 高性能并行处理，充分利用RK3588 NPU");
    println!();

    println!("💡 适用场景:");
    println!("  📹 实时视频流处理");
    println!("  🤖 高并发AI检测服务");
    println!("  📊 批量图像处理");
    println!("  🎯 需要高吞吐量的应用");
    println!("  🔧 内存受限的嵌入式设备");

    Ok(())
}