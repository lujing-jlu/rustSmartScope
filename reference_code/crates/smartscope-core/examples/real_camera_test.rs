//! SmartScope Core 真实相机专用测试
//!
//! 这个示例专门测试真实相机的连接和数据读取：
//! - 强制使用真实相机
//! - 详细的日志输出
//! - 逐步诊断过程
//!
//! 运行方式：
//! ```bash
//! RUST_LOG=debug cargo run --example real_camera_test
//! ```

use smartscope_core::*;
use std::time::{Duration, Instant};

fn main() -> Result<(), SmartScopeError> {
    env_logger::init();
    println!("=== SmartScope Core 真实相机测试 ===");

    // 第一步：创建核心实例
    println!("\n步骤1: 创建核心实例");
    let mut core = SmartScopeCore::new()?;
    println!("✓ 成功创建SmartScope核心实例");

    // 第二步：配置相机参数 - 使用最高分辨率MJPEG设置
    println!("\n步骤2: 配置相机参数");
    let stream_config = SmartScopeStreamConfig {
        width: 1920,
        height: 1080,
        format: ImageFormat::MJPEG,
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::None,
        params_dir: "/tmp/smartscope_test".to_string(),
        enable_caching: false,
    };

    println!("配置详情:");
    println!("  - 分辨率: {}×{}", stream_config.width, stream_config.height);
    println!("  - 格式: {:?}", stream_config.format);
    println!("  - 帧率: {} FPS", stream_config.fps);

    // 第三步：初始化
    println!("\n步骤3: 初始化系统");
    core.initialize(stream_config, correction_config)?;
    println!("✓ 系统初始化成功");

    // 第四步：检查相机状态
    println!("\n步骤4: 检查相机状态");
    let status = core.get_camera_status()?;
    println!("相机状态:");
    println!("  - 模式: {:?}", status.mode);
    println!("  - 相机数量: {}", status.camera_count);
    println!("  - 左相机连接: {}", status.left_connected);
    println!("  - 右相机连接: {}", status.right_connected);

    // 第五步：启动相机流
    println!("\n步骤5: 启动相机流");
    match core.start() {
        Ok(_) => println!("✓ 相机流启动成功"),
        Err(e) => {
            println!("✗ 相机流启动失败: {}", e);
            println!("可能原因: 相机设备被占用或权限不足");
            return Err(e);
        }
    }

    // 等待相机初始化
    println!("\n等待相机完全启动...");
    std::thread::sleep(Duration::from_secs(2));

    // 第六步：尝试获取帧数据
    println!("\n步骤6: 尝试获取真实相机帧数据");
    let mut success_count = 0;
    let mut failure_count = 0;
    let test_duration = Duration::from_secs(10);
    let start_time = Instant::now();

    println!("开始 {} 秒的帧捕获测试...", test_duration.as_secs());

    while start_time.elapsed() < test_duration && success_count < 5 {
        match core.get_left_frame() {
            Ok(frame_data) => {
                success_count += 1;
                println!("✓ 成功获取第 {} 帧:", success_count);
                println!("  - 尺寸: {}×{}", frame_data.width, frame_data.height);
                println!("  - 格式: {:?}", frame_data.format);
                println!("  - 数据大小: {} 字节", frame_data.size);
                println!("  - 相机类型: {:?}", frame_data.camera_type);
                println!("  - 时间戳: {}", frame_data.timestamp);

                // 计算压缩比（仅对MJPEG）
                if matches!(frame_data.format, ImageFormat::MJPEG) {
                    let raw_size = frame_data.width * frame_data.height * 3;
                    let compression_ratio = frame_data.size as f64 / raw_size as f64;
                    println!("  - MJPEG压缩比: {:.1}% ({} -> {} 字节)",
                        compression_ratio * 100.0, raw_size, frame_data.size);
                }

                // 验证数据完整性
                if frame_data.data.is_null() {
                    println!("  ⚠ 警告: 数据指针为空");
                } else if frame_data.size == 0 {
                    println!("  ⚠ 警告: 数据大小为0");
                } else {
                    println!("  ✓ 数据完整性检查通过");
                }

                println!();
            }
            Err(e) => {
                failure_count += 1;
                if failure_count <= 3 {
                    println!("✗ 获取帧失败 (第 {} 次): {}", failure_count, e);
                } else if failure_count % 100 == 0 {
                    println!("✗ 连续失败 {} 次", failure_count);
                }
            }
        }

        std::thread::sleep(Duration::from_millis(100));
    }

    // 第七步：测试结果统计
    println!("\n步骤7: 测试结果");
    let total_attempts = success_count + failure_count;
    let success_rate = if total_attempts > 0 {
        success_count as f64 / total_attempts as f64 * 100.0
    } else {
        0.0
    };

    println!("帧捕获统计:");
    println!("  - 成功次数: {}", success_count);
    println!("  - 失败次数: {}", failure_count);
    println!("  - 总尝试次数: {}", total_attempts);
    println!("  - 成功率: {:.1}%", success_rate);
    println!("  - 测试时长: {:.1}秒", start_time.elapsed().as_secs_f64());

    if success_count > 0 {
        println!("  ✓ 真实相机数据读取成功!");
    } else {
        println!("  ✗ 未能获取真实相机数据");
    }

    // 第八步：测试右相机（如果可用）
    if status.right_connected && success_count > 0 {
        println!("\n步骤8: 测试右相机");
        match core.get_right_frame() {
            Ok(frame_data) => {
                println!("✓ 右相机帧获取成功:");
                println!("  - 尺寸: {}×{}", frame_data.width, frame_data.height);
                println!("  - 格式: {:?}", frame_data.format);
                println!("  - 数据大小: {} 字节", frame_data.size);
            }
            Err(e) => {
                println!("✗ 右相机帧获取失败: {}", e);
            }
        }
    }

    // 第九步：停止相机流
    println!("\n步骤9: 停止相机流");
    core.stop()?;
    println!("✓ 相机流已停止");

    println!("\n=== 真实相机测试完成 ===");

    if success_count > 0 {
        println!("🎉 恭喜！真实相机集成成功！");
        println!("您的系统能够正常读取真实相机数据。");
    } else {
        println!("❌ 真实相机集成需要进一步调试");
        println!("建议检查:");
        println!("  1. USB相机是否正确连接");
        println!("  2. 设备权限是否正确 (sudo usermod -a -G video $USER)");
        println!("  3. 相机是否被其他程序占用");
        println!("  4. V4L2驱动是否正常工作");
    }

    Ok(())
}