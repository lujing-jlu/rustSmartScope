//! SmartScope Core 时间戳同步测试
//!
//! 专门测试优化后的时间戳同步功能：
//! - 测试新的同步帧缓冲算法
//! - 验证时间戳对齐效果
//! - 评估同步质量改进
//!
//! 运行方式：
//! ```bash
//! RUST_LOG=info cargo run --example sync_test
//! ```

use smartscope_core::*;
use std::time::{Duration, Instant};

fn main() -> Result<(), SmartScopeError> {
    env_logger::init();
    println!("=== SmartScope Core 时间戳同步测试 ===");

    let mut core = SmartScopeCore::new()?;

    // 使用MJPEG格式的最高分辨率进行测试 (1280x720 @ 30FPS)
    let stream_config = SmartScopeStreamConfig {
        width: 1280,
        height: 720,
        format: ImageFormat::MJPEG,
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::None,
        params_dir: "/tmp/sync_test".to_string(),
        enable_caching: false,
    };

    core.initialize(stream_config, correction_config)?;

    let status = core.get_camera_status()?;
    println!(
        "相机状态: {:?} - {} 个相机",
        status.mode, status.camera_count
    );

    core.start()?;

    // 等待相机稳定
    println!("等待相机启动稳定...");
    std::thread::sleep(Duration::from_secs(2));

    // 测试1: 传统方法 - 分别获取左右帧
    println!("\n=== 测试1: 传统分别获取方法 ===");
    test_individual_frame_sync(&mut core, 10)?;

    // 测试2: 新的同步方法 - 获取同步帧对
    println!("\n=== 测试2: 新的同步帧对方法 ===");
    test_synchronized_frames(&mut core, 10)?;

    // 测试3: 长时间同步稳定性测试
    println!("\n=== 测试3: 长时间同步稳定性 ===");
    test_long_term_sync_stability(&mut core, Duration::from_secs(30))?;

    core.stop()?;
    println!("\n=== 同步测试完成 ===");

    Ok(())
}

fn test_individual_frame_sync(
    core: &mut SmartScopeCore,
    sample_count: u32,
) -> Result<(), SmartScopeError> {
    let mut timestamp_diffs = Vec::new();
    let mut successful_pairs = 0;

    println!("测试传统分别获取方法 ({} 对帧):", sample_count);

    for i in 1..=sample_count {
        let left_result = core.get_left_frame();
        let right_result = core.get_right_frame();

        match (left_result, right_result) {
            (Ok(left_frame), Ok(right_frame)) => {
                successful_pairs += 1;
                let time_diff = (left_frame.timestamp as i64 - right_frame.timestamp as i64).abs();
                timestamp_diffs.push(time_diff);

                println!(
                    "  帧对 {}: 左={}ms, 右={}ms, 差异={}ms",
                    i, left_frame.timestamp, right_frame.timestamp, time_diff
                );

                if time_diff > 100 {
                    println!("    ⚠ 时间戳差异较大");
                }
            }
            _ => println!("  帧对 {}: 获取失败", i),
        }

        std::thread::sleep(Duration::from_millis(100));
    }

    if !timestamp_diffs.is_empty() {
        let avg_diff = timestamp_diffs.iter().sum::<i64>() as f64 / timestamp_diffs.len() as f64;
        let min_diff = *timestamp_diffs.iter().min().unwrap();
        let max_diff = *timestamp_diffs.iter().max().unwrap();

        println!("传统方法统计:");
        println!(
            "  成功率: {:.1}%",
            successful_pairs as f64 / sample_count as f64 * 100.0
        );
        println!("  平均时间差: {:.2}ms", avg_diff);
        println!("  最小时间差: {}ms", min_diff);
        println!("  最大时间差: {}ms", max_diff);

        if avg_diff < 50.0 {
            println!("  ✓ 同步质量: 优秀");
        } else if avg_diff < 100.0 {
            println!("  ⚠ 同步质量: 一般");
        } else {
            println!("  ✗ 同步质量: 需要改进");
        }
    }

    Ok(())
}

