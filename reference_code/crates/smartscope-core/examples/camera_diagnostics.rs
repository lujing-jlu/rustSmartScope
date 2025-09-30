//! SmartScope Core 相机诊断工具
//!
//! 这个工具用于诊断相机系统问题：
//! - 检测系统中的相机设备
//! - 验证相机功能
//! - 性能问题诊断
//! - 错误状态分析
//!
//! 运行方式：
//! ```bash
//! cargo run --example camera_diagnostics
//! ```

use smartscope_core::*;
use std::time::{Duration, Instant};

fn main() {
    println!("=== SmartScope Core 相机诊断工具 ===");

    // 系统相机检测
    println!("\n1. 系统相机设备检测");
    detect_system_cameras();

    // V4L2设备检查 (Linux)
    if cfg!(target_os = "linux") {
        println!("\n2. V4L2设备检查");
        check_v4l2_devices();
    }

    // SmartScopeCore功能测试
    println!("\n3. SmartScopeCore功能诊断");
    match diagnose_smartscope_core() {
        Ok(_) => println!("✓ SmartScopeCore诊断完成"),
        Err(e) => println!("✗ SmartScopeCore诊断失败: {}", e),
    }

    // 错误处理测试
    println!("\n4. 错误处理诊断");
    diagnose_error_handling();

    // 性能诊断
    println!("\n5. 性能诊断");
    if let Err(e) = diagnose_performance() {
        println!("性能诊断失败: {}", e);
    }

    println!("\n=== 诊断完成 ===");
}

fn detect_system_cameras() {
    // 检查 /dev/video* 设备
    println!("检查 /dev/video* 设备...");

    match std::fs::read_dir("/dev") {
        Ok(entries) => {
            let video_devices: Vec<_> = entries
                .filter_map(|entry| {
                    let entry = entry.ok()?;
                    let name = entry.file_name().to_string_lossy().to_string();
                    if name.starts_with("video") {
                        Some(format!("/dev/{}", name))
                    } else {
                        None
                    }
                })
                .collect();

            if video_devices.is_empty() {
                println!("  ✗ 未发现任何视频设备");
                println!("    建议: 请确保USB相机已连接并被系统识别");
            } else {
                println!("  ✓ 发现 {} 个视频设备:", video_devices.len());
                for device in &video_devices {
                    println!("    - {}", device);
                }
            }

            // 检查设备权限
            for device in &video_devices {
                match std::fs::metadata(device) {
                    Ok(metadata) => {
                        if metadata.permissions().readonly() {
                            println!("  ⚠ {} 只读权限，可能需要添加用户到video组", device);
                        }
                    }
                    Err(e) => {
                        println!("  ✗ {} 访问失败: {}", device, e);
                    }
                }
            }
        }
        Err(e) => {
            println!("  ✗ 读取 /dev 目录失败: {}", e);
        }
    }
}

fn check_v4l2_devices() {
    println!("检查V4L2设备信息...");

    for i in 0..8 {
        let device_path = format!("/dev/video{}", i);
        if std::path::Path::new(&device_path).exists() {
            println!("  检查设备: {}", device_path);

            // 尝试使用v4l2-ctl获取设备信息
            match std::process::Command::new("v4l2-ctl")
                .args(&["--device", &device_path, "--info"])
                .output()
            {
                Ok(output) => {
                    if output.status.success() {
                        let info = String::from_utf8_lossy(&output.stdout);
                        println!("    设备信息: {}", info.lines().next().unwrap_or("未知"));
                    } else {
                        println!("    ✗ v4l2-ctl查询失败");
                    }
                }
                Err(_) => {
                    println!("    ⚠ v4l2-ctl未安装，跳过详细信息查询");
                    break;
                }
            }

            // 尝试获取支持的格式
            match std::process::Command::new("v4l2-ctl")
                .args(&["--device", &device_path, "--list-formats"])
                .output()
            {
                Ok(output) => {
                    if output.status.success() {
                        let formats = String::from_utf8_lossy(&output.stdout);
                        let format_count = formats.lines().count();
                        println!("    支持 {} 种格式", format_count);
                    }
                }
                Err(_) => {} // v4l2-ctl未安装
            }
        }
    }
}

fn diagnose_smartscope_core() -> Result<(), SmartScopeError> {
    println!("创建SmartScopeCore实例...");
    let mut core = SmartScopeCore::new()?;
    println!("  ✓ 创建成功");

    // 测试默认配置
    println!("测试默认配置...");
    let stream_config = SmartScopeStreamConfig::default();
    let correction_config = SmartScopeCorrectionConfig::default();

    println!("  默认流配置:");
    println!("    - 分辨率: {}×{}", stream_config.width, stream_config.height);
    println!("    - 格式: {:?}", stream_config.format);
    println!("    - 帧率: {} FPS", stream_config.fps);

    // 初始化测试
    println!("初始化测试...");
    match core.initialize(stream_config, correction_config) {
        Ok(_) => {
            println!("  ✓ 初始化成功");

            // 检查相机状态
            match core.get_camera_status() {
                Ok(status) => {
                    println!("  相机状态:");
                    println!("    - 模式: {:?}", status.mode);
                    println!("    - 相机数量: {}", status.camera_count);
                    println!("    - 左相机: {}", if status.left_connected { "已连接" } else { "未连接" });
                    println!("    - 右相机: {}", if status.right_connected { "已连接" } else { "未连接" });
                }
                Err(e) => {
                    println!("  ⚠ 获取相机状态失败: {}", e);
                }
            }

            // 启动测试
            println!("启动测试...");
            match core.start() {
                Ok(_) => {
                    println!("  ✓ 启动成功");

                    // 帧获取测试
                    test_frame_capture(&mut core);

                    // 停止测试
                    match core.stop() {
                        Ok(_) => println!("  ✓ 停止成功"),
                        Err(e) => println!("  ✗ 停止失败: {}", e),
                    }
                }
                Err(e) => {
                    println!("  ✗ 启动失败: {}", e);
                    println!("    可能原因: 无可用相机设备或权限不足");
                }
            }
        }
        Err(e) => {
            println!("  ✗ 初始化失败: {}", e);
        }
    }

    Ok(())
}

