//! Full screen recording (1920x1200) with performance optimization

use video_recorder::{RecorderConfig, RecordingService, VideoCodec, HardwareAccelType, X11ScreenCapturer};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};
use std::fs::File;
use std::io::Write;

fn main() {
    env_logger::init();

    println!("=== Full Screen Recording (1920x1200) ===");

    // Get actual screen size
    let mut capturer = X11ScreenCapturer::new().expect("Failed to initialize capturer");
    let (screen_width, screen_height) = capturer.dimensions();

    println!("Screen resolution: {}x{}", screen_width, screen_height);

    // Use actual screen size
    let config = RecorderConfig {
        width: screen_width,
        height: screen_height,
        fps: 30,
        bitrate: 8_000_000,  // Higher bitrate for full screen
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::None,
        max_queue_size: 90,     // Larger queue for full screen
        output_path: "fullscreen_rust.mp4".to_string(),
    };

    println!("Starting FFmpeg for full screen recording...");

    // Start FFmpeg process for full screen
    let mut child = Command::new("ffmpeg")
        .args(&[
            "-y",  // Overwrite output
            "-f", "rawvideo",
            "-vcodec", "rawvideo",
            "-pix_fmt", "rgb24",
            "-s", &format!("{}x{}", config.width, config.height),
            "-r", &config.fps.to_string(),
            "-i", "-",  // Read from stdin
            "-c:v", "libx264",
            "-preset", "ultrafast",
            "-tune", "zerolatency",
            "-pix_fmt", "yuv420p",
            "-b:v", &config.bitrate.to_string(),
            "-maxrate", "12M",  // Allow bitrate spikes
            "-bufsize", "24M", // Buffer for variable bitrate
            &config.output_path,
        ])
        .stdin(Stdio::piped())
        .spawn()
        .expect("Failed to start FFmpeg");

    let mut stdin = child.stdin.take().expect("Failed to open stdin");
    println!("✅ FFmpeg started, PID: {}", child.id());

    // Performance test
    println!("Recording full screen for 5 seconds...");
    let start = Instant::now();
    let mut frames_sent = 0;
    let mut total_bytes = 0;
    let frame_interval = Duration::from_micros(1_000_000 / config.fps as u64);
    let mut last_report = Instant::now();

    while start.elapsed() < Duration::from_secs(5) {
        let frame_start = Instant::now();

        if let Ok(frame) = capturer.capture_frame() {
            // Write frame data to FFmpeg
            match stdin.write_all(&frame.data) {
                Ok(_) => {
                    frames_sent += 1;
                    total_bytes += frame.data.len();
                }
                Err(e) => {
                    eprintln!("Failed to write frame to FFmpeg: {}", e);
                    break;
                }
            }
        }

        // Report every second
        if last_report.elapsed() >= Duration::from_secs(1) {
            let elapsed = start.elapsed().as_secs_f64();
            let current_fps = frames_sent as f64 / elapsed;
            let mb_sent = total_bytes / (1024 * 1024);
            println!("  {:.1}s: {} frames, {:.1} FPS, {} MB sent",
                     elapsed, frames_sent, current_fps, mb_sent);
            last_report = Instant::now();
        }

        // Maintain frame rate
        let elapsed = frame_start.elapsed();
        if elapsed < frame_interval {
            std::thread::sleep(frame_interval - elapsed);
        }
    }

    // Close stdin to signal EOF
    drop(stdin);

    println!("Waiting for FFmpeg to finish processing...");
    match child.wait() {
        Ok(status) => {
            if status.success() {
                println!("✅ FFmpeg completed successfully");
            } else {
                println!("⚠️  FFmpeg exited with status: {:?}", status);
            }
        }
        Err(e) => {
            println!("❌ Failed to wait for FFmpeg: {}", e);
        }
    }

    // Check results
    match std::fs::metadata("fullscreen_rust.mp4") {
        Ok(metadata) => {
            let size_mb = metadata.len() / (1024 * 1024);
            println!("\n=== Full Screen Results ===");
            println!("Resolution: {}x{}", config.width, config.height);
            println!("Frames sent: {}", frames_sent);
            println!("File size: {} MB", size_mb);
            println!("Data processed: {} MB", total_bytes / (1024 * 1024));

            let actual_fps = frames_sent as f64 / 5.0; // 5 seconds
            println!("Average FPS: {:.1}", actual_fps);

            if size_mb > 0 && actual_fps >= 20.0 {
                println!("✅ SUCCESS: Full screen recording completed!");
            } else {
                println!("⚠️  Recording completed but performance may be low");
            }

            // Video info
            println!("\nVideo file: fullscreen_rust.mp4");
            println!("Check with: ffprobe -v quiet -print_format json -show_format fullscreen_rust.mp4");
        }
        Err(e) => {
            println!("❌ Failed to check output file: {}", e);
        }
    }
}