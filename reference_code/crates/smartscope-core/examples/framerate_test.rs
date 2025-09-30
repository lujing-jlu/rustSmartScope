//! SmartScope Core 帧率测试
//!
//! 专门测试同步优化后的相机帧率：
//! - 测量左右相机的实际帧率
//! - 分析帧率稳定性
//! - 评估同步对性能的影响
//!
//! 运行方式：
//! ```bash
//! RUST_LOG=info cargo run --example framerate_test
//! ```

use smartscope_core::*;
use std::time::{Duration, Instant};

fn main() -> Result<(), SmartScopeError> {
    env_logger::init();
    println!("=== SmartScope Core 帧率测试 ===");

    let mut core = SmartScopeCore::new()?;

    // 使用相同的配置
    let stream_config = SmartScopeStreamConfig {
        width: 640,
        height: 480,
        format: ImageFormat::MJPEG,
        fps: 30,
        read_interval_ms: 33,
    };

    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::None,
        params_dir: "/tmp/framerate_test".to_string(),
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

    // 测试1: 左相机帧率
    println!("\n=== 测试1: 左相机帧率 ===");
    test_left_camera_framerate(&mut core, Duration::from_secs(10))?;

    // 测试2: 右相机帧率
    if status.right_connected {
        println!("\n=== 测试2: 右相机帧率 ===");
        test_right_camera_framerate(&mut core, Duration::from_secs(10))?;
    }

    // 测试3: 同步帧率
    println!("\n=== 测试3: 同步帧率 ===");
    test_synchronized_framerate(&mut core, Duration::from_secs(10))?;

    // 测试4: 并发测试 - 同时获取左右帧
    println!("\n=== 测试4: 并发帧率测试 ===");
    test_concurrent_framerate(&mut core, Duration::from_secs(10))?;

    core.stop()?;
    println!("\n=== 帧率测试完成 ===");

    Ok(())
}

fn test_left_camera_framerate(
    core: &mut SmartScopeCore,
    duration: Duration,
) -> Result<(), SmartScopeError> {
    let start_time = Instant::now();
    let mut frame_count = 0;
    let mut last_timestamp = 0u64;
    let mut frame_intervals = Vec::new();
    let mut failed_attempts = 0;

    println!("测试左相机帧率 {} 秒...", duration.as_secs());

    while start_time.elapsed() < duration {
        match core.get_left_frame() {
            Ok(frame) => {
                frame_count += 1;

                if last_timestamp != 0 {
                    let interval = frame.timestamp.saturating_sub(last_timestamp);
                    if interval > 0 && interval < 1000 {
                        // 排除异常间隔
                        frame_intervals.push(interval);
                    }
                }
                last_timestamp = frame.timestamp;
            }
            Err(_) => {
                failed_attempts += 1;
            }
        }

        std::thread::sleep(Duration::from_millis(10));
    }

    let elapsed = start_time.elapsed();
    let actual_fps = frame_count as f64 / elapsed.as_secs_f64();

    let avg_interval = if !frame_intervals.is_empty() {
        frame_intervals.iter().sum::<u64>() as f64 / frame_intervals.len() as f64
    } else {
        0.0
    };

    let fps_from_intervals = if avg_interval > 0.0 {
        1000.0 / avg_interval
    } else {
        0.0
    };

    println!("左相机结果:");
    println!("  总帧数: {}", frame_count);
    println!("  失败次数: {}", failed_attempts);
    println!("  测试时长: {:.2}s", elapsed.as_secs_f64());
    println!("  实际帧率: {:.2} FPS", actual_fps);
    println!("  平均帧间隔: {:.2}ms", avg_interval);
    println!("  基于间隔的帧率: {:.2} FPS", fps_from_intervals);

    if !frame_intervals.is_empty() {
        let min_interval = *frame_intervals.iter().min().unwrap();
        let max_interval = *frame_intervals.iter().max().unwrap();
        println!("  帧间隔范围: {}ms - {}ms", min_interval, max_interval);
    }

    Ok(())
}

fn test_right_camera_framerate(
    core: &mut SmartScopeCore,
    duration: Duration,
) -> Result<(), SmartScopeError> {
    let start_time = Instant::now();
    let mut frame_count = 0;
    let mut last_timestamp = 0u64;
    let mut frame_intervals = Vec::new();
    let mut failed_attempts = 0;

    println!("测试右相机帧率 {} 秒...", duration.as_secs());

    while start_time.elapsed() < duration {
        match core.get_right_frame() {
            Ok(frame) => {
                frame_count += 1;

                if last_timestamp != 0 {
                    let interval = frame.timestamp.saturating_sub(last_timestamp);
                    if interval > 0 && interval < 1000 {
                        // 排除异常间隔
                        frame_intervals.push(interval);
                    }
                }
                last_timestamp = frame.timestamp;
            }
            Err(_) => {
                failed_attempts += 1;
            }
        }

        std::thread::sleep(Duration::from_millis(10));
    }

    let elapsed = start_time.elapsed();
    let actual_fps = frame_count as f64 / elapsed.as_secs_f64();

    let avg_interval = if !frame_intervals.is_empty() {
        frame_intervals.iter().sum::<u64>() as f64 / frame_intervals.len() as f64
    } else {
        0.0
    };

    let fps_from_intervals = if avg_interval > 0.0 {
        1000.0 / avg_interval
    } else {
        0.0
    };

    println!("右相机结果:");
    println!("  总帧数: {}", frame_count);
    println!("  失败次数: {}", failed_attempts);
    println!("  测试时长: {:.2}s", elapsed.as_secs_f64());
    println!("  实际帧率: {:.2} FPS", actual_fps);
    println!("  平均帧间隔: {:.2}ms", avg_interval);
    println!("  基于间隔的帧率: {:.2} FPS", fps_from_intervals);

    if !frame_intervals.is_empty() {
        let min_interval = *frame_intervals.iter().min().unwrap();
        let max_interval = *frame_intervals.iter().max().unwrap();
        println!("  帧间隔范围: {}ms - {}ms", min_interval, max_interval);
    }

    Ok(())
}

