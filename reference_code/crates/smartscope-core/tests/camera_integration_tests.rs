//! SmartScope Core 真实相机集成测试
//!
//! 这些测试需要连接真实的USB相机设备才能运行

use smartscope_core::{
    SmartScopeCore, SmartScopeStreamConfig, SmartScopeCorrectionConfig,
    CameraMode, CorrectionType, ImageFormat
};
use std::time::{Duration, Instant};

/// 测试真实相机的检测和连接
///
/// 运行条件：需要连接至少一个USB相机
#[test]
#[ignore] // 默认忽略，需要手动运行：cargo test camera_detection -- --ignored
fn test_real_camera_detection() {
    // 检查系统中是否有可用的摄像头设备
    let video_devices = std::fs::read_dir("/dev")
        .unwrap()
        .filter_map(|entry| {
            let entry = entry.ok()?;
            let name = entry.file_name().to_string_lossy().to_string();
            if name.starts_with("video") {
                Some(format!("/dev/{}", name))
            } else {
                None
            }
        })
        .collect::<Vec<_>>();

    println!("发现的视频设备: {:?}", video_devices);
    assert!(!video_devices.is_empty(), "没有找到视频设备，请确保USB相机已连接");

    // TODO: 集成usb-camera crate来实际检测相机
    println!("相机检测测试通过 - 找到 {} 个视频设备", video_devices.len());
}

/// 测试真实相机的流配置和初始化
///
/// 运行条件：需要连接USB相机
#[test]
#[ignore] // 需要手动运行：cargo test real_camera_initialization -- --ignored
fn test_real_camera_initialization() {
    let mut core = SmartScopeCore::new()
        .expect("Failed to create SmartScopeCore");

    // 使用常见的相机配置
    let stream_config = SmartScopeStreamConfig {
        width: 640,
        height: 480,
        format: ImageFormat::MJPEG, // 大多数USB相机支持MJPEG
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::Distortion,
        params_dir: "/tmp/smartscope_params".to_string(),
        enable_caching: true,
    };

    // 初始化核心
    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize with real camera config");

    assert!(core.is_initialized());

    // 启动相机流
    core.start().expect("Failed to start camera stream");
    assert!(core.is_started());

    println!("真实相机初始化测试通过");

    // 清理
    core.stop().expect("Failed to stop camera stream");
}

