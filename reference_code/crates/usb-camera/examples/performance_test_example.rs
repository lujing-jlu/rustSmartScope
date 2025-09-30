//! Performance Testing Example
//!
//! This example demonstrates:
//! - Frame rate benchmarking
//! - Memory usage monitoring
//! - Latency measurement
//! - Throughput analysis
//! - Different resolution and format testing

use std::collections::HashMap;
use std::time::{Duration, Instant};
use usb_camera::{config::CameraConfig, device::V4L2DeviceManager, stream::CameraStreamReader};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize logging
    env_logger::init();

    println!("⚡ USB Camera Performance Testing Example");
    println!("{}", "=".repeat(50));

    // Find available cameras
    let devices = V4L2DeviceManager::discover_devices()?;

    if devices.is_empty() {
        println!("❌ 未发现任何相机设备");
        return Ok(());
    }

    let device = &devices[0];
    let device_path = device.path.to_string_lossy();
    println!("📷 测试设备: {} ({})", device.name, device_path);

    // Run different performance tests
    test_frame_rate_benchmark(&device_path)?;
    test_different_resolutions(&device_path)?;
    test_format_comparison(&device_path)?;
    test_sustained_performance(&device_path)?;

    println!("\n🎉 性能测试完成！");
    Ok(())
}

/// Test frame rate benchmarking
fn test_frame_rate_benchmark(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n📊 帧率基准测试");
    println!("{}", "-".repeat(30));

    let config = CameraConfig {
        width: 1280,
        height: 720,
        framerate: 30,
        pixel_format: "MJPG".to_string(),
        parameters: HashMap::new(),
    };

    let camera_name = "PerformanceTest";
    let mut reader = CameraStreamReader::new(device_path, camera_name, config);

    println!("🚀 启动流 (1280x720 @ 30fps MJPG)...");
    reader.start()?;

    // Warm up
    println!("🔥 预热阶段 (2秒)...");
    let warmup_start = Instant::now();
    while warmup_start.elapsed() < Duration::from_secs(2) {
        if let Ok(_) = reader.read_frame() {
            // Just consume frames
        }
    }

    // Benchmark phase
    println!("⏱️ 基准测试阶段 (10秒)...");
    let test_start = Instant::now();
    let mut frame_count = 0;
    let mut total_frame_size = 0;
    let mut min_frame_size = usize::MAX;
    let mut max_frame_size = 0;
    let mut frame_intervals = Vec::new();
    let mut last_frame_time = Instant::now();

    while test_start.elapsed() < Duration::from_secs(10) {
        match reader.read_frame() {
            Ok(Some(frame)) => {
                frame_count += 1;
                total_frame_size += frame.data.len();
                min_frame_size = min_frame_size.min(frame.data.len());
                max_frame_size = max_frame_size.max(frame.data.len());

                let now = Instant::now();
                if frame_count > 1 {
                    frame_intervals.push(now.duration_since(last_frame_time));
                }
                last_frame_time = now;
            }
            Ok(None) => {
                // No frame available
            }
            Err(_) => {
                // Skip failed frames
            }
        }
    }

    reader.stop()?;

    // Calculate statistics
    let test_duration = test_start.elapsed();
    let actual_fps = frame_count as f64 / test_duration.as_secs_f64();
    let avg_frame_size = if frame_count > 0 {
        total_frame_size / frame_count
    } else {
        0
    };
    let throughput_mbps =
        (total_frame_size as f64 * 8.0) / (test_duration.as_secs_f64() * 1_000_000.0);

    // Frame interval statistics
    let avg_interval = if !frame_intervals.is_empty() {
        frame_intervals.iter().sum::<Duration>().as_millis() as f64 / frame_intervals.len() as f64
    } else {
        0.0
    };

    let min_interval = frame_intervals
        .iter()
        .min()
        .map(|d| d.as_millis())
        .unwrap_or(0);
    let max_interval = frame_intervals
        .iter()
        .max()
        .map(|d| d.as_millis())
        .unwrap_or(0);

    println!("\n📈 基准测试结果:");
    println!("   测试时长: {:.2}秒", test_duration.as_secs_f64());
    println!("   总帧数: {}", frame_count);
    println!("   实际帧率: {:.2} FPS", actual_fps);
    println!("   平均帧大小: {:.1} KB", avg_frame_size as f64 / 1024.0);
    println!("   最小帧大小: {:.1} KB", min_frame_size as f64 / 1024.0);
    println!("   最大帧大小: {:.1} KB", max_frame_size as f64 / 1024.0);
    println!("   数据吞吐量: {:.2} Mbps", throughput_mbps);
    println!("   平均帧间隔: {:.1} ms", avg_interval);
    println!("   最小帧间隔: {} ms", min_interval);
    println!("   最大帧间隔: {} ms", max_interval);

    Ok(())
}

