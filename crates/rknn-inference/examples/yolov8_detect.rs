use ab_glyph::{FontRef, PxScale};
use image::{DynamicImage, GenericImageView, ImageBuffer as ImageBuf, Rgb, RgbImage};
use imageproc::drawing::{draw_hollow_rect_mut, draw_text_mut};
use imageproc::rect::Rect;
use rknn_inference::{get_class_name, ImageBuffer, Yolov8Detector};
use std::env;
use std::fs;
use std::path::Path;

/// COCO 类别颜色映射 (BGR -> RGB)
const COLORS: [(u8, u8, u8); 10] = [
    (255, 0, 0),     // Red
    (0, 255, 0),     // Green
    (0, 0, 255),     // Blue
    (255, 255, 0),   // Yellow
    (255, 0, 255),   // Magenta
    (0, 255, 255),   // Cyan
    (128, 0, 0),     // Maroon
    (0, 128, 0),     // Dark Green
    (0, 0, 128),     // Navy
    (128, 128, 0),   // Olive
];

fn get_color_for_class(class_id: i32) -> Rgb<u8> {
    let idx = (class_id as usize) % COLORS.len();
    let (r, g, b) = COLORS[idx];
    Rgb([r, g, b])
}

/// 加载图像文件
fn load_image<P: AsRef<Path>>(path: P) -> Result<DynamicImage, Box<dyn std::error::Error>> {
    println!("Loading image from: {}", path.as_ref().display());
    let img = image::open(path)?;
    println!("Image loaded: {}x{}", img.width(), img.height());
    Ok(img)
}

/// 将 DynamicImage 转换为 RKNN ImageBuffer (RGB888)
fn image_to_rknn_buffer(img: &DynamicImage, target_width: u32, target_height: u32) -> ImageBuffer {
    // 将图像转换为 RGB8
    let rgb_img = img.to_rgb8();
    let (orig_width, orig_height) = rgb_img.dimensions();

    println!(
        "Converting image from {}x{} to {}x{}",
        orig_width, orig_height, target_width, target_height
    );

    // 计算缩放比例 (保持宽高比)
    let scale_x = target_width as f32 / orig_width as f32;
    let scale_y = target_height as f32 / orig_height as f32;
    let scale = scale_x.min(scale_y);

    // 计算缩放后的尺寸
    let scaled_width = (orig_width as f32 * scale) as u32;
    let scaled_height = (orig_height as f32 * scale) as u32;

    println!("Scale: {:.3}, Scaled size: {}x{}", scale, scaled_width, scaled_height);

    // 缩放图像
    let resized = image::imageops::resize(
        &rgb_img,
        scaled_width,
        scaled_height,
        image::imageops::FilterType::Lanczos3,
    );

    // 创建目标图像 (灰色背景 114)
    let mut target_img: RgbImage = ImageBuf::from_pixel(target_width, target_height, Rgb([114u8, 114u8, 114u8]));

    // 计算居中位置
    let offset_x = (target_width - scaled_width) / 2;
    let offset_y = (target_height - scaled_height) / 2;

    println!("Letterbox offset: x={}, y={}", offset_x, offset_y);

    // 将缩放后的图像复制到目标图像中心
    for y in 0..scaled_height {
        for x in 0..scaled_width {
            let pixel = resized.get_pixel(x, y);
            target_img.put_pixel(x + offset_x, y + offset_y, *pixel);
        }
    }

    // 转换为 Vec<u8>
    let data = target_img.into_raw();

    ImageBuffer::from_rgb888(target_width as i32, target_height as i32, data)
}

