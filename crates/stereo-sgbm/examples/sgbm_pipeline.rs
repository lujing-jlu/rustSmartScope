// Example: End-to-end stereo pipeline using camera-correction + stereo-sgbm
// Usage:
//   cargo run -p stereo-sgbm --features opencv --example sgbm_pipeline -- \
//       <params_dir> <left_image> <right_image> [out_prefix]

use camera_correction::{CameraParameters, StereoRectifier};
use opencv::{
    core::{self, Mat, Scalar, CV_16U, CV_8U, CV_8UC3, NORM_MINMAX},
    imgcodecs, imgproc,
    prelude::*,
};
use smartscope_core::{camera::CameraManager, config::SmartScopeConfig};
use turbojpeg::{Decompressor, Image as TjImage, PixelFormat as TjPixelFormat};

fn main() -> opencv::Result<()> {
    // CLI modes:
    // 1) Files:   sgbm_pipeline <params_dir> <left_image> <right_image> [out_prefix]
    // 2) Camera:  sgbm_pipeline --camera <params_dir> [out_prefix]
    let args: Vec<String> = std::env::args().collect();
    if args.len() >= 2 && args[1] == "--camera" {
        let params_dir = args
            .get(2)
            .map(String::as_str)
            .unwrap_or("./camera_parameters");
        let out_prefix = args.get(3).map(String::as_str).unwrap_or("cam_output");
        run_camera_pipeline(params_dir, out_prefix)
    } else {
        if args.len() < 4 {
            eprintln!(
                "Usage: {} <params_dir> <left_image> <right_image> [out_prefix]\n   or: {} --camera <params_dir> [out_prefix]",
                args.get(0).map(|s| s.as_str()).unwrap_or("sgbm_pipeline"),
                args.get(0).map(|s| s.as_str()).unwrap_or("sgbm_pipeline")
            );
            std::process::exit(2);
        }
        let params_dir = &args[1];
        let left_path = &args[2];
        let right_path = &args[3];
        let out_prefix = args.get(4).map(String::as_str).unwrap_or("output");
        run_file_pipeline(params_dir, left_path, right_path, out_prefix)
    }
}

fn run_file_pipeline(
    params_dir: &str,
    left_path: &str,
    right_path: &str,
    out_prefix: &str,
) -> opencv::Result<()> {
    // Load camera parameters
    let params = CameraParameters::from_directory(params_dir)
        .map_err(|e| opencv::Error::new(0, format!("load params error: {}", e)))?;
    let mut rectifier = StereoRectifier::from_parameters(&params)
        .map_err(|e| opencv::Error::new(0, format!("rectifier init error: {}", e)))?;

    // Read images (BGR)
    let left = imgcodecs::imread(left_path, imgcodecs::IMREAD_COLOR)?;
    let right = imgcodecs::imread(right_path, imgcodecs::IMREAD_COLOR)?;
    if left.empty() || right.empty() {
        return Err(opencv::Error::new(0, "Failed to read input images"));
    }

    // 保存左相机原始画面（旋转前）
    imgcodecs::imwrite(
        &format!("{}-left-original.png", out_prefix),
        &left,
        &core::Vector::new(),
    )?;

    let size = left.size()?;
    if size != right.size()? {
        return Err(opencv::Error::new(0, "Left/Right image size mismatch"));
    }

    // 1) 参数针对顺时针旋转后的画面标定：流程前先对输入进行顺时针旋转
    let left_rot = rotate_cw_90(&left)?;
    let right_rot = rotate_cw_90(&right)?;

    // Initialize rectification for rotated size
    let rsize = left_rot.size()?;
    rectifier
        .initialize_rectification(rsize.width as u32, rsize.height as u32)
        .map_err(|e| opencv::Error::new(0, format!("rectification error: {}", e)))?;

    // Rectify images
    let (left_rect, right_rect) = rectifier
        .rectify_images(&left_rot, &right_rot)
        .map_err(|e| opencv::Error::new(0, format!("rectify error: {}", e)))?;

    // 保存左相机矫正后画面
    imgcodecs::imwrite(
        &format!("{}-left-rectified.png", out_prefix),
        &left_rect,
        &core::Vector::new(),
    )?;

    // Q 矩阵
    let q = rectifier
        .get_rectification()
        .and_then(|r| Some(r.q.clone()))
        .ok_or_else(|| opencv::Error::new(0, "Missing Q matrix"))?;

    // 基础SGBM：全图计算，无预处理/后处理/ROI裁剪
    compute_and_save(&left_rect, &right_rect, &q, out_prefix)
}