/// 测试真实相机帧捕获性能
///
/// 运行条件：需要连接USB相机
#[test]
#[ignore] // 需要手动运行：cargo test real_camera_performance -- --ignored
fn test_real_camera_performance() {
    let mut core = SmartScopeCore::new()
        .expect("Failed to create SmartScopeCore");

    let stream_config = SmartScopeStreamConfig {
        width: 1280,
        height: 720,
        format: ImageFormat::RGB888,
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig::default();

    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize core");
    core.start().expect("Failed to start core");

    // 性能测试参数
    let test_duration = Duration::from_secs(10);
    let start_time = Instant::now();
    let mut frame_count = 0;
    let mut total_latency = Duration::ZERO;

    println!("开始 {} 秒的真实相机性能测试...", test_duration.as_secs());

    while start_time.elapsed() < test_duration {
        let frame_start = Instant::now();

        // 尝试获取左相机帧
        match core.get_left_frame() {
            Ok(frame_data) => {
                let latency = frame_start.elapsed();
                total_latency += latency;
                frame_count += 1;

                // 验证帧数据
                assert!(frame_data.width > 0);
                assert!(frame_data.height > 0);
                assert!(!frame_data.data.is_null());
                assert!(frame_data.size > 0);

                if frame_count % 30 == 0 {
                    println!("已捕获 {} 帧，平均延迟: {:.2}ms",
                        frame_count,
                        total_latency.as_secs_f64() * 1000.0 / frame_count as f64
                    );
                }
            }
            Err(e) => {
                eprintln!("获取帧失败: {}", e);
                // 短暂等待后重试
                std::thread::sleep(Duration::from_millis(10));
            }
        }

        // 控制帧率，避免过度CPU占用
        std::thread::sleep(Duration::from_millis(30));
    }

    let elapsed = start_time.elapsed();
    let actual_fps = frame_count as f64 / elapsed.as_secs_f64();
    let avg_latency = total_latency.as_secs_f64() * 1000.0 / frame_count as f64;

    println!("性能测试结果:");
    println!("  总帧数: {}", frame_count);
    println!("  测试时长: {:.2}s", elapsed.as_secs_f64());
    println!("  实际帧率: {:.2} FPS", actual_fps);
    println!("  平均延迟: {:.2}ms", avg_latency);

    // 性能断言
    assert!(frame_count > 0, "没有成功捕获任何帧");
    assert!(actual_fps >= 10.0, "帧率过低: {:.2} FPS", actual_fps);
    assert!(avg_latency < 100.0, "平均延迟过高: {:.2}ms", avg_latency);

    core.stop().expect("Failed to stop core");
}

/// 测试立体相机模式
///
/// 运行条件：需要连接两个USB相机
#[test]
#[ignore] // 需要手动运行：cargo test stereo_camera_mode -- --ignored
fn test_stereo_camera_mode() {
    let mut core = SmartScopeCore::new()
        .expect("Failed to create SmartScopeCore");

    let stream_config = SmartScopeStreamConfig {
        width: 640,
        height: 480,
        format: ImageFormat::RGB888,
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::Stereo,
        params_dir: "./test_data/stereo_params".to_string(),
        enable_caching: true,
    };

    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize stereo camera");
    core.start().expect("Failed to start stereo camera");

    // 检查相机状态
    let status = core.get_camera_status()
        .expect("Failed to get camera status");

    println!("立体相机状态:");
    println!("  模式: {:?}", status.mode);
    println!("  相机数量: {}", status.camera_count);
    println!("  左相机连接: {}", status.left_connected);
    println!("  右相机连接: {}", status.right_connected);

    // 对于立体相机模式，期望有两个相机
    if status.mode == CameraMode::StereoCamera {
        assert!(status.left_connected, "左相机应该已连接");
        assert!(status.right_connected, "右相机应该已连接");
        assert_eq!(status.camera_count, 2, "应该检测到2个相机");
    }

    // 测试同时获取左右帧
    let mut sync_frame_count = 0;

    for _ in 0..10 {
        let left_result = core.get_left_frame();
        let right_result = core.get_right_frame();

        match (left_result, right_result) {
            (Ok(left_frame), Ok(right_frame)) => {
                sync_frame_count += 1;

                // 验证帧数据一致性
                assert_eq!(left_frame.width, right_frame.width);
                assert_eq!(left_frame.height, right_frame.height);
                assert_eq!(left_frame.format, right_frame.format);

                // 时间戳差异应该很小（同步捕获）
                let time_diff = (left_frame.timestamp as i64 - right_frame.timestamp as i64).abs();
                assert!(time_diff < 50, "左右帧时间戳差异过大: {}ms", time_diff);

                println!("成功获取同步帧对 {}/{}", sync_frame_count, 10);
            }
            (Err(e), _) => eprintln!("获取左帧失败: {}", e),
            (_, Err(e)) => eprintln!("获取右帧失败: {}", e),
        }

        std::thread::sleep(Duration::from_millis(100));
    }

    println!("立体相机测试完成，成功获取 {} 对同步帧", sync_frame_count);
    assert!(sync_frame_count > 0, "没有成功获取任何同步帧对");

    core.stop().expect("Failed to stop stereo camera");
}

/// 测试不同图像格式支持
///
/// 运行条件：需要连接支持多格式的USB相机
#[test]
#[ignore] // 需要手动运行：cargo test image_formats -- --ignored
fn test_different_image_formats() {
    let formats_to_test = vec![
        (ImageFormat::MJPEG, "MJPEG"),
        (ImageFormat::RGB888, "RGB888"),
        (ImageFormat::YUYV, "YUYV"),
    ];

    for (format, format_name) in formats_to_test {
        println!("测试图像格式: {}", format_name);

        let mut core = SmartScopeCore::new()
            .expect("Failed to create SmartScopeCore");

        let stream_config = SmartScopeStreamConfig {
            width: 640,
            height: 480,
            format,
            fps: 30,
            read_interval_ms: 33,
        };

        let correction_config = SmartScopeCorrectionConfig::default();

        match core.initialize(stream_config, correction_config) {
            Ok(_) => {
                println!("  {} 格式初始化成功", format_name);

                if core.start().is_ok() {
                    // 尝试获取几帧数据
                    for i in 0..3 {
                        match core.get_left_frame() {
                            Ok(frame) => {
                                assert_eq!(frame.format, format);
                                println!("  成功获取第 {} 帧，格式: {:?}", i + 1, frame.format);
                            }
                            Err(e) => {
                                eprintln!("  获取第 {} 帧失败: {}", i + 1, e);
                                break;
                            }
                        }
                        std::thread::sleep(Duration::from_millis(50));
                    }

                    let _ = core.stop();
                } else {
                    eprintln!("  {} 格式启动失败", format_name);
                }
            }
            Err(e) => {
                eprintln!("  {} 格式初始化失败: {}", format_name, e);
            }
        }
    }
}

/// 相机稳定性长时间测试
///
/// 运行条件：需要连接USB相机
#[test]
#[ignore] // 需要手动运行：cargo test camera_stability -- --ignored
fn test_camera_stability() {
    let mut core = SmartScopeCore::new()
        .expect("Failed to create SmartScopeCore");

    let stream_config = SmartScopeStreamConfig::default();
    let correction_config = SmartScopeCorrectionConfig::default();

    core.initialize(stream_config, correction_config)
        .expect("Failed to initialize core");
    core.start().expect("Failed to start core");

    println!("开始 60 秒稳定性测试...");

    let test_duration = Duration::from_secs(60);
    let start_time = Instant::now();
    let mut successful_frames = 0;
    let mut failed_frames = 0;
    let mut last_report = Instant::now();

    while start_time.elapsed() < test_duration {
        match core.get_left_frame() {
            Ok(_) => successful_frames += 1,
            Err(e) => {
                failed_frames += 1;
                if failed_frames % 10 == 0 {
                    eprintln!("连续失败帧数达到: {}, 最新错误: {}", failed_frames, e);
                }
            }
        }

        // 每10秒报告一次状态
        if last_report.elapsed() >= Duration::from_secs(10) {
            let elapsed = start_time.elapsed().as_secs();
            println!("{}s - 成功: {}, 失败: {}, 成功率: {:.1}%",
                elapsed,
                successful_frames,
                failed_frames,
                successful_frames as f64 / (successful_frames + failed_frames) as f64 * 100.0
            );
            last_report = Instant::now();
        }

        std::thread::sleep(Duration::from_millis(30));
    }

    let total_frames = successful_frames + failed_frames;
    let success_rate = successful_frames as f64 / total_frames as f64 * 100.0;

    println!("稳定性测试完成:");
    println!("  总帧数: {}", total_frames);
    println!("  成功帧数: {}", successful_frames);
    println!("  失败帧数: {}", failed_frames);
    println!("  成功率: {:.2}%", success_rate);

    // 稳定性要求：成功率应该至少95%
    assert!(success_rate >= 95.0, "相机稳定性不足: {:.2}%", success_rate);
    assert!(total_frames > 100, "测试帧数过少: {}", total_frames);

    core.stop().expect("Failed to stop core");
}

/// 辅助函数：打印系统相机信息
#[allow(dead_code)]
fn print_system_camera_info() {
    println!("系统相机信息:");

    // 列出所有视频设备
    if let Ok(entries) = std::fs::read_dir("/dev") {
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

        println!("  发现的视频设备: {:?}", video_devices);
    }

    // 检查v4l2设备信息（如果可用）
    for i in 0..4 {
        let device_path = format!("/dev/video{}", i);
        if std::path::Path::new(&device_path).exists() {
            println!("  设备 {} 存在", device_path);

            // 尝试获取设备信息（需要v4l2工具）
            if let Ok(output) = std::process::Command::new("v4l2-ctl")
                .args(&["--device", &device_path, "--info"])
                .output()
            {
                if output.status.success() {
                    println!("    设备信息: {}", String::from_utf8_lossy(&output.stdout));
                }
            }
        }
    }
}

/// 运行所有真实相机测试的便捷函数
#[allow(dead_code)]
fn run_all_camera_tests() {
    println!("运行所有真实相机测试...");
    print_system_camera_info();

    // 这些测试需要手动运行：
    // cargo test camera_detection -- --ignored
    // cargo test real_camera_initialization -- --ignored
    // cargo test real_camera_performance -- --ignored
    // cargo test stereo_camera_mode -- --ignored
    // cargo test image_formats -- --ignored
    // cargo test camera_stability -- --ignored
}