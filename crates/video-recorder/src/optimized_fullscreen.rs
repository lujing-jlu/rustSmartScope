//! Optimized full screen recording (1920x1200) with better performance

use video_recorder::{X11ScreenCapturer, FFmpegRecorder, RecorderConfig, VideoCodec, HardwareAccelType};
use std::time::{Duration, Instant};
use std::io::Write;

fn main() {
    env_logger::init();

    println!("=== Optimized Full Screen Recording ===");

    // Get screen size
    let mut capturer = X11ScreenCapturer::new().expect("Failed to initialize capturer");
    let (width, height) = capturer.dimensions();

    println!("Screen: {}x{}", width, height);

    // Create config for full screen
    let config = RecorderConfig {
        width,
        height,
        fps: 15,  // Optimized for full screen
        bitrate: 4_000_000,
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::None,
        max_queue_size: 60,
        output_path: "optimized_fullscreen.mp4".to_string(),
    };

    // Start FFmpeg recorder
    let mut recorder = FFmpegRecorder::new(config).expect("Failed to create recorder");
    recorder.start().expect("Failed to start recording");

    println!("✅ Recording started for 5 seconds...");

    // Recording loop
    let start = Instant::now();
    let mut frames_sent = 0;
    let frame_interval = Duration::from_micros(1_000_000 / 15); // 15 FPS

    while start.elapsed() < Duration::from_secs(5) {
        let frame_start = Instant::now();

        if let Ok(frame) = capturer.capture_frame() {
            if let Err(e) = recorder.send_frame(&frame) {
                eprintln!("Failed to send frame: {}", e);
                break;
            }
            frames_sent += 1;
        }

        // Timing control
        let elapsed = frame_start.elapsed();
        if elapsed < frame_interval {
            std::thread::sleep(frame_interval - elapsed);
        }

        // Progress report
        if frames_sent % 30 == 0 {  // Every 2 seconds
            let elapsed = start.elapsed().as_secs_f64();
            let current_fps = frames_sent as f64 / elapsed;
            println!("  {:.1}s: {} frames, {:.1} FPS", elapsed, frames_sent, current_fps);
        }
    }

    // Stop recording
    recorder.stop().expect("Failed to stop recording");

    // Results
    let actual_fps = frames_sent as f64 / 5.0;
    println!("\n=== Results ===");
    println!("Resolution: {}x{}", width, height);
    println!("Actual FPS: {:.1}", actual_fps);
    println!("Frames: {}", frames_sent);
    println!("File: optimized_fullscreen.mp4");

    if actual_fps >= 12.0 {
        println!("✅ SUCCESS: Full screen recording optimized!");
    } else {
        println!("⚠️  Performance needs improvement");
    }
}