//! SmartScope Core 立体相机演示
//!
//! 这个示例专门展示立体相机功能：
//! - 双相机同步捕获
//! - 立体校正
//! - 时间戳同步验证
//! - 深度感知处理
//!
//! 运行方式：
//! ```bash
//! cargo run --example stereo_camera_demo
//! ```

use smartscope_core::*;
use std::time::{Duration, Instant};

fn main() -> Result<(), SmartScopeError> {
    println!("=== SmartScope Core 立体相机演示 ===");

    let mut core = SmartScopeCore::new()?;

    // 立体相机配置
    let stream_config = SmartScopeStreamConfig {
        width: 1280,
        height: 720,
        format: ImageFormat::RGB888,
        fps: 30,
        read_interval_ms: 33,
    };

    // 启用立体校正
    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::Stereo,
        params_dir: "./stereo_calibration".to_string(),
        enable_caching: true,
    };

    // 初始化和启动
    core.initialize(stream_config, correction_config)?;
    core.start()?;

    // 检查立体相机状态
    let status = core.get_camera_status()?;
    println!("立体相机状态:");
    println!("  模式: {:?}", status.mode);
    println!("  相机数量: {}", status.camera_count);
    println!("  左相机连接: {}", status.left_connected);
    println!("  右相机连接: {}", status.right_connected);

    if status.mode != CameraMode::StereoCamera {
        println!("警告: 系统未检测到立体相机模式");
        if !status.right_connected {
            println!("请确保连接了两个USB相机");
        }
    }

    // 立体同步测试
    println!("\n开始立体同步测试...");
    test_stereo_synchronization(&mut core, 20)?;

    // 时间戳分析
    println!("\n分析时间戳同步性...");
    analyze_timestamp_sync(&mut core, 10)?;

    // 立体校正效果测试
    println!("\n测试立体校正效果...");
    test_stereo_correction(&mut core)?;

    // 双相机帧率对比
    println!("\n双相机帧率对比测试...");
    compare_camera_framerates(&mut core, Duration::from_secs(5))?;

    core.stop()?;
    println!("\n✓ 立体相机演示完成");

    Ok(())
}

fn test_stereo_synchronization(
    core: &mut SmartScopeCore,
    frame_count: u32,
) -> Result<(), SmartScopeError> {
    let mut successful_pairs = 0;
    let mut timestamp_diffs = Vec::new();

    for i in 1..=frame_count {
        let left_result = core.get_left_frame();
        let right_result = core.get_right_frame();

        match (left_result, right_result) {
            (Ok(left_frame), Ok(right_frame)) => {
                successful_pairs += 1;

                // 计算时间戳差异
                let timestamp_diff = (left_frame.timestamp as i64 - right_frame.timestamp as i64).abs();
                timestamp_diffs.push(timestamp_diff);

                println!("帧对 {}: 左相机 {}ms, 右相机 {}ms, 差异 {}ms",
                    i, left_frame.timestamp, right_frame.timestamp, timestamp_diff);

                // 验证帧属性一致性
                if left_frame.width != right_frame.width || left_frame.height != right_frame.height {
                    println!("  警告: 左右帧尺寸不一致");
                }

                if left_frame.format != right_frame.format {
                    println!("  警告: 左右帧格式不一致");
                }

                // 检查同步质量
                if timestamp_diff > 50 {
                    println!("  警告: 时间戳差异过大 ({}ms)", timestamp_diff);
                }
            }
            (Err(e), _) => println!("帧对 {}: 左相机获取失败 - {}", i, e),
            (_, Err(e)) => println!("帧对 {}: 右相机获取失败 - {}", i, e),
        }

        std::thread::sleep(Duration::from_millis(100));
    }

    // 同步性统计
    let success_rate = successful_pairs as f64 / frame_count as f64 * 100.0;
    println!("\n同步测试结果:");
    println!("  成功帧对: {}/{}", successful_pairs, frame_count);
    println!("  成功率: {:.2}%", success_rate);

    if !timestamp_diffs.is_empty() {
        let avg_diff = timestamp_diffs.iter().sum::<i64>() as f64 / timestamp_diffs.len() as f64;
        let max_diff = *timestamp_diffs.iter().max().unwrap();
        let min_diff = *timestamp_diffs.iter().min().unwrap();

        println!("  平均时间戳差异: {:.2}ms", avg_diff);
        println!("  最小时间戳差异: {}ms", min_diff);
        println!("  最大时间戳差异: {}ms", max_diff);

        // 评估同步质量
        if avg_diff < 20.0 {
            println!("  ✓ 同步质量: 优秀");
        } else if avg_diff < 50.0 {
            println!("  ⚠ 同步质量: 良好");
        } else {
            println!("  ✗ 同步质量: 需要改进");
        }
    }

    Ok(())
}

