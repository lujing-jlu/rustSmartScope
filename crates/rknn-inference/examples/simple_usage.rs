//! 简单使用示例
//!
//! 展示如何使用MultiDetectorInferenceService进行YOLOv8推理
//!
//! 运行命令：
//! ```bash
//! cargo run --release -p rknn-inference --example simple_usage
//! ```

use rknn_inference::{get_class_name, MultiDetectorInferenceService};
use std::time::Instant;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("🚀 MultiDetectorInferenceService 简单使用示例");
    println!();

    // 1. 创建6个detector的高性能推理服务
    println!("📝 初始化推理服务...");
    let service_start = Instant::now();
    let service = MultiDetectorInferenceService::new("models/yolov8m.rknn", 6)?;
    let init_time = service_start.elapsed();
    println!("✓ 服务初始化完成: {:.2}ms", init_time.as_millis());
    println!("✓ 工作线程数: {}", service.worker_count());
    println!();

    // 2. 加载并预处理图片
    println!("📷 加载图片...");
    let img = image::open("tests/test.jpg")?.to_rgb8();
    println!("✓ 图片加载成功: {}x{}", img.width(), img.height());

    // 创建预处理器
    let preprocessor = rknn_inference::ImagePreprocessor::new();

    // 预处理图片
    println!("🔄 预处理图片...");
    let image_buffer = preprocessor.preprocess(&img);
    println!("✓ 图片预处理完成: {}x{}", image_buffer.width, image_buffer.height);
    println!();

    // 3. 执行推理
    println!("🧠 执行推理...");
    let inference_start = Instant::now();
    let results = service.inference_preprocessed(&image_buffer)?;
    let inference_time = inference_start.elapsed();

    println!("✓ 推理完成: {:.2}ms", inference_time.as_millis());
    println!("✓ 检测到 {} 个对象", results.len());
    println!();

    // 4. 显示检测结果
    if !results.is_empty() {
        println!("🎯 检测结果:");
        for (i, detection) in results.iter().take(5).enumerate() {
            let class_name = get_class_name(detection.class_id)
                .unwrap_or_else(|| format!("class_{}", detection.class_id));
            println!(
                "  {}. {} - 置信度: {:.1}%, 位置: ({},{},{},{})",
                i + 1,
                class_name,
                detection.confidence * 100.0,
                detection.bbox.left,
                detection.bbox.top,
                detection.bbox.right,
                detection.bbox.bottom
            );
        }

        if results.len() > 5 {
            println!("  ... 还有 {} 个对象", results.len() - 5);
        }
    } else {
        println!("❌ 未检测到任何对象");
    }

    println!();
    println!("🎉 示例完成！");

    Ok(())
}