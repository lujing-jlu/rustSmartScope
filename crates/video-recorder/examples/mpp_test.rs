//! Test Rockchip MPP hardware encoder

use std::time::{Duration, Instant};
use video_recorder::{
    HardwareAccelType, MppEncoder, RecorderConfig, RecordingService, VideoCodec, X11ScreenCapturer,
};

fn main() {
    env_logger::init();

    println!("=== Rockchip MPP Hardware Test ===");

    // Check if MPP is available
    if MppEncoder::is_available() {
        println!("✅ MPP hardware detected");
    } else {
        println!("⚠️  MPP hardware not detected, will use software fallback");
    }

    // Test MPP encoder directly
    let config = RecorderConfig {
        width: 1280,
        height: 720,
        fps: 30,
        bitrate: 4_000_000,
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::RkMpp,
        max_queue_size: 60,
        output_path: "mpp_test.mp4".to_string(),
    };

    println!("\nTesting MPP encoder...");
    let mut encoder = MppEncoder::new(config.clone()).expect("Failed to create MPP encoder");

    encoder.start().expect("Failed to start MPP encoder");
    println!("✅ MPP encoder started successfully");

    // Test with a few frames
    let mut capturer = X11ScreenCapturer::new().expect("Failed to initialize capturer");
    let (w, h) = capturer.dimensions();
    println!("Screen size: {}x{}", w, h);

    println!("Capturing and encoding 5 frames...");
    for i in 0..5 {
        if let Ok(frame) = capturer.capture_frame() {
            encoder
                .encode_frame(&frame)
                .expect("Failed to encode frame");
            println!("  Frame {} encoded: {}x{}", i, frame.width, frame.height);
        }
        std::thread::sleep(Duration::from_millis(100));
    }

    encoder.finish().expect("Failed to finish MPP encoder");
    println!("✅ MPP test completed successfully");

    println!("\n=== Full Recording Service Test ===");

    // Test with recording service
    let mut service = RecordingService::new(config);
    service.start().expect("Failed to start recording service");

    println!("Recording for 2 seconds...");
    let start = Instant::now();
    let mut frames = 0;

    while start.elapsed() < Duration::from_secs(2) {
        if let Ok(frame) = capturer.capture_frame() {
            let _ = service.submit_frame(frame);
            frames += 1;
        }
        std::thread::sleep(Duration::from_millis(50)); // ~20 FPS
    }

    service.stop().expect("Failed to stop recording service");
    let stats = service.stats();

    println!("=== Final Results ===");
    println!("Frames captured: {}", frames);
    println!("Frames encoded: {}", stats.frames_encoded);
    println!("Frames dropped: {}", stats.frames_dropped);
    println!("Output: mpp_test.mp4");
    println!("=== Done ===");
}
