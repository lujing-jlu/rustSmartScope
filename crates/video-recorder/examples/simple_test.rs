//! Simple encoder test to verify video file generation

use video_recorder::{RecorderConfig, RecordingService, VideoCodec, HardwareAccelType, OptimizedScreenCapturer, SimpleEncoder};
use std::time::{Duration, Instant};

fn main() {
    env_logger::init();

    println!("=== Simple Encoder Test ===");

    // Test simple encoder directly
    let config = RecorderConfig {
        width: 1280,
        height: 720,
        fps: 30,
        bitrate: 2_000_000,
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::None,
        max_queue_size: 60,
        output_path: "simple_test.mp4".to_string(),
    };

    println!("Testing simple encoder directly...");
    let mut encoder = SimpleEncoder::new(config.clone())
        .expect("Failed to create simple encoder");

    encoder.start().expect("Failed to start encoder");
    println!("✅ Simple encoder started");

    // Capture and encode a few frames
    let mut capturer = OptimizedScreenCapturer::for_high_fps()
        .expect("Failed to create capturer");

    println!("Encoding 10 frames...");
    for i in 0..10 {
        if let Ok(frame) = capturer.capture_frame() {
            encoder.encode_frame(&frame).expect("Failed to encode frame");
            println!("  Frame {} encoded ({}x{})", i, frame.width, frame.height);
        }
        std::thread::sleep(Duration::from_millis(33)); // ~30 FPS
    }

    encoder.finish().expect("Failed to finish encoder");
    println!("✅ Simple encoder finished");

    // Check file size
    match std::fs::metadata("simple_test.mp4") {
        Ok(metadata) => {
            let size_kb = metadata.len() / 1024;
            println!("📁 Output file size: {} KB", size_kb);
            if size_kb > 0 {
                println!("✅ Video file generated successfully!");
            } else {
                println!("❌ Video file is empty");
            }
        }
        Err(e) => {
            println!("❌ Failed to read output file: {}", e);
        }
    }

    println!("\n=== Full Recording Test ===");

    // Test with recording service using simple encoder
    println!("Testing full recording pipeline...");
    let mut service = RecordingService::new(config);

    service.start().expect("Failed to start recording");
    println!("✅ Recording service started");

    println!("Recording 2 seconds...");
    let start = Instant::now();
    let mut frames = 0;

    while start.elapsed() < Duration::from_secs(2) {
        if let Ok(frame) = capturer.capture_frame() {
            let _ = service.submit_frame(frame);
            frames += 1;
        }
        std::thread::sleep(Duration::from_millis(33)); // ~30 FPS
    }

    service.stop().expect("Failed to stop recording");
    let stats = service.stats();

    // Check final file
    match std::fs::metadata("simple_test.mp4") {
        Ok(metadata) => {
            let size_kb = metadata.len() / 1024;
            println!("\n=== Results ===");
            println!("Frames captured: {}", frames);
            println!("Frames encoded: {}", stats.frames_encoded);
            println!("File size: {} KB", size_kb);
            if size_kb > 0 && stats.frames_encoded > 0 {
                println!("✅ SUCCESS: Video file created with encoded frames!");
            } else {
                println!("⚠️  File created but no frames were encoded");
            }
        }
        Err(e) => {
            println!("❌ Failed to check final file: {}", e);
        }
    }
}