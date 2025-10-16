//! Performance test for optimized screen capture

use video_recorder::{RecorderConfig, RecordingService, VideoCodec, HardwareAccelType, OptimizedScreenCapturer};
use std::time::{Duration, Instant};

fn main() {
    env_logger::init();

    println!("=== Performance Test ===");

    // Test optimized capture performance
    let mut capturer = OptimizedScreenCapturer::for_high_fps()
        .expect("Failed to create optimized capturer");

    let (w, h) = capturer.dimensions();
    println!("Capture region: {}x{}", w, h);

    // Benchmark capture alone
    println!("\n1. Testing capture performance...");
    let capture_fps = capturer.benchmark(30).expect("Benchmark failed");
    println!("📊 Capture FPS: {:.1}", capture_fps);

    // Test full recording pipeline
    println!("\n2. Testing full recording pipeline...");
    let config = RecorderConfig {
        width: w,
        height: h,
        fps: 30,
        bitrate: 4_000_000,
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::None,  // Use software for now
        max_queue_size: 60,
        output_path: "performance_test.mp4".to_string(),
    };

    let mut service = RecordingService::new(config);
    service.start().expect("Failed to start recording");

    println!("Recording 3 seconds at target 30 FPS...");
    let start = Instant::now();
    let mut frames_captured = 0;
    let mut last_report = Instant::now();

    while start.elapsed() < Duration::from_secs(3) {
        let frame_start = Instant::now();

        if let Ok(frame) = capturer.capture_frame() {
            if let Err(e) = service.submit_frame(frame) {
                eprintln!("Failed to submit frame: {}", e);
            }
            frames_captured += 1;
        }

        // Report every 500ms
        if last_report.elapsed() >= Duration::from_millis(500) {
            let elapsed = start.elapsed().as_secs_f32();
            let current_fps = frames_captured as f32 / elapsed;
            let queue_size = service.queue_size();
            println!("  {:.1}s: {} frames, {:.1} FPS, queue: {}",
                     elapsed, frames_captured, current_fps, queue_size);
            last_report = Instant::now();
        }

        // Target 30 FPS (33ms per frame)
        let frame_time = frame_start.elapsed();
        if frame_time < Duration::from_millis(33) {
            std::thread::sleep(Duration::from_millis(33) - frame_time);
        }
    }

    service.stop().expect("Failed to stop recording");
    let stats = service.stats();

    let total_time = start.elapsed().as_secs_f64();
    let actual_fps = frames_captured as f64 / total_time;

    println!("\n=== Performance Results ===");
    println!("Capture benchmark: {:.1} FPS", capture_fps);
    println!("Actual recording: {:.1} FPS ({} frames / {:.2}s)", actual_fps, frames_captured, total_time);
    println!("Frames encoded: {}", stats.frames_encoded);
    println!("Frames dropped: {}", stats.frames_dropped);
    println!("Queue utilization: {:.1}%", (stats.frames_dropped as f64 / frames_captured as f64) * 100.0);

    if actual_fps >= 25.0 {
        println!("✅ Performance: GOOD (>= 25 FPS)");
    } else if actual_fps >= 15.0 {
        println!("⚠️  Performance: OK (15-25 FPS)");
    } else {
        println!("❌ Performance: POOR (< 15 FPS)");
    }

    println!("Output: performance_test.mp4");
}