/// 将检测结果绘制到原始图像上
fn draw_detections(
    img: &mut RgbImage,
    detections: &[rknn_inference::DetectionResult],
    font: &FontRef,
) {
    println!("\nDrawing {} detections on image", detections.len());

    for (i, det) in detections.iter().enumerate() {
        let color = get_color_for_class(det.class_id);

        // 绘制边界框
        let rect = Rect::at(det.bbox.left, det.bbox.top)
            .of_size(det.bbox.width() as u32, det.bbox.height() as u32);

        // 绘制 3 个像素宽的边框
        for offset in 0..3 {
            let expanded_rect = Rect::at(rect.left() - offset, rect.top() - offset)
                .of_size(rect.width() + (offset * 2) as u32, rect.height() + (offset * 2) as u32);
            draw_hollow_rect_mut(img, expanded_rect, color);
        }

        // 获取类别名称
        let class_name = get_class_name(det.class_id).unwrap_or_else(|| format!("class_{}", det.class_id));
        let label = format!("{}: {:.2}%", class_name, det.confidence * 100.0);

        println!(
            "  [{}] {} at [{},{},{},{}] ({}x{})",
            i,
            label,
            det.bbox.left,
            det.bbox.top,
            det.bbox.right,
            det.bbox.bottom,
            det.bbox.width(),
            det.bbox.height()
        );

        // 绘制标签背景和文字
        let scale = PxScale::from(24.0);
        let text_x = det.bbox.left.max(0);
        let text_y = (det.bbox.top - 30).max(0);

        // 绘制白色文字
        draw_text_mut(
            img,
            Rgb([255, 255, 255]),
            text_x,
            text_y,
            scale,
            font,
            &label,
        );
    }
}

