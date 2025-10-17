//! Example: Record X11 screen to video file

use video_recorder::{RecorderConfig, RecordingService, VideoCodec, HardwareAccelType, X11ScreenCapturer};
use std::time::{Duration, Instant};

fn main() {
    env_logger::init();

    println!("{}", "=".repeat(50));
    println!("X11 Screen Recording Example");
    println!("{}", "=".repeat(50));

    // Initialize screen capturer
    let mut capturer = match X11ScreenCapturer::new() {
        Ok(c) => {
            let (w, h) = c.dimensions();
            println!("Screen capturer initialized: {}x{}", w, h);
            c
        }
        Err(e) => {
            eprintln!("Failed to initialize screen capturer: {}", e);
            return;
        }
    };

    let (screen_width, screen_height) = capturer.dimensions();

    // Configure recorder
    let config = RecorderConfig {
        width: screen_width,
        height: screen_height,
        fps: 30,  // 提升到 30 FPS
        bitrate: 4_000_000,  // 4 Mbps
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::RkMpp,
        max_queue_size: 60,
        output_path: "screen_recording.mp4".to_string(),
    };

    println!("\nRecording configuration:");
    println!("  Resolution: {}x{}", config.width, config.height);
    println!("  FPS: {}", config.fps);
    println!("  Bitrate: {} bps", config.bitrate);
    println!("  Codec: {:?}", config.codec);
    println!("  Hardware Accel: {:?}", config.hardware_accel);
    println!("  Output: {}", config.output_path);

    // Create recording service
    let mut service = RecordingService::new(config);

    // Start recording
    if let Err(e) = service.start() {
        eprintln!("Failed to start recording: {}", e);
        return;
    }

    println!("\nRecording started... (will record for 10 seconds)");

    let start_time = Instant::now();
    let duration = Duration::from_secs(10);
    let frame_interval = Duration::from_micros(1_000_000 / 30); // 30 FPS

    let mut frame_count = 0;
    let mut last_stats_time = Instant::now();

    while start_time.elapsed() < duration {
        let frame_start = Instant::now();

        // Capture frame from screen
        match capturer.capture_frame() {
            Ok(frame) => {
                if let Err(e) = service.submit_frame(frame) {
                    eprintln!("Failed to submit frame: {}", e);
                }
                frame_count += 1;
            }
            Err(e) => {
                eprintln!("Failed to capture frame: {}", e);
            }
        }

        // Print statistics every second
        if last_stats_time.elapsed() >= Duration::from_secs(1) {
            let stats = service.stats();
            println!(
                "Progress: {:.1}s | Captured: {} | Encoded: {} | Dropped: {} | Queue: {}",
                start_time.elapsed().as_secs_f32(),
                frame_count,
                stats.frames_encoded,
                stats.frames_dropped,
                service.queue_size()
            );
            last_stats_time = Instant::now();
        }

        // Sleep to maintain frame rate
        let elapsed = frame_start.elapsed();
        if elapsed < frame_interval {
            std::thread::sleep(frame_interval - elapsed);
        }
    }

    println!("\nStopping recording...");

    // Stop recording
    if let Err(e) = service.stop() {
        eprintln!("Failed to stop recording: {}", e);
        return;
    }

    let stats = service.stats();
    println!("\nRecording completed!");
    println!("  Frames captured: {}", frame_count);
    println!("  Frames encoded: {}", stats.frames_encoded);
    println!("  Frames dropped: {}", stats.frames_dropped);
    println!("  Output file: screen_recording.mp4");
    println!("{}", "=".repeat(50));
}