fn run_camera_pipeline(params_dir: &str, out_prefix: &str) -> opencv::Result<()> {
    // Load parameters and prepare rectifier (image size known after first frame)
    let params = CameraParameters::from_directory(params_dir)
        .map_err(|e| opencv::Error::new(0, format!("load params error: {}", e)))?;
    let mut rectifier = StereoRectifier::from_parameters(&params)
        .map_err(|e| opencv::Error::new(0, format!("rectifier init error: {}", e)))?;

    // Start camera manager (auto-detect stereo cameras)
    let cfg = SmartScopeConfig::default();
    let mut cam = CameraManager::new(cfg);
    cam.initialize()
        .map_err(|e| opencv::Error::new(0, format!("camera init error: {}", e)))?;
    cam.start()
        .map_err(|e| opencv::Error::new(0, format!("camera start error: {}", e)))?;

    // Poll until we get both frames
    let start = std::time::Instant::now();
    let timeout = std::time::Duration::from_secs(5);
    let (mut left_mat, mut right_mat) = (Mat::default(), Mat::default());
    loop {
        cam.process_frames();
        if let (Some(l), Some(r)) = (cam.get_left_frame(), cam.get_right_frame()) {
            left_mat = decode_frame_to_bgr(&l)?;
            right_mat = decode_frame_to_bgr(&r)?;
            break;
        }
        if start.elapsed() > timeout {
            return Err(opencv::Error::new(0, "Timeout waiting for stereo frames"));
        }
        std::thread::sleep(std::time::Duration::from_millis(10));
    }

    // 保存左相机原始画面（旋转前）
    imgcodecs::imwrite(
        &format!("{}-left-original.png", out_prefix),
        &left_mat,
        &core::Vector::new(),
    )?;

    // 1) 参数针对顺时针旋转后的画面标定：流程前先对输入进行顺时针旋转
    let left_rot = rotate_cw_90(&left_mat)?;
    let right_rot = rotate_cw_90(&right_mat)?;

    // Initialize rectification for rotated size
    let size = left_rot.size()?;
    rectifier
        .initialize_rectification(size.width as u32, size.height as u32)
        .map_err(|e| opencv::Error::new(0, format!("rectification error: {}", e)))?;

    // Rectify
    let (left_rect, right_rect) = rectifier
        .rectify_images(&left_rot, &right_rot)
        .map_err(|e| opencv::Error::new(0, format!("rectify error: {}", e)))?;

    // 保存左相机矫正后画面
    imgcodecs::imwrite(
        &format!("{}-left-rectified.png", out_prefix),
        &left_rect,
        &core::Vector::new(),
    )?;

    // Q 矩阵
    let q = rectifier
        .get_rectification()
        .and_then(|r| Some(r.q.clone()))
        .ok_or_else(|| opencv::Error::new(0, "Missing Q matrix"))?;

    compute_and_save(&left_rect, &right_rect, &q, out_prefix)
}

fn compute_and_save(
    left_rect: &Mat,
    right_rect: &Mat,
    q: &Mat,
    out_prefix: &str,
) -> opencv::Result<()> {
    let mut sgbm = stereo_sgbm::StereoSgbm::with_defaults()?;
    let disp32f = sgbm.compute_disparity(left_rect, right_rect)?;
    let depth32f = sgbm.compute_depth_from_disparity(&disp32f, q)?;

    // Save grayscale visualizations
    save_normalized_8u(&disp32f, &format!("{}-disparity-vis.png", out_prefix))?;
    save_normalized_8u(&depth32f, &format!("{}-depth-vis.png", out_prefix))?;

    // Save colorized visualizations
    save_colormap(
        &disp32f,
        &format!("{}-disparity-color.png", out_prefix),
        imgproc::COLORMAP_JET,
    )?;
    save_colormap(
        &depth32f,
        &format!("{}-depth-color.png", out_prefix),
        imgproc::COLORMAP_INFERNO,
    )?;

    // Raw depth as 16U (scaled by max range)
    save_depth_16u(
        &depth32f,
        &format!("{}-depth-raw16.png", out_prefix),
        100_000.0,
    )?;
    println!("Wrote outputs with prefix '{}'.", out_prefix);
    Ok(())
}