/// 保存检测结果到文本文件
fn save_results_txt<P: AsRef<Path>>(
    path: P,
    detections: &[rknn_inference::DetectionResult],
) -> Result<(), Box<dyn std::error::Error>> {
    use std::io::Write;

    let mut file = fs::File::create(path.as_ref())?;
    writeln!(file, "YOLOv8 Detection Results")?;
    writeln!(file, "=========================")?;
    writeln!(file, "Total detections: {}\n", detections.len())?;

    for (i, det) in detections.iter().enumerate() {
        let class_name = get_class_name(det.class_id).unwrap_or_else(|| format!("class_{}", det.class_id));

        writeln!(file, "Detection #{}:", i)?;
        writeln!(file, "  Class: {} (ID: {})", class_name, det.class_id)?;
        writeln!(file, "  Confidence: {:.2}%", det.confidence * 100.0)?;
        writeln!(
            file,
            "  Bounding Box: [{}, {}, {}, {}]",
            det.bbox.left, det.bbox.top, det.bbox.right, det.bbox.bottom
        )?;
        writeln!(file, "  Size: {}x{}", det.bbox.width(), det.bbox.height())?;
        writeln!(file)?;
    }

    println!("Results saved to: {}", path.as_ref().display());
    Ok(())
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();

    // 默认路径
    let default_model = "models/yolov8m.rknn";
    let default_image = "tests/test.jpg";

    let model_path = if args.len() > 1 {
        &args[1]
    } else {
        default_model
    };

    let image_path = if args.len() > 2 {
        &args[2]
    } else {
        default_image
    };

    let output_image = if args.len() > 3 {
        args[3].clone()
    } else {
        "output_detection.jpg".to_string()
    };

    let output_txt = if args.len() > 4 {
        args[4].clone()
    } else {
        "output_detection.txt".to_string()
    };

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║         YOLOv8 RKNN Object Detection Example        ║");
    println!("╚══════════════════════════════════════════════════════╝");
    println!();
    println!("Configuration:");
    println!("  Model:        {}", model_path);
    println!("  Input Image:  {}", image_path);
    println!("  Output Image: {}", output_image);
    println!("  Output Text:  {}", output_txt);
    println!();

    // Step 1: 加载模型
    println!("Step 1: Loading YOLOv8 model...");
    let mut detector = Yolov8Detector::new(model_path)?;
    let (model_w, model_h) = detector.model_size();
    println!("✓ Model loaded successfully!");
    println!("  Input size: {}x{}", model_w, model_h);
    println!();

    // Step 2: 加载图像
    println!("Step 2: Loading input image...");
    let orig_image = load_image(image_path)?;
    let (orig_w, orig_h) = orig_image.dimensions();
    println!("✓ Image loaded successfully!");
    println!();

    // Step 3: 预处理图像
    println!("Step 3: Preprocessing image...");
    let rknn_image = image_to_rknn_buffer(&orig_image, model_w, model_h);
    println!("✓ Image preprocessed (letterbox + resize)");
    println!();

    // Step 4: 运行推理
    println!("Step 4: Running inference...");
    let start = std::time::Instant::now();
    let detections = detector.detect(&rknn_image)?;
    let elapsed = start.elapsed();
    println!("✓ Inference completed in {:.2}ms", elapsed.as_secs_f64() * 1000.0);
    println!("  Detected {} objects", detections.len());
    println!();

    // Step 5: 后处理和绘制结果
    println!("Step 5: Post-processing and visualization...");

    // 计算坐标转换因子
    let scale_x = orig_w as f32 / model_w as f32;
    let scale_y = orig_h as f32 / model_h as f32;
    let scale = scale_x.max(scale_y);

    let offset_x = ((model_w as f32 - orig_w as f32 / scale) / 2.0).max(0.0) as i32;
    let offset_y = ((model_h as f32 - orig_h as f32 / scale) / 2.0).max(0.0) as i32;

    // 转换检测结果坐标到原始图像空间
    let mut scaled_detections = Vec::new();
    for det in &detections {
        let mut scaled_det = det.clone();

        // 移除 letterbox padding
        scaled_det.bbox.left = (det.bbox.left - offset_x).max(0);
        scaled_det.bbox.top = (det.bbox.top - offset_y).max(0);
        scaled_det.bbox.right = (det.bbox.right - offset_x).max(0);
        scaled_det.bbox.bottom = (det.bbox.bottom - offset_y).max(0);

        // 缩放到原始图像尺寸
        scaled_det.bbox.left = (scaled_det.bbox.left as f32 * scale) as i32;
        scaled_det.bbox.top = (scaled_det.bbox.top as f32 * scale) as i32;
        scaled_det.bbox.right = (scaled_det.bbox.right as f32 * scale) as i32;
        scaled_det.bbox.bottom = (scaled_det.bbox.bottom as f32 * scale) as i32;

        // 确保坐标在图像范围内
        scaled_det.bbox.left = scaled_det.bbox.left.max(0).min(orig_w as i32);
        scaled_det.bbox.top = scaled_det.bbox.top.max(0).min(orig_h as i32);
        scaled_det.bbox.right = scaled_det.bbox.right.max(0).min(orig_w as i32);
        scaled_det.bbox.bottom = scaled_det.bbox.bottom.max(0).min(orig_h as i32);

        scaled_detections.push(scaled_det);
    }

    // 加载字体
    let font_data = include_bytes!("../fonts/DejaVuSans.ttf");
    let font = FontRef::try_from_slice(font_data)
        .map_err(|e| format!("Failed to load font: {:?}", e))?;

    // 绘制检测结果
    let mut result_image = orig_image.to_rgb8();
    draw_detections(&mut result_image, &scaled_detections, &font);
    println!("✓ Detections drawn on image");
    println!();

    // Step 6: 保存结果
    println!("Step 6: Saving results...");
    result_image.save(&output_image)?;
    println!("✓ Output image saved: {}", output_image);

    save_results_txt(&output_txt, &scaled_detections)?;
    println!("✓ Text results saved: {}", output_txt);
    println!();

    // Summary
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║                       Summary                        ║");
    println!("╚══════════════════════════════════════════════════════╝");
    println!("  Input:       {}x{} pixels", orig_w, orig_h);
    println!("  Model:       {}x{} input", model_w, model_h);
    println!("  Detections:  {} objects found", detections.len());
    println!("  Inference:   {:.2}ms", elapsed.as_secs_f64() * 1000.0);
    println!("  Output:      {} (with bounding boxes)", output_image);
    println!();
    println!("✓ All done!");

    Ok(())
}
