//! 图像处理管道
//!
//! 完整的图像处理流程：
//! 1. MJPEG 解码 (turbojpeg) → RGB
//! 2. 畸变校正 (camera-correction + opencv) - 最优先
//! 3. RGA 变换 (旋转、翻转、反色)

use crate::error::{SmartScopeError, Result};
use crate::video_transform::VideoTransformProcessor;
use camera_correction::{CameraParameters, DistortionCorrector, StereoRectifier};
use opencv::{core::Mat, prelude::*};
use rockchip_rga::RgaFormat;
use std::sync::{Arc, RwLock};
use tracing::{debug, info, warn};
use turbojpeg::{Decompressor, Image, PixelFormat};

/// 图像处理管道
pub struct ImagePipeline {
    /// MJPEG 解码器
    decompressor: Decompressor,
    /// 畸变校正器（左相机）
    distortion_corrector_left: Option<DistortionCorrector>,
    /// 畸变校正器（右相机）
    distortion_corrector_right: Option<DistortionCorrector>,
    /// 立体校正器
    stereo_rectifier: Option<StereoRectifier>,
    /// 视频变换处理器
    video_transform: Arc<RwLock<VideoTransformProcessor>>,
    /// 是否启用畸变校正
    distortion_correction_enabled: bool,
}

impl ImagePipeline {
    /// 创建新的图像处理管道
    pub fn new(
        camera_params_dir: Option<&str>,
        video_transform: Arc<RwLock<VideoTransformProcessor>>,
    ) -> Result<Self> {
        // 创建 MJPEG 解码器
        let decompressor = Decompressor::new()
            .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to create MJPEG decompressor: {}", e)))?;

        // 加载相机参数和创建畸变校正器、立体校正器
        let (distortion_corrector_left, distortion_corrector_right, stereo_rectifier) = if let Some(params_dir) = camera_params_dir {
            match CameraParameters::from_directory(params_dir) {
                Ok(params) => {
                    let stereo = params.get_stereo_parameters()
                        .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to get stereo parameters: {}", e)))?;

                    // 创建左右相机的畸变校正器
                    let left_corrector = DistortionCorrector::new(stereo.left_intrinsics.clone());
                    let right_corrector = DistortionCorrector::new(stereo.right_intrinsics.clone());

                    // 创建立体校正器
                    let rectifier = StereoRectifier::from_parameters(&params)
                        .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to create stereo rectifier: {}", e)))?;

                    info!("Distortion correctors and stereo rectifier initialized for stereo cameras");
                    (Some(left_corrector), Some(right_corrector), Some(rectifier))
                }
                Err(e) => {
                    warn!("Failed to load camera parameters, correction disabled: {}", e);
                    (None, None, None)
                }
            }
        } else {
            info!("No camera parameters directory provided, correction disabled");
            (None, None, None)
        };

        Ok(Self {
            decompressor,
            distortion_corrector_left,
            distortion_corrector_right,
            stereo_rectifier,
            video_transform,
            distortion_correction_enabled: true,
        })
    }

    /// 启用/禁用畸变校正
    pub fn set_distortion_correction_enabled(&mut self, enabled: bool) {
        self.distortion_correction_enabled = enabled;
        debug!("Distortion correction: {}", if enabled { "enabled" } else { "disabled" });
    }

    /// 检查畸变校正是否可用
    pub fn is_distortion_correction_available(&self) -> bool {
        self.distortion_corrector_left.is_some() || self.distortion_corrector_right.is_some()
    }

    /// 处理 MJPEG 帧（完整流程）
    ///
    /// 流程：MJPEG解码 → 畸变校正（如果启用） → RGA变换 → 返回RGB数据
    pub fn process_mjpeg_frame(
        &mut self,
        mjpeg_data: &[u8],
        is_left_camera: bool,
    ) -> Result<(Vec<u8>, u32, u32)> {
        // 1. 解码 MJPEG 到 RGB
        let (rgb_data, width, height) = self.decode_mjpeg(mjpeg_data)?;

        // 2. 应用畸变校正（如果启用且可用）
        let corrected_data = if self.distortion_correction_enabled {
            self.apply_distortion_correction(&rgb_data, width, height, is_left_camera)?
        } else {
            rgb_data
        };

        // 3. 应用 RGA 变换（旋转、翻转、反色）
        let final_data = self.apply_video_transforms(&corrected_data, width, height)?;

        Ok((final_data, width, height))
    }

