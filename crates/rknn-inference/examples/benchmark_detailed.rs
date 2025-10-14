use rknn_inference::{ImageBuffer, ImageFormat};
use std::env;
use std::ffi::CString;
use std::time::{Duration, Instant};

// Import FFI types
use rknn_inference::ffi::{
    FfiImageBuffer, LetterBox, ObjectDetectResultList, RknnAppContext, RknnOutput,
};

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║    YOLOv8 RKNN Detailed Benchmark (100 iter)        ║");
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
    let model_path_c = CString::new(model_path).unwrap();
    let mut app_ctx = RknnAppContext::default();

    unsafe {
        let ret = rknn_inference::ffi::init_yolov8_model(model_path_c.as_ptr(), &mut app_ctx);
        if ret < 0 {
            panic!("Failed to load model: {}", ret);
        }

        // Initialize post-process
        let ret = rknn_inference::ffi::init_post_process();
        if ret < 0 {
            panic!("Failed to init post process: {}", ret);
        }
    }

    println!("✓ Model loaded successfully!");
    println!("  Input size: {}x{}\n", app_ctx.model_width, app_ctx.model_height);

    // Step 2: Load image
    println!("Step 2: Loading input image...");
    let img = image::open(image_path).expect("Failed to load image");
    let img = img.to_rgb8();
    println!("Image loaded: {}x{}", img.width(), img.height());
    println!("✓ Image loaded successfully!\n");

    // Convert to ImageBuffer
    let image_buffer = ImageBuffer {
        width: img.width() as i32,
        height: img.height() as i32,
        format: ImageFormat::Rgb888,
        data: img.into_raw(),
    };

    // Prepare for benchmark
    let mut preprocess_times = Vec::new();
    let mut inference_times = Vec::new();
    let mut postprocess_times = Vec::new();

    println!("Step 3: Running detailed benchmark (100 iterations)...\n");

    for i in 0..100 {
        let mut ffi_img = FfiImageBuffer::from_image_buffer(&image_buffer);
        let mut dst_img = rknn_inference::ffi::CImageBuffer {
            width: 0,
            height: 0,
            width_stride: 0,
            height_stride: 0,
            format: 0,
            virt_addr: std::ptr::null_mut(),
            size: 0,
            fd: -1,
        };
        let mut letter_box = LetterBox::default();
        let mut outputs: Vec<RknnOutput> = (0..app_ctx.io_num.n_output)
            .map(|_| RknnOutput::default())
            .collect();
        let mut od_results = ObjectDetectResultList::default();

        // Preprocess timing
        let start_preprocess = Instant::now();
        unsafe {
            let ret = rknn_inference::ffi::yolov8_preprocess(
                &mut app_ctx,
                ffi_img.as_ptr(),
                &mut dst_img,
                &mut letter_box,
            );
            if ret < 0 {
                panic!("Preprocess failed: {}", ret);
            }
        }
        let preprocess_time = start_preprocess.elapsed();
        preprocess_times.push(preprocess_time);

        // Inference timing
        let start_inference = Instant::now();
        unsafe {
            let ret = rknn_inference::ffi::yolov8_inference(
                &mut app_ctx,
                &mut dst_img,
                outputs.as_mut_ptr(),
            );
            if ret < 0 {
                panic!("Inference failed: {}", ret);
            }
        }
        let inference_time = start_inference.elapsed();
        inference_times.push(inference_time);

        // Postprocess timing
        let start_postprocess = Instant::now();
        unsafe {
            let ret = rknn_inference::ffi::yolov8_postprocess(
                &mut app_ctx,
                outputs.as_mut_ptr(),
                &mut letter_box,
                &mut od_results,
            );
            if ret < 0 {
                panic!("Postprocess failed: {}", ret);
            }
        }
        let postprocess_time = start_postprocess.elapsed();
        postprocess_times.push(postprocess_time);

        // Cleanup
        unsafe {
            rknn_inference::ffi::yolov8_release_outputs(&mut app_ctx, outputs.as_mut_ptr());
            if !dst_img.virt_addr.is_null() {
                libc::free(dst_img.virt_addr as *mut libc::c_void);
            }
        }

        if i == 0 {
            println!("  First iteration detected {} objects", od_results.count);
        }

        if (i + 1) % 10 == 0 {
            print!(".");
            std::io::Write::flush(&mut std::io::stdout()).unwrap();
        }
    }

    println!("\n✓ Benchmark completed!\n");

    // Cleanup
    unsafe {
        rknn_inference::ffi::release_yolov8_model(&mut app_ctx);
        rknn_inference::ffi::deinit_post_process();
    }

    // Calculate statistics
    fn calculate_stats(times: &[Duration]) -> (f64, f64, f64, f64, f64) {
        let total: Duration = times.iter().sum();
        let avg = total / times.len() as u32;
        let min = *times.iter().min().unwrap();
        let max = *times.iter().max().unwrap();

        let avg_secs = avg.as_secs_f64();
        let variance: f64 = times
            .iter()
            .map(|t| {
                let diff = t.as_secs_f64() - avg_secs;
                diff * diff
            })
            .sum::<f64>()
            / times.len() as f64;
        let std_dev = variance.sqrt();

        (
            avg_secs * 1000.0,
            min.as_secs_f64() * 1000.0,
            max.as_secs_f64() * 1000.0,
            std_dev * 1000.0,
            total.as_secs_f64() * 1000.0,
        )
    }

    let (pre_avg, pre_min, pre_max, pre_std, pre_total) = calculate_stats(&preprocess_times);
    let (inf_avg, inf_min, inf_max, inf_std, inf_total) = calculate_stats(&inference_times);
    let (post_avg, post_min, post_max, post_std, post_total) = calculate_stats(&postprocess_times);

    let total_avg = pre_avg + inf_avg + post_avg;
    let total_time = pre_total + inf_total + post_total;

    // Display results
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║              Detailed Benchmark Results             ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    println!("【1. Preprocessing (letterbox + format conversion)】");
    println!("  Average:       {:.2}ms  ({:.1}%)", pre_avg, pre_avg / total_avg * 100.0);
    println!("  Min:           {:.2}ms", pre_min);
    println!("  Max:           {:.2}ms", pre_max);
    println!("  Std deviation: {:.2}ms", pre_std);
    println!("  Total (100x):  {:.2}ms\n", pre_total);

    println!("【2. Inference (NPU execution)】");
    println!("  Average:       {:.2}ms  ({:.1}%)", inf_avg, inf_avg / total_avg * 100.0);
    println!("  Min:           {:.2}ms", inf_min);
    println!("  Max:           {:.2}ms", inf_max);
    println!("  Std deviation: {:.2}ms", inf_std);
    println!("  Total (100x):  {:.2}ms\n", inf_total);

    println!("【3. Postprocessing (NMS + coordinate transform)】");
    println!("  Average:       {:.2}ms  ({:.1}%)", post_avg, post_avg / total_avg * 100.0);
    println!("  Min:           {:.2}ms", post_min);
    println!("  Max:           {:.2}ms", post_max);
    println!("  Std deviation: {:.2}ms", post_std);
    println!("  Total (100x):  {:.2}ms\n", post_total);

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║                    Total Pipeline                    ║");
    println!("╚══════════════════════════════════════════════════════╝");
    println!("  Total per image:  {:.2}ms", total_avg);
    println!("  Breakdown:");
    println!("    - Preprocessing:  {:.2}ms ({:.1}%)", pre_avg, pre_avg / total_avg * 100.0);
    println!("    - Inference:      {:.2}ms ({:.1}%)", inf_avg, inf_avg / total_avg * 100.0);
    println!("    - Postprocessing: {:.2}ms ({:.1}%)", post_avg, post_avg / total_avg * 100.0);
    println!("  FPS:              {:.2}", 1000.0 / total_avg);
    println!("  Total (100 iter): {:.2}ms\n", total_time);

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║                    Performance Analysis               ║");
    println!("╚══════════════════════════════════════════════════════╝");

    // Determine bottleneck
    let max_component = pre_avg.max(inf_avg).max(post_avg);
    let bottleneck = if (inf_avg - max_component).abs() < 0.01 {
        "Inference (NPU)"
    } else if (post_avg - max_component).abs() < 0.01 {
        "Postprocessing (CPU)"
    } else {
        "Preprocessing (CPU)"
    };

    println!("  Bottleneck:       {}", bottleneck);
    println!("  NPU utilization:  {:.1}%", inf_avg / total_avg * 100.0);
    println!("  CPU utilization:  {:.1}%", (pre_avg + post_avg) / total_avg * 100.0);

    println!("\n✓ Detailed benchmark complete!");
}