/// Test different resolutions
fn test_different_resolutions(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n📐 不同分辨率性能测试");
    println!("{}", "-".repeat(30));

    let resolutions = vec![
        (640, 480, "VGA"),
        (1280, 720, "HD"),
        // Add more if supported
    ];

    for (width, height, name) in resolutions {
        println!("\n🔍 测试分辨率: {} ({}x{})", name, width, height);

        let config = CameraConfig {
            width,
            height,
            framerate: 30,
            pixel_format: "MJPG".to_string(),
            parameters: HashMap::new(),
        };

        let camera_name = format!("PerfTest_{}", name);
        let mut reader = CameraStreamReader::new(device_path, &camera_name, config);

        match reader.start() {
            Ok(_) => {
                let test_start = Instant::now();
                let mut frame_count = 0;
                let mut total_size = 0;

                // Test for 3 seconds
                while test_start.elapsed() < Duration::from_secs(3) {
                    if let Ok(Some(frame)) = reader.read_frame() {
                        frame_count += 1;
                        total_size += frame.data.len();
                    }
                }

                reader.stop()?;

                let duration = test_start.elapsed();
                let fps = frame_count as f64 / duration.as_secs_f64();
                let avg_size = if frame_count > 0 {
                    total_size / frame_count
                } else {
                    0
                };

                println!("   ✅ 帧率: {:.1} FPS", fps);
                println!("   📦 平均帧大小: {:.1} KB", avg_size as f64 / 1024.0);
                println!("   📊 总帧数: {}", frame_count);
            }
            Err(e) => {
                println!("   ❌ 不支持该分辨率: {}", e);
            }
        }
    }

    Ok(())
}

/// Test format comparison
fn test_format_comparison(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n🎨 格式对比测试");
    println!("{}", "-".repeat(30));

    let formats = vec!["MJPG", "YUYV"];

    for format in formats {
        println!("\n🔍 测试格式: {}", format);

        let config = CameraConfig {
            width: 1280,
            height: 720,
            framerate: 30,
            pixel_format: format.to_string(),
            parameters: HashMap::new(),
        };

        let camera_name = format!("PerfTest_{}", format);
        let mut reader = CameraStreamReader::new(device_path, &camera_name, config);

        match reader.start() {
            Ok(_) => {
                let test_start = Instant::now();
                let mut frame_count = 0;
                let mut total_size = 0;

                // Test for 3 seconds
                while test_start.elapsed() < Duration::from_secs(3) {
                    if let Ok(Some(frame)) = reader.read_frame() {
                        frame_count += 1;
                        total_size += frame.data.len();
                    }
                }

                reader.stop()?;

                let duration = test_start.elapsed();
                let fps = frame_count as f64 / duration.as_secs_f64();
                let avg_size = if frame_count > 0 {
                    total_size / frame_count
                } else {
                    0
                };
                let throughput = (total_size as f64 * 8.0) / (duration.as_secs_f64() * 1_000_000.0);

                println!("   ✅ 帧率: {:.1} FPS", fps);
                println!("   📦 平均帧大小: {:.1} KB", avg_size as f64 / 1024.0);
                println!("   📊 吞吐量: {:.2} Mbps", throughput);
            }
            Err(e) => {
                println!("   ❌ 不支持该格式: {}", e);
            }
        }
    }

    Ok(())
}

/// Test sustained performance
fn test_sustained_performance(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n⏳ 持续性能测试");
    println!("{}", "-".repeat(30));

    let config = CameraConfig {
        width: 1280,
        height: 720,
        framerate: 30,
        pixel_format: "MJPG".to_string(),
        parameters: HashMap::new(),
    };

    let camera_name = "SustainedTest";
    let mut reader = CameraStreamReader::new(device_path, camera_name, config);

    println!("🚀 启动长时间测试 (30秒)...");
    reader.start()?;

    let test_start = Instant::now();
    let mut measurements = Vec::new();
    let measurement_interval = Duration::from_secs(5);
    let mut last_measurement = Instant::now();
    let mut interval_frames = 0;

    while test_start.elapsed() < Duration::from_secs(30) {
        if let Ok(_) = reader.read_frame() {
            interval_frames += 1;
        }

        if last_measurement.elapsed() >= measurement_interval {
            let fps = interval_frames as f64 / measurement_interval.as_secs_f64();
            measurements.push(fps);
            println!("   📊 {}秒: {:.1} FPS", measurements.len() * 5, fps);

            interval_frames = 0;
            last_measurement = Instant::now();
        }
    }

    reader.stop()?;

    // Calculate stability metrics
    if !measurements.is_empty() {
        let avg_fps = measurements.iter().sum::<f64>() / measurements.len() as f64;
        let min_fps = measurements.iter().fold(f64::INFINITY, |a, &b| a.min(b));
        let max_fps = measurements.iter().fold(0.0f64, |a, &b| a.max(b));
        let variance = measurements
            .iter()
            .map(|&x| (x - avg_fps).powi(2))
            .sum::<f64>()
            / measurements.len() as f64;
        let std_dev = variance.sqrt();

        println!("\n📈 持续性能统计:");
        println!("   平均帧率: {:.2} FPS", avg_fps);
        println!("   最低帧率: {:.2} FPS", min_fps);
        println!("   最高帧率: {:.2} FPS", max_fps);
        println!("   标准差: {:.2} FPS", std_dev);
        println!("   稳定性: {:.1}%", (1.0 - std_dev / avg_fps) * 100.0);
    }

    Ok(())
}
