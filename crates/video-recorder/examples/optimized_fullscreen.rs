//! Optimized full screen recording (1920x1200) with better performance

use video_recorder::{X11ScreenCapturer};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

fn main() {
    env_logger::init();

    println!("=== Optimized Full Screen Recording ===");

    // Get screen size
    let mut capturer = X11ScreenCapturer::new().expect("Failed to initialize capturer");
    let (width, height) = capturer.dimensions();

    println!("Screen: {}x{}", width, height);

    // Optimized settings for full screen
    let target_fps = 15;  // Reduce to 15 FPS for better performance
    let duration_secs = 5;

    println!("Starting optimized FFmpeg ({} FPS, {}s)...", target_fps, duration_secs);

    let mut child = Command::new("ffmpeg")
        .args(&[
            "-y",
            "-f", "rawvideo",
            "-vcodec", "rawvideo",
            "-pix_fmt", "rgb24",
            "-s", &format!("{}x{}", width, height),
            "-r", &target_fps.to_string(),
            "-i", "-",
            "-c:v", "libx264",
            "-preset", "superfast",    // Faster encoding
            "-tune", "zerolatency",
            "-pix_fmt", "yuv420p",
            "-b:v", "4M",              // Lower bitrate for performance
            "-maxrate", "6M",
            "-bufsize", "12M",
            "-g", "30",                // Keyframe every 2 seconds
            "optimized_fullscreen.mp4",
        ])
        .stdin(Stdio::piped())
        .spawn()
        .expect("Failed to start FFmpeg");

    let mut stdin = child.stdin.take().expect("Failed to open stdin");
    println!("✅ FFmpeg started");

    // Recording loop with optimized timing
    let start = Instant::now();
    let mut frames_sent = 0;
    let frame_interval = Duration::from_micros(1_000_000 / target_fps);

    while start.elapsed() < Duration::from_secs(duration_secs) {
        let frame_start = Instant::now();

        if let Ok(frame) = capturer.capture_frame() {
            if let Err(e) = stdin.write_all(&frame.data) {
                eprintln!("Write error: {}", e);
                break;
            }
            frames_sent += 1;
        }

        // Precise timing control
        let elapsed = frame_start.elapsed();
        if elapsed < frame_interval {
            std::thread::sleep(frame_interval - elapsed);
        }

        // Progress report
        if frames_sent % (target_fps * 2) == 0 {  // Every 2 seconds
            let elapsed = start.elapsed().as_secs_f64();
            let current_fps = frames_sent as f64 / elapsed;
            println!("  {:.1}s: {} frames, {:.1} FPS", elapsed, frames_sent, current_fps);
        }
    }

    drop(stdin);

    println!("Finishing recording...");
    match child.wait() {
        Ok(status) if status.success() => println!("✅ Success!"),
        Ok(status) => println!("⚠️  FFmpeg exited: {:?}", status),
        Err(e) => println!("❌ Error: {}", e),
    }

    // Results
    if let Ok(metadata) = std::fs::metadata("optimized_fullscreen.mp4") {
        let size_mb = metadata.len() / (1024 * 1024);
        let actual_fps = frames_sent as f64 / duration_secs as f64;

        println!("\n=== Optimized Results ===");
        println!("Resolution: {}x{}", width, height);
        println!("Target FPS: {}", target_fps);
        println!("Actual FPS: {:.1}", actual_fps);
        println!("Frames: {}", frames_sent);
        println!("File size: {} MB", size_mb);
        println!("Duration: {}s", duration_secs);

        if actual_fps >= target_fps * 0.8 && size_mb > 0 {
            println!("✅ OPTIMIZED: Full screen recording successful!");
        } else {
            println!("⚠️  Performance still needs improvement");
        }

        println!("\n📹 File: optimized_fullscreen.mp4");
        println!("📊 Check with: ffprobe optimized_fullscreen.mp4");
    }
}