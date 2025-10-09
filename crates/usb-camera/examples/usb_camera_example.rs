//! USB Camera Linux-Video 实现示例
//! 
//! 这个示例展示了如何正确使用 usb-camera 库的 linux-video 实现：
//! - 设备发现和配置
//! - 流的启动和停止
//! - 帧的读取和处理
//! - 错误处理和资源管理
//! - 性能监控

use std::collections::HashMap;
use std::time::{Duration, Instant};
use usb_camera::{config::CameraConfig, stream::CameraStreamReader};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 初始化日志
    env_logger::init();
    
    println!("🎥 USB Camera Linux-Video 实现示例");
    println!("{}", "=".repeat(50));

    // 定义要测试的设备
    let device_paths = ["/dev/video1", "/dev/video3"];
    
    for device_path in &device_paths {
        println!("\n📹 测试设备: {}", device_path);
        
        if let Err(e) = test_camera_device(device_path) {
            println!("❌ 设备测试失败: {}", e);
        }
    }

    println!("\n🎉 示例完成！");
    Ok(())
}

/// 测试单个相机设备
fn test_camera_device(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    // 1. 检查设备是否存在
    if !std::path::Path::new(device_path).exists() {
        return Err(format!("设备不存在: {}", device_path).into());
    }

    // 2. 创建相机配置
    let config = CameraConfig {
        width: 1280,
        height: 720,
        framerate: 30,
        pixel_format: "MJPG".to_string(),
        parameters: HashMap::new(),
    };

    println!("📋 配置: {}x{} @ {} FPS, 格式: {}", 
        config.width, config.height, config.framerate, config.pixel_format);

    // 3. 创建流读取器
    let camera_name = format!("Camera_{}", device_path.replace("/dev/video", ""));
    let mut reader = CameraStreamReader::new(device_path, &camera_name, config);

    // 4. 启动流
    reader.start()?;
    println!("✅ 流启动成功");

    // 5. 等待流稳定
    std::thread::sleep(Duration::from_millis(500));

    // 6. 性能测试和帧读取
    let result = capture_and_analyze_frames(&mut reader, device_path);

    // 7. 停止流 (确保资源清理)
    if let Err(e) = reader.stop() {
        println!("⚠️  停止流失败: {}", e);
    } else {
        println!("🛑 流已停止");
    }

    result
}

/// 捕获和分析帧
fn capture_and_analyze_frames(
    reader: &mut CameraStreamReader, 
    device_path: &str
) -> Result<(), Box<dyn std::error::Error>> {
    println!("📸 开始捕获帧...");
    
    let start_time = Instant::now();
    let mut frame_count = 0;
    let mut total_size = 0u64;
    let mut frame_intervals = Vec::new();
    let mut last_frame_time = None;
    
    const MAX_FRAMES: usize = 30;
    const TIMEOUT: Duration = Duration::from_secs(5);
    
    // 读取帧循环
    while frame_count < MAX_FRAMES && start_time.elapsed() < TIMEOUT {
        match reader.read_frame() {
            Ok(Some(frame)) => {
                let now = Instant::now();
                frame_count += 1;
                total_size += frame.data.len() as u64;
                
                // 计算帧间隔
                if let Some(last_time) = last_frame_time {
                    let interval = now.duration_since(last_time);
                    frame_intervals.push(interval);
                }
                last_frame_time = Some(now);
                
                // 显示帧信息
                if frame_count <= 5 || frame_count % 10 == 0 {
                    println!("  📦 帧 {}: {}x{}, {} KB, ID: {}", 
                        frame_count,
                        frame.width,
                        frame.height,
                        frame.data.len() / 1024,
                        frame.frame_id
                    );
                }
                
                // 保存第一帧作为示例
                if frame_count == 1 {
                    save_sample_frame(&frame, device_path)?;
                }
            }
            Ok(None) => {
                // 没有帧可用，短暂等待
                std::thread::sleep(Duration::from_millis(10));
            }
            Err(e) => {
                println!("❌ 读取帧失败: {}", e);
                break;
            }
        }
    }
    
    // 显示性能统计
    display_performance_stats(frame_count, total_size, &frame_intervals, start_time.elapsed());
    
    Ok(())
}

/// 保存示例帧
fn save_sample_frame(
    frame: &usb_camera::stream::VideoFrame, 
    device_path: &str
) -> Result<(), Box<dyn std::error::Error>> {
    let filename = format!("sample_frame_{}.jpg", device_path.replace("/dev/video", ""));
    std::fs::write(&filename, &frame.data)?;
    println!("💾 示例帧已保存到: {}", filename);
    Ok(())
}

/// 显示性能统计
fn display_performance_stats(
    frame_count: usize,
    total_size: u64,
    intervals: &[Duration],
    total_duration: Duration,
) {
    println!("\n📊 性能统计:");
    println!("  总帧数: {}", frame_count);
    println!("  总时长: {:.2}s", total_duration.as_secs_f64());
    
    if frame_count > 0 {
        let avg_fps = frame_count as f64 / total_duration.as_secs_f64();
        let avg_size = total_size as f64 / frame_count as f64 / 1024.0;
        
        println!("  平均FPS: {:.2}", avg_fps);
        println!("  平均帧大小: {:.1} KB", avg_size);
    }
    
    if !intervals.is_empty() {
        let avg_interval = intervals.iter()
            .map(|d| d.as_millis() as f64)
            .sum::<f64>() / intervals.len() as f64;
        let min_interval = intervals.iter().min().unwrap().as_millis();
        let max_interval = intervals.iter().max().unwrap().as_millis();
        
        println!("  帧间隔: 平均 {:.1}ms, 最小 {}ms, 最大 {}ms", 
            avg_interval, min_interval, max_interval);
    }
}
