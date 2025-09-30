//! SmartScope Core 基础相机演示
//!
//! 这个示例展示如何使用 SmartScopeCore 进行基础的相机操作：
//! - 初始化相机系统
//! - 配置流参数
//! - 启动和停止相机
//! - 获取帧数据
//!
//! 运行方式：
//! ```bash
//! cargo run --example basic_camera_demo
//! ```

use smartscope_core::*;
use std::time::{Duration, Instant};

fn main() -> Result<(), SmartScopeError> {
    println!("=== SmartScope Core 基础相机演示 ===");

    // 创建核心实例
    let mut core = SmartScopeCore::new()?;
    println!("✓ 创建 SmartScopeCore 实例成功");

    // 配置流参数 - 使用最兼容的MJPEG格式
    let stream_config = SmartScopeStreamConfig {
        width: 640,
        height: 480,
        format: ImageFormat::MJPEG, // 大多数USB相机都支持MJPEG
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::None, // 不使用校正以简化演示
        params_dir: "/tmp/smartscope_demo".to_string(),
        enable_caching: false,
    };

    // 初始化
    core.initialize(stream_config, correction_config)?;
    println!("✓ 相机初始化成功");
    println!("  - 分辨率: 640×480");
    println!("  - 格式: RGB888");
    println!("  - 帧率: 30 FPS");

    // 检查相机状态
    let status = core.get_camera_status()?;
    println!("✓ 相机状态:");
    println!("  - 模式: {:?}", status.mode);
    println!("  - 相机数量: {}", status.camera_count);
    println!("  - 左相机连接: {}", status.left_connected);
    println!("  - 右相机连接: {}", status.right_connected);

    // 启动相机流
    core.start()?;
    println!("✓ 相机流启动成功");

    // 捕获几帧数据进行演示
    println!("\n开始捕获帧数据...");

    for i in 1..=5 {
        let frame_start = Instant::now();

        match core.get_left_frame() {
            Ok(frame_data) => {
                let capture_time = frame_start.elapsed();

                println!("第 {} 帧:", i);
                println!("  - 尺寸: {}×{}", frame_data.width, frame_data.height);
                println!("  - 格式: {:?}", frame_data.format);
                println!("  - 数据大小: {} 字节", frame_data.size);
                println!("  - 时间戳: {}", frame_data.timestamp);
                println!("  - 捕获延迟: {:.2}ms", capture_time.as_secs_f64() * 1000.0);

                // 验证数据完整性
                assert!(!frame_data.data.is_null(), "帧数据指针不应为空");
                assert!(frame_data.size > 0, "帧数据大小应该大于0");

                // 对于不同格式的数据大小验证
                match frame_data.format {
                    ImageFormat::RGB888 | ImageFormat::BGR888 => {
                        let expected_size = (frame_data.width * frame_data.height * 3) as usize;
                        assert_eq!(frame_data.size, expected_size, "RGB/BGR数据大小应该匹配");
                    }
                    ImageFormat::YUYV => {
                        let expected_size = (frame_data.width * frame_data.height * 2) as usize;
                        assert_eq!(frame_data.size, expected_size, "YUYV数据大小应该匹配");
                    }
                    ImageFormat::MJPEG => {
                        // MJPEG是压缩格式，数据大小是可变的，只检查非零即可
                        println!("  - MJPEG压缩帧，压缩比: {:.1}%",
                            frame_data.size as f64 / (frame_data.width * frame_data.height * 3) as f64 * 100.0);
                    }
                }
            }
            Err(e) => {
                eprintln!("获取第 {} 帧失败: {}", i, e);
            }
        }

        // 间隔100ms再获取下一帧
        std::thread::sleep(Duration::from_millis(100));
    }

    // 如果是立体相机模式，演示右相机
    if status.mode == CameraMode::StereoCamera && status.right_connected {
        println!("\n测试右相机...");

        match core.get_right_frame() {
            Ok(frame_data) => {
                println!("右相机帧:");
                println!("  - 尺寸: {}×{}", frame_data.width, frame_data.height);
                println!("  - 格式: {:?}", frame_data.format);
                println!("  - 数据大小: {} 字节", frame_data.size);
                println!("  - 时间戳: {}", frame_data.timestamp);
            }
            Err(e) => {
                eprintln!("获取右相机帧失败: {}", e);
            }
        }
    }

    // 演示校正功能
    println!("\n测试校正功能...");
    for correction_type in [CorrectionType::None, CorrectionType::Distortion] {
        match core.set_correction(correction_type) {
            Ok(_) => println!("✓ 设置校正类型为 {:?}", correction_type),
            Err(e) => eprintln!("✗ 设置校正类型失败: {}", e),
        }
    }

    // 停止相机流
    core.stop()?;
    println!("\n✓ 相机流已停止");

    println!("\n=== 演示完成 ===");
    Ok(())
}