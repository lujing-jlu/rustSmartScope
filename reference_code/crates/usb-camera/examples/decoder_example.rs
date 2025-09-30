//! Hardware decoder example
//!
//! This example demonstrates how to use the hardware-accelerated decoder
//! with the USB camera stream to decode MJPEG frames.

use std::collections::HashMap;
use std::time::{Duration, Instant};
use usb_camera::{
    config::CameraConfig,
    decoder::{DecodePriority, DecodeResult, DecodedFormat, DecoderConfig, DecoderManager},
    stream::CameraStreamReader,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize logging
    env_logger::init();

    println!("🎥 Hardware Decoder Example");
    println!("{}", "=".repeat(50));

    // Test device
    let device_path = "/dev/video1";

    if !std::path::Path::new(device_path).exists() {
        println!("❌ 设备不存在: {}", device_path);
        return Ok(());
    }

    // Create camera configuration
    let config = CameraConfig {
        width: 1280,
        height: 720,
        framerate: 30,
        pixel_format: "MJPG".to_string(),
        parameters: HashMap::new(),
    };

    println!(
        "📋 配置: {}x{} @ {} FPS, 格式: {}",
        config.width, config.height, config.framerate, config.pixel_format
    );

    // Create stream reader
    let camera_name = format!("Camera_{}", device_path.replace("/dev/video", ""));
    let mut reader = CameraStreamReader::new(device_path, &camera_name, config);

    // Start stream
    reader.start()?;
    println!("✅ 流启动成功");

    // Create decoder configuration
    let decoder_config = DecoderConfig {
        enabled: true,
        max_queue_size: 50,
        target_format: DecodedFormat::RGB888,
        max_resolution: (1920, 1080),
        enable_caching: true,
        cache_size: 5,
        priority: DecodePriority::Normal,
    };

    // Create and start decoder
    let mut decoder = DecoderManager::new(decoder_config);
    decoder.start()?;
    println!("✅ 解码器启动成功");

    // Wait for stream to stabilize
    std::thread::sleep(Duration::from_millis(500));

    // Statistics
    let mut frame_count = 0;
    let mut decoded_count = 0;
    let mut last_stats_time = Instant::now();
    let start_time = Instant::now();

    println!("\n📊 开始处理帧...");

    // Main processing loop
    for i in 0..300 {
        // Process 300 frames (about 10 seconds at 30 FPS)
        match reader.read_frame() {
            Ok(Some(frame)) => {
                frame_count += 1;

                // Submit frame for decoding
                if let Some(request_id) = decoder.submit_frame(frame.clone()) {
                    if frame_count <= 5 {
                        println!(
                            "  📦 提交帧 {} 进行解码 (请求ID: {})",
                            frame_count, request_id
                        );
                    }
                } else {
                    if frame_count <= 5 {
                        println!("  ⚠️  解码器队列满，跳过帧 {}", frame_count);
                    }
                }

                // Check for decoded results
                while let Some(result) = decoder.try_get_result() {
                    match result {
                        DecodeResult::Success(decoded_frame) => {
                            decoded_count += 1;

                            if decoded_count <= 5 {
                                println!(
                                    "  ✅ 解码成功: {}x{} {:?} 格式, 耗时: {:?}",
                                    decoded_frame.width,
                                    decoded_frame.height,
                                    decoded_frame.format,
                                    decoded_frame.decode_duration
                                );
                            }

                            // Save first decoded frame as example
                            if decoded_count == 1 {
                                save_decoded_frame(&decoded_frame)?;
                            }
                        }
                        DecodeResult::Error(error) => {
                            println!("  ❌ 解码错误: {}", error);
                        }
                        DecodeResult::Skipped => {
                            println!("  ⏭️  帧被跳过");
                        }
                    }
                }

                // Print statistics every 5 seconds
                if last_stats_time.elapsed() >= Duration::from_secs(5) {
                    let elapsed = start_time.elapsed().as_secs_f64();
                    let frame_fps = frame_count as f64 / elapsed;
                    let decode_fps = decoded_count as f64 / elapsed;
                    let decode_ratio = if frame_count > 0 {
                        (decoded_count as f64 / frame_count as f64) * 100.0
                    } else {
                        0.0
                    };

                    println!("\n📊 统计信息 ({:.1}s):", elapsed);
                    println!("  📸 原始帧: {} ({:.1} FPS)", frame_count, frame_fps);
                    println!("  🎨 解码帧: {} ({:.1} FPS)", decoded_count, decode_fps);
                    println!("  🔄 解码率: {:.1}%", decode_ratio);

                    let stats = decoder.get_stats();
                    println!(
                        "  📋 解码器状态: 队列={}, 结果队列={}, 运行={}",
                        stats.queue_size, stats.result_queue_size, stats.running
                    );

                    last_stats_time = Instant::now();
                }
            }
            Ok(None) => {
                // No frame available, short wait
                std::thread::sleep(Duration::from_millis(10));
            }
            Err(e) => {
                println!("❌ 读取帧失败: {}", e);
                break;
            }
        }

        // Short delay between iterations
        if i % 30 == 0 {
            std::thread::sleep(Duration::from_millis(100));
        }
    }

    // Final statistics
    let total_elapsed = start_time.elapsed().as_secs_f64();
    let final_frame_fps = frame_count as f64 / total_elapsed;
    let final_decode_fps = decoded_count as f64 / total_elapsed;
    let final_decode_ratio = if frame_count > 0 {
        (decoded_count as f64 / frame_count as f64) * 100.0
    } else {
        0.0
    };

    println!("\n🎯 最终统计:");
    println!("  总时长: {:.2}s", total_elapsed);
    println!("  原始帧: {} ({:.1} FPS)", frame_count, final_frame_fps);
    println!("  解码帧: {} ({:.1} FPS)", decoded_count, final_decode_fps);
    println!("  解码成功率: {:.1}%", final_decode_ratio);

    // Stop decoder and stream
    decoder.stop()?;
    reader.stop()?;

    println!("\n🛑 解码器和流已停止");
    println!("🎉 示例完成！");

    Ok(())
}

/// Save a decoded frame as an example
fn save_decoded_frame(
    frame: &usb_camera::decoder::DecodedFrame,
) -> Result<(), Box<dyn std::error::Error>> {
    let filename = format!(
        "decoded_frame_{}x{}_{:?}.raw",
        frame.width, frame.height, frame.format
    );

    std::fs::write(&filename, &frame.data)?;
    println!(
        "💾 解码帧已保存到: {} ({} bytes)",
        filename,
        frame.data.len()
    );

    // Also save metadata
    let metadata = format!(
        "Decoded Frame Metadata\n\
        Width: {}\n\
        Height: {}\n\
        Format: {:?}\n\
        Camera: {}\n\
        Frame ID: {}\n\
        Decode Duration: {:?}\n\
        Data Size: {} bytes\n\
        Timestamp: {:?}\n",
        frame.width,
        frame.height,
        frame.format,
        frame.camera_name,
        frame.frame_id,
        frame.decode_duration,
        frame.data.len(),
        frame.timestamp
    );

    let metadata_filename = format!(
        "decoded_frame_{}x{}_{:?}.txt",
        frame.width, frame.height, frame.format
    );
    std::fs::write(&metadata_filename, metadata)?;
    println!("📄 元数据已保存到: {}", metadata_filename);

    Ok(())
}
