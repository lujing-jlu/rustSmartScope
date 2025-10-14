use rknn_inference::{ImageBuffer, ImageFormat, Yolov8Detector};
use std::env;
use std::time::{Duration, Instant};

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║    YOLOv8 RKNN Timing Analysis (100 iterations)     ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    // Parse command line arguments
    let args: Vec<String> = env::args().collect();
    let model_path = if args.len() > 1 {
        args[1].as_str()
    } else {
        "models/yolov8m.rknn"
    };
    let image_path = if args.len() > 2 {
        args[2].as_str()
    } else {
        "tests/test.jpg"
    };

    println!("Configuration:");
    println!("  Model:        {}", model_path);
    println!("  Input Image:  {}", image_path);
    println!("  Iterations:   100\n");

    // Step 1: Load model
    println!("Step 1: Loading YOLOv8 model...");
    let mut detector = Yolov8Detector::new(model_path).expect("Failed to load model");
    let (model_w, model_h) = detector.model_size();
    println!("✓ Model loaded successfully!");
    println!("  Input size: {}x{}\n", model_w, model_h);

    // Step 2: Load image
    println!("Step 2: Loading input image...");
    println!("Loading image from: {}", image_path);
    let img = image::open(image_path).expect("Failed to load image");
    let img = img.to_rgb8();
    println!("Image loaded: {}x{}", img.width(), img.height());
    println!("✓ Image loaded successfully!\n");

    // Step 3: First run to test with preprocessing
    println!("Step 3: First run (includes preprocessing)...");
    let buffer_first = image_to_rknn_buffer(&img, model_w, model_h);
    let start_first = Instant::now();
    let results = detector.detect(&buffer_first).expect("First inference failed");
    let first_time = start_first.elapsed();
    println!("✓ First run completed in {:.2}ms", first_time.as_secs_f64() * 1000.0);
    println!("  Detected {} objects\n", results.len());

    // Step 4: Subsequent runs (preprocessing already done, test with same buffer)
    println!("Step 4: Running benchmark (100 iterations with pre-processed buffer)...");

    let mut total_times = Vec::new();
    let mut min_time = Duration::MAX;
    let mut max_time = Duration::ZERO;

    // Warmup
    let _ = detector.detect(&buffer_first).expect("Warmup failed");

    for i in 0..100 {
        let start = Instant::now();
        let _results = detector.detect(&buffer_first).expect("Inference failed");
        let elapsed = start.elapsed();

        total_times.push(elapsed);
        min_time = min_time.min(elapsed);
        max_time = max_time.max(elapsed);

        if (i + 1) % 10 == 0 {
            print!(".");
            std::io::Write::flush(&mut std::io::stdout()).unwrap();
        }
    }
    println!("\n✓ Benchmark completed!\n");

    // Calculate statistics
    let total_time: Duration = total_times.iter().sum();
    let avg_time = total_time / 100;
    let avg_ms = avg_time.as_secs_f64() * 1000.0;
    let min_ms = min_time.as_secs_f64() * 1000.0;
    let max_ms = max_time.as_secs_f64() * 1000.0;

    // Calculate standard deviation
    let variance: f64 = total_times
        .iter()
        .map(|t| {
            let diff = t.as_secs_f64() - avg_time.as_secs_f64();
            diff * diff
        })
        .sum::<f64>()
        / 100.0;
    let std_dev_ms = variance.sqrt() * 1000.0;

    // Step 5: Test preprocessing time separately
    println!("Step 5: Testing preprocessing time separately...");
    let mut preprocess_times = Vec::new();
    for _ in 0..10 {
        let start = Instant::now();
        let _ = image_to_rknn_buffer(&img, model_w, model_h);
        preprocess_times.push(start.elapsed());
    }
    let avg_preprocess: Duration = preprocess_times.iter().sum::<Duration>() / 10;
    let preprocess_ms = avg_preprocess.as_secs_f64() * 1000.0;
    println!("✓ Average preprocessing time: {:.2}ms (10 iterations)\n", preprocess_ms);

    // Display results
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║                   Benchmark Results                  ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    println!("【Preprocessing (Rust - letterbox + resize)】");
    println!("  Average time:  {:.2}ms\n", preprocess_ms);

    println!("【Inference + Post-processing (per iteration)】");
    println!("  Average time:  {:.2}ms", avg_ms);
    println!("  Min time:      {:.2}ms", min_ms);
    println!("  Max time:      {:.2}ms", max_ms);
    println!("  Std deviation: {:.2}ms", std_dev_ms);
    println!("  Total (100x):  {:.2}ms\n", total_time.as_secs_f64() * 1000.0);

    println!("【Total Pipeline】");
    println!("  Preprocessing:     {:.2}ms  ({:.1}%)",
             preprocess_ms, preprocess_ms / (preprocess_ms + avg_ms) * 100.0);
    println!("  Inference + Post:  {:.2}ms  ({:.1}%)",
             avg_ms, avg_ms / (preprocess_ms + avg_ms) * 100.0);
    println!("  Total per image:   {:.2}ms", preprocess_ms + avg_ms);
    println!("  FPS:               {:.2}\n", 1000.0 / (preprocess_ms + avg_ms));

    println!("【Analysis】");
    println!("  The {:.2}ms per iteration includes:", avg_ms);
    println!("  - C++ preprocessing (letterbox via RGA)");
    println!("  - RKNN inference (NPU execution)");
    println!("  - Post-processing (NMS, coordinate transform)");
    println!("  \n  Note: Cannot separate these without modifying C++ code.");
    println!("  Based on typical YOLO timings:");
    println!("  - Preprocessing:  ~5-10ms (RGA acceleration)");
    println!("  - NPU Inference:  ~60-70ms (main bottleneck)");
    println!("  - Postprocessing: ~10-15ms (NMS on CPU)\n");

    println!("✓ Analysis complete!");
}

fn image_to_rknn_buffer(img: &image::RgbImage, model_w: u32, model_h: u32) -> ImageBuffer {
    let (orig_w, orig_h) = (img.width(), img.height());

    // Calculate scale to fit image into model input size (letterbox)
    let scale = (model_w as f32 / orig_w as f32).min(model_h as f32 / orig_h as f32);
    let new_w = (orig_w as f32 * scale) as u32;
    let new_h = (orig_h as f32 * scale) as u32;

    // Resize image
    let resized = image::imageops::resize(
        img,
        new_w,
        new_h,
        image::imageops::FilterType::CatmullRom,
    );

    // Create letterbox image (padded to model size)
    let mut letterbox = image::RgbImage::new(model_w, model_h);
    // Fill with gray (114, 114, 114)
    for pixel in letterbox.pixels_mut() {
        *pixel = image::Rgb([114, 114, 114]);
    }

    // Calculate padding offsets
    let x_offset = (model_w - new_w) / 2;
    let y_offset = (model_h - new_h) / 2;

    // Copy resized image to center of letterbox
    image::imageops::overlay(&mut letterbox, &resized, x_offset.into(), y_offset.into());

    // Convert to RGB888 format (raw bytes)
    let pixels = letterbox.into_raw();

    ImageBuffer {
        width: model_w as i32,
        height: model_h as i32,
        format: ImageFormat::Rgb888,
        data: pixels,
    }
}