fn analyze_timestamp_sync(
    core: &mut SmartScopeCore,
    samples: u32,
) -> Result<(), SmartScopeError> {
    let mut left_timestamps = Vec::new();
    let mut right_timestamps = Vec::new();

    // 收集时间戳样本
    for _ in 0..samples {
        if let (Ok(left_frame), Ok(right_frame)) = (core.get_left_frame(), core.get_right_frame()) {
            left_timestamps.push(left_frame.timestamp);
            right_timestamps.push(right_frame.timestamp);
        }
        std::thread::sleep(Duration::from_millis(50));
    }

    if left_timestamps.len() < 2 || right_timestamps.len() < 2 {
        println!("时间戳样本不足，无法分析");
        return Ok(());
    }

    // 分析时间戳稳定性
    let left_intervals: Vec<u64> = left_timestamps.windows(2)
        .map(|w| w[1] - w[0])
        .collect();

    let right_intervals: Vec<u64> = right_timestamps.windows(2)
        .map(|w| w[1] - w[0])
        .collect();

    let avg_left_interval = left_intervals.iter().sum::<u64>() as f64 / left_intervals.len() as f64;
    let avg_right_interval = right_intervals.iter().sum::<u64>() as f64 / right_intervals.len() as f64;

    println!("时间戳分析结果:");
    println!("  左相机平均帧间隔: {:.2}ms", avg_left_interval);
    println!("  右相机平均帧间隔: {:.2}ms", avg_right_interval);
    println!("  理论帧间隔 (30fps): 33.33ms");

    // 检查帧率稳定性
    let left_stability = calculate_stability(&left_intervals, avg_left_interval);
    let right_stability = calculate_stability(&right_intervals, avg_right_interval);

    println!("  左相机稳定性: {:.2}%", left_stability);
    println!("  右相机稳定性: {:.2}%", right_stability);

    Ok(())
}

fn calculate_stability(intervals: &[u64], avg: f64) -> f64 {
    if intervals.is_empty() {
        return 0.0;
    }

    let variance: f64 = intervals.iter()
        .map(|&x| (x as f64 - avg).powi(2))
        .sum::<f64>() / intervals.len() as f64;

    let std_dev = variance.sqrt();
    let cv = std_dev / avg; // 变异系数

    // 转换为稳定性百分比 (较低的变异系数 = 较高的稳定性)
    ((1.0 - cv.min(1.0)) * 100.0).max(0.0)
}

fn test_stereo_correction(core: &mut SmartScopeCore) -> Result<(), SmartScopeError> {
    // 测试不同的校正模式
    let correction_types = [
        (CorrectionType::None, "无校正"),
        (CorrectionType::Distortion, "畸变校正"),
        (CorrectionType::Stereo, "立体校正"),
        (CorrectionType::Both, "完整校正"),
    ];

    for (correction_type, description) in &correction_types {
        println!("测试 {}: ", description);

        match core.set_correction(*correction_type) {
            Ok(_) => {
                println!("  ✓ 校正设置成功");

                // 获取一帧测试校正效果
                match core.get_left_frame() {
                    Ok(frame) => {
                        println!("  ✓ 校正后帧获取成功");
                        println!("    - 尺寸: {}×{}", frame.width, frame.height);
                        println!("    - 数据大小: {} 字节", frame.size);
                    }
                    Err(e) => {
                        println!("  ✗ 校正后帧获取失败: {}", e);
                    }
                }
            }
            Err(e) => {
                println!("  ✗ 校正设置失败: {}", e);
            }
        }

        std::thread::sleep(Duration::from_millis(100));
    }

    Ok(())
}

fn compare_camera_framerates(
    core: &mut SmartScopeCore,
    duration: Duration,
) -> Result<(), SmartScopeError> {
    let start_time = Instant::now();
    let mut left_frames = 0;
    let mut right_frames = 0;
    let mut last_report = start_time;

    println!("开始 {} 秒的帧率对比测试...", duration.as_secs());

    while start_time.elapsed() < duration {
        // 测试左相机
        if core.get_left_frame().is_ok() {
            left_frames += 1;
        }

        // 测试右相机
        if core.get_right_frame().is_ok() {
            right_frames += 1;
        }

        // 每秒报告进度
        if last_report.elapsed() >= Duration::from_secs(1) {
            let elapsed = start_time.elapsed().as_secs_f64();
            let left_fps = left_frames as f64 / elapsed;
            let right_fps = right_frames as f64 / elapsed;

            println!("  {:.0}s - 左: {:.1} FPS, 右: {:.1} FPS",
                elapsed, left_fps, right_fps);
            last_report = Instant::now();
        }

        std::thread::sleep(Duration::from_millis(10));
    }

    let elapsed = start_time.elapsed().as_secs_f64();
    let left_fps = left_frames as f64 / elapsed;
    let right_fps = right_frames as f64 / elapsed;

    println!("\n帧率对比结果:");
    println!("  左相机: {} 帧 ({:.2} FPS)", left_frames, left_fps);
    println!("  右相机: {} 帧 ({:.2} FPS)", right_frames, right_fps);
    println!("  帧率差异: {:.2} FPS", (left_fps - right_fps).abs());

    if (left_fps - right_fps).abs() < 2.0 {
        println!("  ✓ 双相机帧率平衡良好");
    } else {
        println!("  ⚠ 双相机帧率存在较大差异");
    }

    Ok(())
}