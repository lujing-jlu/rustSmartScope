//! Quick test for video recording with Rockchip support

use video_recorder::{RecorderConfig, RecordingService, VideoCodec, HardwareAccelType, X11ScreenCapturer};
use std::time::{Duration, Instant};

fn main() {
    env_logger::init();

    println!("=== Quick Rockchip Video Test ===");

    // Initialize screen capturer
    let mut capturer = X11ScreenCapturer::new().expect("Failed to initialize capturer");
    let (w, h) = capturer.dimensions();
    println!("Screen: {}x{}", w, h);

    // Configure with Rockchip hardware acceleration
    let config = RecorderConfig {
        width: w.min(1280),  // Limit size for testing
        height: h.min(720),
        fps: 5,  // Low FPS for quick test
        bitrate: 1_000_000,
        codec: VideoCodec::H264,
        hardware_accel: HardwareAccelType::RkMpp,
        max_queue_size: 30,
        output_path: "rockchip_test.mp4".to_string(),
    };

    println!("Config: {}x{} @ {}fps, RkMpp: {}",
             config.width, config.height, config.fps,
             matches!(config.hardware_accel, HardwareAccelType::RkMpp));

    let mut service = RecordingService::new(config);

    // Start recording
    service.start().expect("Failed to start recording");
    println!("Recording started (3 seconds)...");

    // Record for 3 seconds
    let start = Instant::now();
    let mut frames = 0;
    while start.elapsed() < Duration::from_secs(3) {
        if let Ok(frame) = capturer.capture_frame() {
            let _ = service.submit_frame(frame);
            frames += 1;
        }
        std::thread::sleep(Duration::from_millis(200)); // 5 FPS
    }

    // Stop and check results
    service.stop().expect("Failed to stop recording");
    let stats = service.stats();

    println!("=== Results ===");
    println!("Frames captured: {}", frames);
    println!("Frames encoded: {}", stats.frames_encoded);
    println!("Frames dropped: {}", stats.frames_dropped);
    println!("Output: rockchip_test.mp4");
    println!("=== Done ===");
}