fn test_synchronized_frames(
    core: &mut SmartScopeCore,
    sample_count: u32,
) -> Result<(), SmartScopeError> {
    let mut timestamp_diffs = Vec::new();
    let mut successful_pairs = 0;
    let mut total_attempts = 0;

    println!("测试新的同步帧对方法 ({} 对帧):", sample_count);

    for i in 1..=sample_count {
        total_attempts += 1;

        match core.get_synchronized_frames() {
            Ok(Some((left_frame, right_frame))) => {
                successful_pairs += 1;
                let time_diff = (left_frame.timestamp as i64 - right_frame.timestamp as i64).abs();
                timestamp_diffs.push(time_diff);

                println!(
                    "  同步帧对 {}: 左={}ms, 右={}ms, 差异={}ms",
                    i, left_frame.timestamp, right_frame.timestamp, time_diff
                );

                if time_diff > 50 {
                    println!("    ⚠ 同步差异较大");
                } else if time_diff < 10 {
                    println!("    ✓ 优秀同步");
                }
            }
            Ok(None) => {
                println!("  同步帧对 {}: 暂无可用同步帧", i);
                // 重试一次
                std::thread::sleep(Duration::from_millis(50));
                if let Ok(Some((left_frame, right_frame))) = core.get_synchronized_frames() {
                    successful_pairs += 1;
                    let time_diff =
                        (left_frame.timestamp as i64 - right_frame.timestamp as i64).abs();
                    timestamp_diffs.push(time_diff);
                    println!("    重试成功: 差异={}ms", time_diff);
                }
            }
            Err(e) => println!("  同步帧对 {}: 错误 - {}", i, e),
        }

        std::thread::sleep(Duration::from_millis(100));
    }

    if !timestamp_diffs.is_empty() {
        let avg_diff = timestamp_diffs.iter().sum::<i64>() as f64 / timestamp_diffs.len() as f64;
        let min_diff = *timestamp_diffs.iter().min().unwrap();
        let max_diff = *timestamp_diffs.iter().max().unwrap();

        println!("同步方法统计:");
        println!(
            "  成功率: {:.1}%",
            successful_pairs as f64 / total_attempts as f64 * 100.0
        );
        println!("  平均时间差: {:.2}ms", avg_diff);
        println!("  最小时间差: {}ms", min_diff);
        println!("  最大时间差: {}ms", max_diff);

        // 计算改进效果
        if avg_diff < 20.0 {
            println!("  ✓ 同步质量: 优秀 (显著改进!)");
        } else if avg_diff < 50.0 {
            println!("  ✓ 同步质量: 良好 (有改进)");
        } else {
            println!("  ⚠ 同步质量: 一般");
        }
    }

    Ok(())
}

fn test_long_term_sync_stability(
    core: &mut SmartScopeCore,
    duration: Duration,
) -> Result<(), SmartScopeError> {
    println!("开始 {} 秒长时间稳定性测试...", duration.as_secs());

    let start_time = Instant::now();
    let mut sync_count = 0;
    let mut total_attempts = 0;
    let mut timestamp_diffs = Vec::new();
    let mut last_report = start_time;

    while start_time.elapsed() < duration {
        total_attempts += 1;

        if let Ok(Some((left_frame, right_frame))) = core.get_synchronized_frames() {
            sync_count += 1;
            let time_diff = (left_frame.timestamp as i64 - right_frame.timestamp as i64).abs();
            timestamp_diffs.push(time_diff);

            // 每10秒报告一次状态
            if last_report.elapsed() >= Duration::from_secs(10) {
                let elapsed = start_time.elapsed().as_secs();
                let current_avg = if !timestamp_diffs.is_empty() {
                    timestamp_diffs.iter().sum::<i64>() as f64 / timestamp_diffs.len() as f64
                } else {
                    0.0
                };
                let success_rate = sync_count as f64 / total_attempts as f64 * 100.0;

                println!(
                    "  {}s: 同步率={:.1}%, 平均差异={:.2}ms, 总同步帧={}",
                    elapsed, success_rate, current_avg, sync_count
                );
                last_report = Instant::now();
            }
        }

        std::thread::sleep(Duration::from_millis(50));
    }

    // 最终统计
    let success_rate = sync_count as f64 / total_attempts as f64 * 100.0;
    let avg_diff = if !timestamp_diffs.is_empty() {
        timestamp_diffs.iter().sum::<i64>() as f64 / timestamp_diffs.len() as f64
    } else {
        0.0
    };

    println!("\n长时间稳定性测试结果:");
    println!("  总尝试次数: {}", total_attempts);
    println!("  成功同步次数: {}", sync_count);
    println!("  同步成功率: {:.2}%", success_rate);
    println!("  平均时间差: {:.2}ms", avg_diff);

    if success_rate >= 90.0 && avg_diff < 30.0 {
        println!("  🎉 长时间稳定性: 优秀!");
    } else if success_rate >= 80.0 && avg_diff < 50.0 {
        println!("  ✓ 长时间稳定性: 良好");
    } else {
        println!("  ⚠ 长时间稳定性: 需要进一步优化");
    }

    Ok(())
}
