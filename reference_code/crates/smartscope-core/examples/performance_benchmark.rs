//! SmartScope Core 性能基准测试
//!
//! 这个示例专门用于测试相机性能：
//! - 帧率基准测试
//! - 延迟分析
//! - 内存使用情况
//! - 长时间稳定性测试
//!
//! 运行方式：
//! ```bash
//! cargo run --example performance_benchmark
//! ```

use smartscope_core::*;
use std::time::{Duration, Instant};

struct PerformanceMetrics {
    total_frames: u64,
    successful_frames: u64,
    failed_frames: u64,
    total_latency: Duration,
    min_latency: Duration,
    max_latency: Duration,
    start_time: Instant,
}

impl PerformanceMetrics {
    fn new() -> Self {
        Self {
            total_frames: 0,
            successful_frames: 0,
            failed_frames: 0,
            total_latency: Duration::ZERO,
            min_latency: Duration::from_secs(u64::MAX),
            max_latency: Duration::ZERO,
            start_time: Instant::now(),
        }
    }

    fn record_success(&mut self, latency: Duration) {
        self.successful_frames += 1;
        self.total_frames += 1;
        self.total_latency += latency;

        if latency < self.min_latency {
            self.min_latency = latency;
        }
        if latency > self.max_latency {
            self.max_latency = latency;
        }
    }

    fn record_failure(&mut self) {
        self.failed_frames += 1;
        self.total_frames += 1;
    }

    fn print_report(&self) {
        let elapsed = self.start_time.elapsed();
        let success_rate = if self.total_frames > 0 {
            self.successful_frames as f64 / self.total_frames as f64 * 100.0
        } else {
            0.0
        };

        let avg_latency = if self.successful_frames > 0 {
            self.total_latency.as_secs_f64() * 1000.0 / self.successful_frames as f64
        } else {
            0.0
        };

        let actual_fps = self.successful_frames as f64 / elapsed.as_secs_f64();

        println!("=== 性能基准测试报告 ===");
        println!("测试时长: {:.2}秒", elapsed.as_secs_f64());
        println!("总帧数: {}", self.total_frames);
        println!("成功帧数: {}", self.successful_frames);
        println!("失败帧数: {}", self.failed_frames);
        println!("成功率: {:.2}%", success_rate);
        println!("实际帧率: {:.2} FPS", actual_fps);
        println!("平均延迟: {:.2}ms", avg_latency);
        if self.successful_frames > 0 {
            println!("最小延迟: {:.2}ms", self.min_latency.as_secs_f64() * 1000.0);
            println!("最大延迟: {:.2}ms", self.max_latency.as_secs_f64() * 1000.0);
        }
        println!("========================");
    }
}

fn main() -> Result<(), SmartScopeError> {
    println!("=== SmartScope Core 性能基准测试 ===");

    // 创建和初始化核心
    let mut core = SmartScopeCore::new()?;

    // 配置高性能参数
    let stream_config = SmartScopeStreamConfig {
        width: 1280,
        height: 720,
        format: ImageFormat::RGB888, // 高质量格式
        fps: 60, // 高帧率
        read_interval_ms: 16, // 对应60fps
    };

    let correction_config = SmartScopeCorrectionConfig {
        correction_type: CorrectionType::None, // 禁用校正以获得最佳性能
        params_dir: "/tmp/smartscope_perf".to_string(),
        enable_caching: true,
    };

    core.initialize(stream_config, correction_config)?;
    core.start()?;

    println!("配置参数:");
    println!("  分辨率: 1280×720");
    println!("  格式: RGB888");
    println!("  目标帧率: 60 FPS");
    println!("  读取间隔: 16ms");

    // 短期性能测试 (10秒)
    println!("\n开始短期性能测试 (10秒)...");
    run_performance_test(&mut core, Duration::from_secs(10), "左相机")?;

    // 检查是否有右相机
    let status = core.get_camera_status()?;
    if status.right_connected {
        println!("\n测试右相机性能...");
        run_right_camera_test(&mut core, Duration::from_secs(5))?;
    }

    // 立体同步性能测试
    if status.mode == CameraMode::StereoCamera && status.right_connected {
        println!("\n开始立体同步性能测试 (5秒)...");
        run_stereo_sync_test(&mut core, Duration::from_secs(5))?;
    }

    // 不同分辨率性能对比
    println!("\n开始分辨率性能对比测试...");
    run_resolution_comparison_test()?;

    core.stop()?;
    println!("\n✓ 性能基准测试完成");

    Ok(())
}

