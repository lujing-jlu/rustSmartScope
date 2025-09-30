//! SmartScope Core 实时相机模式切换测试
//!
//! 测试三种相机模式的实时自动切换：
//! - NoCamera（无相机）
//! - SingleCamera（单相机）
//! - StereoCamera（立体相机）
//!
//! 运行方式：
//! ```bash
//! RUST_LOG=info cargo run --example mode_switching_test
//! ```

use smartscope_core::*;
use std::time::Duration;

fn main() -> Result<(), SmartScopeError> {
    env_logger::init();
    println!("=== SmartScope Core 实时相机模式切换测试 ===");

    let mut core = SmartScopeCore::new()?;

    // 使用默认配置初始化
    let stream_config = SmartScopeStreamConfig::default();
    let correction_config = SmartScopeCorrectionConfig::default();

    core.initialize(stream_config, correction_config)?;
    println!("SmartScope Core 初始化完成");

    // 获取初始状态
    let initial_status = core.get_camera_status()?;
    println!(
        "初始相机状态: {:?} - {} 个相机",
        initial_status.mode, initial_status.camera_count
    );

    // 启动核心（包括相机监控）
    core.start()?;
    println!("SmartScope Core 已启动，相机监控已开始");

    // 实时监控模式变化（持续30秒）
    let monitoring_duration = Duration::from_secs(30);
    let start_time = std::time::Instant::now();
    let mut last_mode = initial_status.mode;
    let mut mode_change_count = 0;

    println!(
        "\n=== 开始 {} 秒实时模式监控 ===",
        monitoring_duration.as_secs()
    );
    println!("请在测试期间连接/断开相机观察模式切换效果...");

    while start_time.elapsed() < monitoring_duration {
        // 检查当前状态
        match core.get_camera_status() {
            Ok(current_status) => {
                // 检查模式是否发生变化
                if current_status.mode != last_mode {
                    mode_change_count += 1;
                    println!(
                        "\n🔄 模式变化检测 #{}: {:?} -> {:?}",
                        mode_change_count, last_mode, current_status.mode
                    );
                    println!(
                        "   相机数量: {} -> {}",
                        get_camera_count_for_mode(last_mode),
                        current_status.camera_count
                    );
                    println!(
                        "   左相机: {}, 右相机: {}",
                        if current_status.left_connected {
                            "已连接"
                        } else {
                            "未连接"
                        },
                        if current_status.right_connected {
                            "已连接"
                        } else {
                            "未连接"
                        }
                    );

                    // 处理模式变化（重新配置相机读取器）
                    println!("   ℹ️ 模式变化已检测到，相机配置将自动更新");

                    last_mode = current_status.mode;
                }

                // 每5秒显示一次当前状态
                if start_time.elapsed().as_secs() % 5 == 0
                    && start_time.elapsed().as_millis() % 5000 < 100
                {
                    println!(
                        "⏱️  {}s: 模式={:?}, 相机数={}, 模式变化次数={}",
                        start_time.elapsed().as_secs(),
                        current_status.mode,
                        current_status.camera_count,
                        mode_change_count
                    );
                }
            }
            Err(e) => {
                println!("❌ 获取相机状态失败: {}", e);
            }
        }

        // 检查实时状态更新（非阻塞）
        // 注释：实时监控功能在当前版本中实现

        // 短暂休眠避免CPU占用过高
        std::thread::sleep(Duration::from_millis(100));
    }

    println!("\n=== 测试结果统计 ===");
    println!("总监控时间: {} 秒", monitoring_duration.as_secs());
    println!("模式变化次数: {}", mode_change_count);

    let final_status = core.get_camera_status()?;
    println!(
        "最终模式: {:?} - {} 个相机",
        final_status.mode, final_status.camera_count
    );

    if mode_change_count > 0 {
        println!("🎉 实时模式切换功能正常工作!");
    } else {
        println!("ℹ️  测试期间无模式变化（请尝试连接/断开相机）");
    }

    // 测试帧获取在不同模式下的表现
    test_frame_access_in_current_mode(&mut core)?;

    core.stop()?;
    println!("SmartScope Core 已停止");

    Ok(())
}

fn get_camera_count_for_mode(mode: CameraMode) -> u32 {
    match mode {
        CameraMode::NoCamera => 0,
        CameraMode::SingleCamera => 1,
        CameraMode::StereoCamera => 2,
    }
}

fn test_frame_access_in_current_mode(core: &mut SmartScopeCore) -> Result<(), SmartScopeError> {
    println!("\n=== 测试当前模式下的帧访问 ===");

    let status = core.get_camera_status()?;
    println!("当前模式: {:?}", status.mode);

    match status.mode {
        CameraMode::NoCamera => {
            println!("无相机模式 - 帧访问应该失败");
            match core.get_left_frame() {
                Err(_) => println!("✅ 左帧访问正确失败"),
                Ok(_) => println!("❌ 左帧访问意外成功"),
            }
        }
        CameraMode::SingleCamera => {
            println!("单相机模式 - 尝试获取帧");
            match core.get_left_frame() {
                Ok(frame) => println!(
                    "✅ 成功获取帧: {}x{}, {} bytes",
                    frame.width, frame.height, frame.size
                ),
                Err(e) => println!("❌ 获取帧失败: {}", e),
            }
        }
        CameraMode::StereoCamera => {
            println!("立体相机模式 - 尝试获取同步帧");
            match core.get_synchronized_frames() {
                Ok(Some((left, right))) => {
                    let time_diff = (left.timestamp as i64 - right.timestamp as i64).abs();
                    println!(
                        "✅ 成功获取同步帧对: {}x{}, 时间差={}ms",
                        left.width, left.height, time_diff
                    );
                }
                Ok(None) => println!("ℹ️ 暂无可用同步帧"),
                Err(e) => println!("❌ 获取同步帧失败: {}", e),
            }
        }
    }

    Ok(())
}
