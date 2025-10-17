//! Test external process screen recorder

use video_recorder::ScreenRecorder;
use std::time::Duration;

fn main() {
    env_logger::init();

    println!("=== External Process Screen Recorder Test ===");

    // Create recorder
    let mut recorder = match ScreenRecorder::new() {
        Ok(r) => {
            println!("✅ Recorder created with backend: {}", r.backend().name());
            r
        }
        Err(e) => {
            eprintln!("❌ Failed to create recorder: {}", e);
            return;
        }
    };

    // Set dimensions (auto-detect or manual)
    recorder.set_dimensions(1920, 1200);
    recorder.set_fps(30);

    // Generate output path
    let root_dir = std::env::var("HOME").unwrap_or_else(|_| "/tmp".to_string());
    let output_path = recorder.generate_output_path(&root_dir);
    println!("📹 Output: {}", output_path.display());

    // Start recording
    println!("🔴 Starting recording for 5 seconds...");
    if let Err(e) = recorder.start(output_path.clone()) {
        eprintln!("❌ Failed to start: {}", e);
        return;
    }

    // Record for 5 seconds with progress
    for i in 1..=5 {
        std::thread::sleep(Duration::from_secs(1));
        let elapsed = recorder.elapsed_seconds();
        println!("  {}s elapsed (is_recording: {})", elapsed, recorder.is_recording());
    }

    // Stop recording
    println!("⏹️  Stopping recording...");
    if let Err(e) = recorder.stop() {
        eprintln!("❌ Failed to stop: {}", e);
        return;
    }

    // Check output
    if output_path.exists() {
        if let Ok(metadata) = std::fs::metadata(&output_path) {
            let size_kb = metadata.len() / 1024;
            println!("✅ Recording saved: {} KB", size_kb);

            if size_kb > 0 {
                println!("✅ SUCCESS: Video file created!");
                println!("📁 File: {}", output_path.display());
            } else {
                println!("⚠️  Warning: File is empty");
            }
        }
    } else {
        println!("❌ Output file not found");
    }
}
