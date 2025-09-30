//! SmartScope Core 分辨率检测测试
//!
//! 检测相机支持的所有MJPEG分辨率，找到最高分辨率
//!
//! 运行方式：
//! ```bash
//! RUST_LOG=debug cargo run --example resolution_test
//! ```

use smartscope_core::*;

fn main() -> Result<(), SmartScopeError> {
    env_logger::init();
    println!("=== SmartScope Core 分辨率检测 ===");

    let mut core = SmartScopeCore::new()?;

    // 常见的高分辨率配置进行测试
    let test_resolutions = [
        (1920, 1080), // 1080p
        (1280, 720),  // 720p
        (1024, 768),  // XGA
        (800, 600),   // SVGA
        (640, 480),   // VGA
    ];

    println!("正在测试MJPEG格式支持的分辨率:");

    for (width, height) in test_resolutions.iter() {
        println!("\n测试分辨率: {}×{}", width, height);

        let stream_config = SmartScopeStreamConfig {
            width: *width,
            height: *height,
            format: ImageFormat::MJPEG,
            fps: 30,
            read_interval_ms: 33,
        };

        let correction_config = SmartScopeCorrectionConfig {
            correction_type: CorrectionType::None,
            params_dir: "/tmp/resolution_test".to_string(),
            enable_caching: false,
        };

        match core.initialize(stream_config.clone(), correction_config.clone()) {
            Ok(_) => {
                println!("  ✓ {}×{} 初始化成功", width, height);

                match core.start() {
                    Ok(_) => {
                        println!("  ✓ {}×{} 启动成功", width, height);

                        // 等待启动
                        std::thread::sleep(std::time::Duration::from_secs(1));

                        // 尝试获取帧
                        match core.get_left_frame() {
                            Ok(frame) => {
                                println!("  ✓ {}×{} 成功获取帧: 实际尺寸={}×{}, 数据大小={}KB",
                                    width, height, frame.width, frame.height, frame.size / 1024);

                                // 计算压缩比
                                let raw_size = (frame.width * frame.height * 3) as usize;
                                let compression_ratio = frame.size as f64 / raw_size as f64 * 100.0;
                                println!("    MJPEG压缩比: {:.1}%", compression_ratio);
                            }
                            Err(e) => println!("  ✗ {}×{} 获取帧失败: {}", width, height, e),
                        }

                        core.stop().ok();
                    }
                    Err(e) => println!("  ✗ {}×{} 启动失败: {}", width, height, e),
                }
            }
            Err(e) => println!("  ✗ {}×{} 初始化失败: {}", width, height, e),
        }

        // 清理状态
        std::thread::sleep(std::time::Duration::from_millis(500));
    }

    println!("\n=== 分辨率检测完成 ===");
    Ok(())
}