fn test_frame_capture(core: &mut SmartScopeCore) {
    println!("  帧捕获测试...");

    // 测试左相机
    match core.get_left_frame() {
        Ok(frame) => {
            println!("    ✓ 左相机帧获取成功");
            println!("      - 尺寸: {}×{}", frame.width, frame.height);
            println!("      - 格式: {:?}", frame.format);
            println!("      - 数据大小: {} 字节", frame.size);
            println!("      - 时间戳: {}", frame.timestamp);

            // 验证数据完整性
            if frame.data.is_null() {
                println!("      ⚠ 数据指针为空");
            } else if frame.size == 0 {
                println!("      ⚠ 数据大小为0");
            } else {
                println!("      ✓ 数据完整性检查通过");
            }
        }
        Err(e) => {
            println!("    ✗ 左相机帧获取失败: {}", e);
        }
    }

    // 测试右相机
    match core.get_right_frame() {
        Ok(frame) => {
            println!("    ✓ 右相机帧获取成功");
            println!("      - 尺寸: {}×{}", frame.width, frame.height);
            println!("      - 数据大小: {} 字节", frame.size);
        }
        Err(e) => {
            println!("    ✗ 右相机帧获取失败: {}", e);
        }
    }
}

fn diagnose_error_handling() {
    println!("测试错误处理机制...");

    // 测试重复初始化
    println!("  测试重复初始化...");
    if let Ok(mut core) = SmartScopeCore::new() {
        let config = SmartScopeStreamConfig::default();
        let correction = SmartScopeCorrectionConfig::default();

        let _ = core.initialize(config.clone(), correction.clone());
        match core.initialize(config, correction) {
            Err(SmartScopeError::AlreadyInitialized(_)) => {
                println!("    ✓ 正确检测重复初始化错误");
            }
            _ => {
                println!("    ✗ 未正确处理重复初始化");
            }
        }
    }

    // 测试未初始化操作
    println!("  测试未初始化操作...");
    if let Ok(mut core) = SmartScopeCore::new() {
        match core.start() {
            Err(SmartScopeError::NotInitialized(_)) => {
                println!("    ✓ 正确检测未初始化错误");
            }
            _ => {
                println!("    ✗ 未正确处理未初始化操作");
            }
        }
    }

    // 测试未启动帧获取
    println!("  测试未启动帧获取...");
    if let Ok(mut core) = SmartScopeCore::new() {
        let config = SmartScopeStreamConfig::default();
        let correction = SmartScopeCorrectionConfig::default();

        if core.initialize(config, correction).is_ok() {
            match core.get_left_frame() {
                Err(SmartScopeError::NotStarted(_)) => {
                    println!("    ✓ 正确检测未启动错误");
                }
                _ => {
                    println!("    ✗ 未正确处理未启动帧获取");
                }
            }
        }
    }
}

fn diagnose_performance() -> Result<(), SmartScopeError> {
    println!("执行性能诊断...");

    let mut core = SmartScopeCore::new()?;
    let stream_config = SmartScopeStreamConfig {
        width: 640,
        height: 480,
        format: ImageFormat::RGB888,
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig::default();

    core.initialize(stream_config, correction_config)?;
    core.start()?;

    // 性能基准测试
    let test_duration = Duration::from_secs(3);
    let start_time = Instant::now();
    let mut frame_count = 0;
    let mut total_latency = Duration::ZERO;
    let mut max_latency = Duration::ZERO;

    println!("  执行 {} 秒性能测试...", test_duration.as_secs());

    while start_time.elapsed() < test_duration {
        let frame_start = Instant::now();

        if core.get_left_frame().is_ok() {
            let latency = frame_start.elapsed();
            frame_count += 1;
            total_latency += latency;

            if latency > max_latency {
                max_latency = latency;
            }
        }

        std::thread::sleep(Duration::from_millis(33));
    }

    let elapsed = start_time.elapsed();
    let avg_fps = frame_count as f64 / elapsed.as_secs_f64();
    let avg_latency = if frame_count > 0 {
        total_latency.as_secs_f64() * 1000.0 / frame_count as f64
    } else {
        0.0
    };

    println!("  性能测试结果:");
    println!("    - 总帧数: {}", frame_count);
    println!("    - 平均帧率: {:.2} FPS", avg_fps);
    println!("    - 平均延迟: {:.2}ms", avg_latency);
    println!("    - 最大延迟: {:.2}ms", max_latency.as_secs_f64() * 1000.0);

    // 性能评估
    if avg_fps >= 25.0 {
        println!("    ✓ 帧率性能: 优秀");
    } else if avg_fps >= 15.0 {
        println!("    ⚠ 帧率性能: 一般");
    } else {
        println!("    ✗ 帧率性能: 需要改进");
    }

    if avg_latency < 50.0 {
        println!("    ✓ 延迟性能: 优秀");
    } else if avg_latency < 100.0 {
        println!("    ⚠ 延迟性能: 一般");
    } else {
        println!("    ✗ 延迟性能: 需要改进");
    }

    core.stop()?;
    Ok(())
}