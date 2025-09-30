//! Utility functions for camera correction

use crate::error::CorrectionResult;

#[cfg(feature = "opencv")]
use opencv::{
    core::{Mat, Size},
    imgcodecs,
    prelude::*,
};

/// Load an image from file
#[cfg(feature = "opencv")]
pub fn load_image<P: AsRef<std::path::Path>>(path: P) -> CorrectionResult<Mat> {
    let image = imgcodecs::imread(path.as_ref().to_str().unwrap(), imgcodecs::IMREAD_COLOR)?;
    if image.empty() {
        return Err(crate::error::CorrectionError::ImageCorrectionError(
            "Failed to load image".to_string(),
        ));
    }
    Ok(image)
}

/// Save an image to file
#[cfg(feature = "opencv")]
pub fn save_image<P: AsRef<std::path::Path>>(image: &Mat, path: P) -> CorrectionResult<()> {
    let params = opencv::core::Vector::<i32>::new();
    let success = imgcodecs::imwrite(path.as_ref().to_str().unwrap(), image, &params)?;
    if !success {
        return Err(crate::error::CorrectionError::ImageCorrectionError(
            "Failed to save image".to_string(),
        ));
    }
    Ok(())
}

/// Get image size as (width, height)
#[cfg(feature = "opencv")]
pub fn get_image_size(image: &Mat) -> CorrectionResult<(u32, u32)> {
    let size = image.size()?;
    Ok((size.width as u32, size.height as u32))
}

/// Create a test image for debugging
#[cfg(feature = "opencv")]
pub fn create_test_image(width: u32, height: u32) -> CorrectionResult<Mat> {
    let _size = Size::new(width as i32, height as i32);
    let mut image = Mat::zeros(height as i32, width as i32, opencv::core::CV_8UC3)?.to_mat()?;

    // Fill with a simple pattern
    for y in 0..height {
        for x in 0..width {
            let pixel = opencv::core::Vec3b::from_array([
                (x % 256) as u8,
                (y % 256) as u8,
                ((x + y) % 256) as u8,
            ]);
            *image.at_2d_mut::<opencv::core::Vec3b>(y as i32, x as i32)? = pixel;
        }
    }

    Ok(image)
}

/// Print matrix information for debugging
#[cfg(feature = "opencv")]
pub fn print_matrix_info(name: &str, matrix: &Mat) {
    if matrix.empty() {
        println!("{}: empty matrix", name);
        return;
    }

    let size = matrix.size().unwrap_or_default();
    let depth = matrix.depth();
    let channels = matrix.channels();

    println!(
        "{}: {}x{} (depth: {:?}, channels: {})",
        name, size.width, size.height, depth, channels
    );
}

/// Convert OpenCV Mat to raw bytes
#[cfg(feature = "opencv")]
pub fn mat_to_bytes(mat: &Mat) -> CorrectionResult<Vec<u8>> {
    let bytes_slice = mat.data_bytes()?;
    Ok(bytes_slice.to_vec())
}

/// Convert raw bytes to OpenCV Mat
#[cfg(feature = "opencv")]
pub fn bytes_to_mat(
    bytes: &[u8],
    _width: i32,
    _height: i32,
    _channels: i32,
) -> CorrectionResult<Mat> {
    let buf = opencv::core::Vector::from_slice(bytes);
    let mat = imgcodecs::imdecode(&buf, imgcodecs::IMREAD_UNCHANGED)?;
    Ok(mat)
}

/// Create a simple test pattern image
#[cfg(feature = "opencv")]
pub fn create_checkerboard_image(
    width: u32,
    height: u32,
    square_size: u32,
) -> CorrectionResult<Mat> {
    let _size = Size::new(width as i32, height as i32);
    let mut image = Mat::zeros(height as i32, width as i32, opencv::core::CV_8UC3)?.to_mat()?;

    for y in 0..height {
        for x in 0..width {
            let square_x = (x / square_size) % 2;
            let square_y = (y / square_size) % 2;
            let is_white = (square_x + square_y) % 2 == 0;

            let pixel = if is_white {
                opencv::core::Vec3b::from_array([255, 255, 255])
            } else {
                opencv::core::Vec3b::from_array([0, 0, 0])
            };

            *image.at_2d_mut::<opencv::core::Vec3b>(y as i32, x as i32)? = pixel;
        }
    }

    Ok(image)
}

/// Create a stereo test image pair
#[cfg(feature = "opencv")]
pub fn create_stereo_test_images(width: u32, height: u32) -> CorrectionResult<(Mat, Mat)> {
    let left_image = create_checkerboard_image(width, height, 50)?;
    let mut right_image = left_image.clone();

    // Add some horizontal offset to simulate stereo disparity
    let offset = 20;
    for y in 0..height {
        for x in offset..width {
            let src_pixel =
                *left_image.at_2d::<opencv::core::Vec3b>(y as i32, (x - offset) as i32)?;
            *right_image.at_2d_mut::<opencv::core::Vec3b>(y as i32, x as i32)? = src_pixel;
        }
    }

    Ok((left_image, right_image))
}