    /// 解码 MJPEG 到 RGB
    fn decode_mjpeg(&mut self, mjpeg_data: &[u8]) -> Result<(Vec<u8>, u32, u32)> {
        // 读取图像头信息
        let header = self.decompressor
            .read_header(mjpeg_data)
            .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to read MJPEG header: {}", e)))?;

        let width = header.width as u32;
        let height = header.height as u32;

        // 创建输出缓冲区 (RGB: 3 bytes per pixel)
        let buffer_size = (width * height * 3) as usize;
        let mut rgb_buffer = vec![0u8; buffer_size];

        // 解码到 RGB
        let output_image = Image {
            pixels: &mut rgb_buffer[..],
            width: width as usize,
            pitch: (width * 3) as usize,  // RGB: 3 bytes per pixel
            height: height as usize,
            format: PixelFormat::RGB,
        };

        self.decompressor
            .decompress(mjpeg_data, output_image)
            .map_err(|e| SmartScopeError::VideoProcessingError(format!("MJPEG decode failed: {}", e)))?;

        debug!("Decoded MJPEG frame: {}x{}", width, height);
        Ok((rgb_buffer, width, height))
    }

    /// 应用畸变校正（包含旋转补偿）
    ///
    /// 流程：
    /// 1. RGB → Mat
    /// 2. 逆时针旋转90度（补偿标定时的顺时针旋转）
    /// 3. 应用畸变校正
    /// 4. 顺时针旋转90度（恢复正常方向）
    /// 5. Mat → RGB
    fn apply_distortion_correction(
        &mut self,
        rgb_data: &[u8],
        width: u32,
        height: u32,
        is_left_camera: bool,
    ) -> Result<Vec<u8>> {
        // 步骤1: 将 RGB 数据转换为 OpenCV Mat
        let mat = self.rgb_to_mat(rgb_data, width, height)?;

        // 步骤2: 逆时针旋转90度（补偿标定时相机的顺时针90度旋转）
        let rotated_mat = self.rotate_mat_counter_clockwise_90(&mat)?;
        debug!("Rotated image counter-clockwise 90° for calibration compensation");

        // 获取对应相机的校正器
        let corrector = if is_left_camera {
            &mut self.distortion_corrector_left
        } else {
            &mut self.distortion_corrector_right
        };

        if let Some(corrector) = corrector {
            // 注意：旋转后图像尺寸会交换（width ↔ height）
            let rotated_width = rotated_mat.cols() as u32;
            let rotated_height = rotated_mat.rows() as u32;

            // 初始化校正映射表（如果还未初始化）
            // 使用旋转后的尺寸初始化
            if !corrector.are_maps_initialized() {
                corrector.initialize_maps(rotated_width, rotated_height)
                    .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to initialize distortion maps: {}", e)))?;
                info!("Initialized distortion correction maps for {}x{} (rotated)", rotated_width, rotated_height);
            }

            // 步骤3: 应用畸变校正
            let corrected_mat = corrector.correct_distortion(&rotated_mat)
                .map_err(|e| SmartScopeError::VideoProcessingError(format!("Distortion correction failed: {}", e)))?;
            debug!("Distortion correction applied for {} camera", if is_left_camera { "left" } else { "right" });

            // 步骤4: 顺时针旋转90度（恢复到正常方向）
            let final_mat = self.rotate_mat_clockwise_90(&corrected_mat)?;
            debug!("Rotated image clockwise 90° to restore normal orientation");

            // 步骤5: 转换回 RGB 数据
            let final_data = self.mat_to_rgb(&final_mat)?;

            Ok(final_data)
        } else {
            // 没有校正器，需要将旋转后的图像转回来
            let final_mat = self.rotate_mat_clockwise_90(&rotated_mat)?;
            let final_data = self.mat_to_rgb(&final_mat)?;
            Ok(final_data)
        }
    }

