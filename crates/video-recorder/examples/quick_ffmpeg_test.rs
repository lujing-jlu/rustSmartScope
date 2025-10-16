//! Quick test using system FFmpeg command line

use video_recorder::{RecorderConfig, RecordingService, VideoCodec, HardwareAccelType, OptimizedScreenCapturer};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};
use std::fs::OpenOptions;
use std::io::Write;

fn main() {
    env_logger::init();

    println!("=== Quick FFmpeg Command Line Test ===");

    // Start FFmpeg process
    let config = RecorderConfig {
        width: 1280,
        height: 720,
        fps: 30,
        bitrate: 2_000_000,
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::None,
        max_queue_size: 60,
        output_path: "ffmpeg_test.mp4".to_string(),
    };

    println!("Starting FFmpeg process...");
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
            &config.output_path,
        ])
        .stdin(Stdio::piped())
        .spawn()
        .expect("Failed to start FFmpeg");

    let stdin = child.stdin.take().expect("Failed to open stdin");

    println!("✅ FFmpeg process started, PID: {}", child.id());

    // Create capturer
    let mut capturer = OptimizedScreenCapturer::for_high_fps()
        .expect("Failed to create capturer");

    println!("Capturing and feeding frames to FFmpeg for 3 seconds...");

    let start = Instant::now();
    let mut frames_sent = 0;
    let frame_interval = Duration::from_micros(1_000_000 / config.fps as u64);

    while start.elapsed() < Duration::from_secs(3) {
        let frame_start = Instant::now();

        if let Ok(frame) = capturer.capture_frame() {
            // Write frame data to FFmpeg stdin
            if let Err(e) = stdin.lock().unwrap().write_all(&frame.data) {
                eprintln!("Failed to write frame to FFmpeg: {}", e);
                break;
            }
            frames_sent += 1;
        }

        // Maintain frame rate
        let elapsed = frame_start.elapsed();
        if elapsed < frame_interval {
            std::thread::sleep(frame_interval - elapsed);
        }
    }

    // Close stdin to signal EOF
    drop(stdin);

    println!("Waiting for FFmpeg to finish...");
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

    // Check output file
    match std::fs::metadata("ffmpeg_test.mp4") {
        Ok(metadata) => {
            let size_kb = metadata.len() / 1024;
            println!("\n=== Results ===");
            println!("Frames sent: {}", frames_sent);
            println!("File size: {} KB", size_kb);

            if size_kb > 0 {
                println!("✅ SUCCESS: Video file created!");

                // Try to get video info
                if let Ok(output) = Command::new("ffprobe")
                    .args(&["-v", "quiet", "-print_format", "json", "-show_format", "-show_streams", "ffmpeg_test.mp4"])
                    .output()
                {
                    if output.status.success() {
                        let json = String::from_utf8_lossy(&output.stdout);
                        println!("📹 Video info available via: ffprobe ffmpeg_test.mp4");
                    }
                }
            } else {
                println!("❌ Video file is empty");
            }
        }
        Err(e) => {
            println!("❌ Failed to check output file: {}", e);
        }
    }

    println!("Output: ffmpeg_test.mp4");
}