fn save_normalized_8u(src32f: &Mat, path: &str) -> opencv::Result<()> {
    if src32f.empty() {
        return Ok(());
    }
    let mut dst8u = Mat::default();
    core::normalize(
        src32f,
        &mut dst8u,
        0.0,
        255.0,
        NORM_MINMAX,
        CV_8U,
        &Mat::default(),
    )?;
    imgcodecs::imwrite(path, &dst8u, &core::Vector::new())?;
    Ok(())
}

fn save_depth_16u(src32f: &Mat, path: &str, max_mm: f64) -> opencv::Result<()> {
    if src32f.empty() {
        return Ok(());
    }
    // Clamp negatives and cap to max_mm, then scale to 16U
    let mut clamped = Mat::default();
    // max(src, 0)
    core::max(src32f, &Scalar::all(0.0), &mut clamped)?;

    // scale: mm -> 16U; choose scale = 65535 / max_mm
    let scale = if max_mm > 1.0 { 65535.0 / max_mm } else { 1.0 };
    let mut depth16 = Mat::default();
    clamped.convert_to(&mut depth16, CV_16U, scale, 0.0)?;
    imgcodecs::imwrite(path, &depth16, &core::Vector::new())?;
    Ok(())
}

fn save_colormap(src32f: &Mat, path: &str, cmap: i32) -> opencv::Result<()> {
    if src32f.empty() {
        return Ok(());
    }
    // Normalize to 8U then apply colormap
    let mut norm8 = Mat::default();
    core::normalize(
        src32f,
        &mut norm8,
        0.0,
        255.0,
        NORM_MINMAX,
        CV_8U,
        &Mat::default(),
    )?;
    let mut color = Mat::default();
    imgproc::apply_color_map(&norm8, &mut color, cmap)?;
    imgcodecs::imwrite(path, &color, &core::Vector::new())?;
    Ok(())
}

// 基础SGBM：不做ROI裁剪与后处理

fn rotate_cw_90(src: &Mat) -> opencv::Result<Mat> {
    let mut dst = Mat::default();
    core::rotate(src, &mut dst, core::ROTATE_90_CLOCKWISE)?;
    Ok(dst)
}

fn decode_frame_to_bgr(frame: &smartscope_core::camera::VideoFrame) -> opencv::Result<Mat> {
    // Currently handle MJPEG via turbojpeg; other formats could be added as needed
    let mut decomp =
        Decompressor::new().map_err(|e| opencv::Error::new(0, format!("tj init: {}", e)))?;
    let header = decomp
        .read_header(&frame.data)
        .map_err(|e| opencv::Error::new(0, format!("tj header: {}", e)))?;
    let (w, h) = (header.width as i32, header.height as i32);
    let mut rgb = vec![0u8; (w * h * 3) as usize];
    let output = TjImage {
        pixels: rgb.as_mut_slice(),
        width: w as usize,
        pitch: (w * 3) as usize,
        height: h as usize,
        format: TjPixelFormat::RGB,
    };
    decomp
        .decompress(&frame.data, output)
        .map_err(|e| opencv::Error::new(0, format!("tj decompress: {}", e)))?;

    // Copy into Mat and convert to BGR
    let mut rgb_mat = core::Mat::zeros(h, w, CV_8UC3)?.to_mat()?;
    unsafe {
        let dst_ptr = rgb_mat
            .ptr_mut(0)
            .map_err(|e| opencv::Error::new(0, format!("mat ptr: {}", e)))?;
        std::ptr::copy_nonoverlapping(rgb.as_ptr(), dst_ptr, rgb.len());
    }
    let mut bgr = Mat::default();
    imgproc::cvt_color(&rgb_mat, &mut bgr, imgproc::COLOR_RGB2BGR, 0)?;
    Ok(bgr)
}

#[inline]
fn wrapping_i32(v: i32) -> i32 {
    v
}