fn run_performance_test(
    core: &mut SmartScopeCore,
    duration: Duration,
    camera_name: &str,
) -> Result<(), SmartScopeError> {
    let mut metrics = PerformanceMetrics::new();
    let mut last_report = Instant::now();

    while metrics.start_time.elapsed() < duration {
        let frame_start = Instant::now();

        match core.get_left_frame() {
            Ok(frame_data) => {
                let latency = frame_start.elapsed();
                metrics.record_success(latency);

                // 验证帧数据完整性
                if frame_data.data.is_null() || frame_data.size == 0 {
                    eprintln!("警告: 检测到无效帧数据");
                }
            }
            Err(e) => {
                metrics.record_failure();
                if metrics.failed_frames % 10 == 0 {
                    eprintln!("连续失败 {} 次，最新错误: {}", metrics.failed_frames, e);
                }
            }
        }

        // 每2秒报告一次进度
        if last_report.elapsed() >= Duration::from_secs(2) {
            let elapsed = metrics.start_time.elapsed().as_secs();
            let current_fps = metrics.successful_frames as f64 / metrics.start_time.elapsed().as_secs_f64();
            println!("{}s - 当前帧率: {:.1} FPS, 成功: {}, 失败: {}",
                elapsed, current_fps, metrics.successful_frames, metrics.failed_frames);
            last_report = Instant::now();
        }

        // 小幅延迟避免过度CPU占用
        std::thread::sleep(Duration::from_millis(1));
    }

    println!("\n{} 性能测试结果:", camera_name);
    metrics.print_report();

    Ok(())
}

fn run_right_camera_test(
    core: &mut SmartScopeCore,
    duration: Duration,
) -> Result<(), SmartScopeError> {
    let mut metrics = PerformanceMetrics::new();

    while metrics.start_time.elapsed() < duration {
        let frame_start = Instant::now();

        match core.get_right_frame() {
            Ok(_) => {
                let latency = frame_start.elapsed();
                metrics.record_success(latency);
            }
            Err(_) => {
                metrics.record_failure();
            }
        }

        std::thread::sleep(Duration::from_millis(1));
    }

    println!("\n右相机性能测试结果:");
    metrics.print_report();

    Ok(())
}

fn run_stereo_sync_test(
    core: &mut SmartScopeCore,
    duration: Duration,
) -> Result<(), SmartScopeError> {
    let mut sync_count = 0;
    let mut total_attempts = 0;
    let mut max_timestamp_diff = 0i64;
    let mut total_timestamp_diff = 0i64;
    let start_time = Instant::now();

    while start_time.elapsed() < duration {
        let left_result = core.get_left_frame();
        let right_result = core.get_right_frame();

        total_attempts += 1;

        match (left_result, right_result) {
            (Ok(left_frame), Ok(right_frame)) => {
                sync_count += 1;

                let timestamp_diff = (left_frame.timestamp as i64 - right_frame.timestamp as i64).abs();
                total_timestamp_diff += timestamp_diff;
                if timestamp_diff > max_timestamp_diff {
                    max_timestamp_diff = timestamp_diff;
                }

                // 验证尺寸一致性
                if left_frame.width != right_frame.width || left_frame.height != right_frame.height {
                    eprintln!("警告: 左右帧尺寸不一致");
                }
            }
            _ => {} // 记录失败
        }

        std::thread::sleep(Duration::from_millis(10));
    }

    let sync_rate = sync_count as f64 / total_attempts as f64 * 100.0;
    let avg_timestamp_diff = if sync_count > 0 {
        total_timestamp_diff as f64 / sync_count as f64
    } else {
        0.0
    };

    println!("\n立体同步性能测试结果:");
    println!("总尝试次数: {}", total_attempts);
    println!("成功同步: {}", sync_count);
    println!("同步成功率: {:.2}%", sync_rate);
    println!("平均时间戳差异: {:.2}ms", avg_timestamp_diff);
    println!("最大时间戳差异: {}ms", max_timestamp_diff);

    Ok(())
}

fn run_resolution_comparison_test() -> Result<(), SmartScopeError> {
    let resolutions = vec![
        (640, 480, "VGA"),
        (1280, 720, "HD"),
        (1920, 1080, "Full HD"),
    ];

    println!("\n分辨率性能对比:");

    for (width, height, name) in resolutions {
        println!("\n测试 {} ({}×{})...", name, width, height);

        let mut core = SmartScopeCore::new()?;

        let stream_config = SmartScopeStreamConfig {
            width,
            height,
            format: ImageFormat::RGB888,
            fps: 30,
            read_interval_ms: 33,
        };

        let correction_config = SmartScopeCorrectionConfig::default();

        match core.initialize(stream_config, correction_config) {
            Ok(_) => {
                if core.start().is_ok() {
                    // 快速测试5秒
                    let test_start = Instant::now();
                    let mut frame_count = 0;

                    while test_start.elapsed() < Duration::from_secs(3) {
                        if core.get_left_frame().is_ok() {
                            frame_count += 1;
                        }
                        std::thread::sleep(Duration::from_millis(33));
                    }

                    let actual_fps = frame_count as f64 / test_start.elapsed().as_secs_f64();
                    println!("  {} 实际帧率: {:.2} FPS", name, actual_fps);

                    let _ = core.stop();
                } else {
                    println!("  {} 启动失败", name);
                }
            }
            Err(e) => {
                println!("  {} 初始化失败: {}", name, e);
            }
        }
    }

    Ok(())
}