fn test_synchronized_framerate(
    core: &mut SmartScopeCore,
    duration: Duration,
) -> Result<(), SmartScopeError> {
    let start_time = Instant::now();
    let mut sync_frame_count = 0;
    let mut failed_attempts = 0;
    let mut last_left_timestamp = 0u64;
    let mut last_right_timestamp = 0u64;
    let mut left_intervals = Vec::new();
    let mut right_intervals = Vec::new();
    let mut sync_quality = Vec::new();

    println!("测试同步帧率 {} 秒...", duration.as_secs());

    while start_time.elapsed() < duration {
        match core.get_synchronized_frames() {
            Ok(Some((left_frame, right_frame))) => {
                sync_frame_count += 1;

                // 计算同步质量
                let sync_diff = (left_frame.timestamp as i64 - right_frame.timestamp as i64).abs();
                sync_quality.push(sync_diff);

                // 计算左帧间隔
                if last_left_timestamp != 0 {
                    let interval = left_frame.timestamp.saturating_sub(last_left_timestamp);
                    if interval > 0 && interval < 1000 {
                        left_intervals.push(interval);
                    }
                }
                last_left_timestamp = left_frame.timestamp;

                // 计算右帧间隔
                if last_right_timestamp != 0 {
                    let interval = right_frame.timestamp.saturating_sub(last_right_timestamp);
                    if interval > 0 && interval < 1000 {
                        right_intervals.push(interval);
                    }
                }
                last_right_timestamp = right_frame.timestamp;
            }
            Ok(None) => {
                // 暂无可用同步帧
            }
            Err(_) => {
                failed_attempts += 1;
            }
        }

        std::thread::sleep(Duration::from_millis(20));
    }

    let elapsed = start_time.elapsed();
    let sync_fps = sync_frame_count as f64 / elapsed.as_secs_f64();

    let avg_sync_quality = if !sync_quality.is_empty() {
        sync_quality.iter().sum::<i64>() as f64 / sync_quality.len() as f64
    } else {
        0.0
    };

    let left_avg_interval = if !left_intervals.is_empty() {
        left_intervals.iter().sum::<u64>() as f64 / left_intervals.len() as f64
    } else {
        0.0
    };

    let right_avg_interval = if !right_intervals.is_empty() {
        right_intervals.iter().sum::<u64>() as f64 / right_intervals.len() as f64
    } else {
        0.0
    };

    println!("同步帧率结果:");
    println!("  同步帧对数: {}", sync_frame_count);
    println!("  失败次数: {}", failed_attempts);
    println!("  测试时长: {:.2}s", elapsed.as_secs_f64());
    println!("  同步帧率: {:.2} FPS", sync_fps);
    println!("  平均同步质量: {:.2}ms", avg_sync_quality);
    println!("  左帧平均间隔: {:.2}ms", left_avg_interval);
    println!("  右帧平均间隔: {:.2}ms", right_avg_interval);

    if sync_fps > 25.0 {
        println!("  ✓ 同步帧率: 优秀");
    } else if sync_fps > 15.0 {
        println!("  ⚠ 同步帧率: 一般");
    } else {
        println!("  ✗ 同步帧率: 需要优化");
    }

    Ok(())
}

fn test_concurrent_framerate(
    core: &mut SmartScopeCore,
    duration: Duration,
) -> Result<(), SmartScopeError> {
    let start_time = Instant::now();
    let mut left_count = 0;
    let mut right_count = 0;
    let mut concurrent_pairs = 0;

    println!("测试并发帧率 {} 秒...", duration.as_secs());

    while start_time.elapsed() < duration {
        let left_result = core.get_left_frame();
        let right_result = core.get_right_frame();

        match (&left_result, &right_result) {
            (Ok(_), Ok(_)) => concurrent_pairs += 1,
            _ => {}
        }

        if left_result.is_ok() {
            left_count += 1;
        }
        if right_result.is_ok() {
            right_count += 1;
        }

        std::thread::sleep(Duration::from_millis(15));
    }

    let elapsed = start_time.elapsed();
    let left_fps = left_count as f64 / elapsed.as_secs_f64();
    let right_fps = right_count as f64 / elapsed.as_secs_f64();
    let pair_fps = concurrent_pairs as f64 / elapsed.as_secs_f64();

    println!("并发测试结果:");
    println!("  左相机帧数: {} ({:.2} FPS)", left_count, left_fps);
    println!("  右相机帧数: {} ({:.2} FPS)", right_count, right_fps);
    println!("  并发帧对数: {} ({:.2} FPS)", concurrent_pairs, pair_fps);
    println!(
        "  并发成功率: {:.1}%",
        concurrent_pairs as f64 / left_count.min(right_count) as f64 * 100.0
    );

    Ok(())
}