    /// 应用视频变换（旋转、翻转、反色）
    fn apply_video_transforms(
        &self,
        rgb_data: &[u8],
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let video_transform = self.video_transform.read().unwrap();
        let transforms = video_transform.get_config().get_transforms();

        if transforms.is_empty() {
            return Ok(rgb_data.to_vec());
        }

        // 使用 RGA 硬件加速（如果可用）
        if video_transform.is_rga_available() {
            debug!("Applying {} RGA transforms", transforms.len());
            video_transform.apply_transforms(
                rgb_data,
                width,
                height,
                RgaFormat::Rgb888,
                &transforms,
            )
        } else {
            // RGA 不可用，返回原始数据
            warn!("RGA not available, video transforms skipped");
            Ok(rgb_data.to_vec())
        }
    }

    /// 将 RGB 数据转换为 OpenCV Mat (BGR格式)
    fn rgb_to_mat(&self, rgb_data: &[u8], width: u32, height: u32) -> Result<Mat> {
        use opencv::core::{Vec3b, CV_8UC3};

        let mut mat = Mat::new_rows_cols_with_default(
            height as i32,
            width as i32,
            CV_8UC3,
            opencv::core::Scalar::default(),
        ).map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to create Mat: {}", e)))?;

        // RGB 转 BGR (OpenCV使用BGR格式)
        let pixel_count = (width * height) as usize;
        for i in 0..pixel_count {
            let y = (i as u32 / width) as i32;
            let x = (i as u32 % width) as i32;
            let idx = i * 3;

            if idx + 2 < rgb_data.len() {
                let pixel = Vec3b::from_array([
                    rgb_data[idx + 2], // B
                    rgb_data[idx + 1], // G
                    rgb_data[idx],     // R
                ]);
                *mat.at_2d_mut::<Vec3b>(y, x)
                    .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to set pixel: {}", e)))? = pixel;
            }
        }

        Ok(mat)
    }

    /// 将 OpenCV Mat (BGR格式) 转换为 RGB 数据
    fn mat_to_rgb(&self, mat: &Mat) -> Result<Vec<u8>> {
        use opencv::core::Vec3b;

        let height = mat.rows();
        let width = mat.cols();
        let mut rgb_data = Vec::with_capacity((width * height * 3) as usize);

        // BGR 转 RGB
        for y in 0..height {
            for x in 0..width {
                let pixel = mat.at_2d::<Vec3b>(y, x)
                    .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to get pixel: {}", e)))?;

                // OpenCV 是 BGR，转换为 RGB
                rgb_data.push(pixel[2]); // R
                rgb_data.push(pixel[1]); // G
                rgb_data.push(pixel[0]); // B
            }
        }

        Ok(rgb_data)
    }

    /// 旋转 OpenCV Mat（逆时针90度）
    fn rotate_mat_counter_clockwise_90(&self, mat: &Mat) -> Result<Mat> {
        use opencv::core::ROTATE_90_COUNTERCLOCKWISE;
        let mut rotated = Mat::default();
        opencv::core::rotate(mat, &mut rotated, ROTATE_90_COUNTERCLOCKWISE)
            .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to rotate image counter-clockwise: {}", e)))?;
        Ok(rotated)
    }

    /// 旋转 OpenCV Mat（顺时针90度）
    fn rotate_mat_clockwise_90(&self, mat: &Mat) -> Result<Mat> {
        use opencv::core::ROTATE_90_CLOCKWISE;
        let mut rotated = Mat::default();
        opencv::core::rotate(mat, &mut rotated, ROTATE_90_CLOCKWISE)
            .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to rotate image clockwise: {}", e)))?;
        Ok(rotated)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::video_transform::VideoTransformProcessor;

    #[test]
    fn test_pipeline_creation_without_params() {
        let processor = Arc::new(RwLock::new(VideoTransformProcessor::new()));
        let result = ImagePipeline::new(None, processor);
        assert!(result.is_ok());
    }

    #[test]
    fn test_pipeline_creation_with_invalid_params() {
        let processor = Arc::new(RwLock::new(VideoTransformProcessor::new()));
        let result = ImagePipeline::new(Some("/nonexistent/path"), processor);
        assert!(result.is_ok()); // 应该成功创建，但校正功能不可用

        let pipeline = result.unwrap();
        assert!(!pipeline.is_distortion_correction_available());
    }
}
