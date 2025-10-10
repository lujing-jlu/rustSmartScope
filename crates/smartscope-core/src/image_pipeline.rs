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
    #[allow(dead_code)]
    stereo_rectifier: Option<StereoRectifier>,
    /// 视频变换处理器
    video_transform: Arc<RwLock<VideoTransformProcessor>>,
    /// 是否启用畸变校正
    distortion_correction_enabled: bool,
    /// 缓冲池，用于减少内存分配
    #[allow(dead_code)]
    buffer_pool: Arc<std::sync::Mutex<Vec<Vec<u8>>>>,
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

                    // 调整相机内参以适配旋转90度的图像（标定时相机是旋转的）
                    // 对于逆时针旋转90度：(x, y) -> (y, height - x)
                    // 内参调整：cx' = cy, cy' = width - cx (width和height是旋转前的)
                    let left_intrinsics_rotated = Self::rotate_intrinsics_90_ccw(&stereo.left_intrinsics);
                    let right_intrinsics_rotated = Self::rotate_intrinsics_90_ccw(&stereo.right_intrinsics);

                    // 创建左右相机的畸变校正器（使用调整后的内参）
                    let left_corrector = DistortionCorrector::new(left_intrinsics_rotated);
                    let right_corrector = DistortionCorrector::new(right_intrinsics_rotated);

                    // 创建立体校正器
                    let rectifier = StereoRectifier::from_parameters(&params)
                        .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to create stereo rectifier: {}", e)))?;

                    info!("Distortion correctors with rotated intrinsics and stereo rectifier initialized");
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
            buffer_pool: Arc::new(std::sync::Mutex::new(Vec::new())),
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

    /// 旋转相机内参以适配逆时针90度旋转的图像
    ///
    /// 标定时相机顺时针旋转90度拍摄，所以需要将内参调整为逆时针90度
    /// 旋转变换：(x, y) -> (y, height - x)
    /// 内参调整：fx' = fy, fy' = fx, cx' = cy, cy' = height - cx
    fn rotate_intrinsics_90_ccw(intrinsics: &camera_correction::parameters::CameraIntrinsics) -> camera_correction::parameters::CameraIntrinsics {
        use camera_correction::parameters::{CameraIntrinsics, CameraMatrix};

        let matrix = &intrinsics.matrix;

        // 注意：这里假设标定图像的原始尺寸
        // 由于我们在initialize_maps时会使用实际的旋转后尺寸，
        // 这里只需要交换 fx/fy 和 cx/cy
        let rotated_matrix = CameraMatrix {
            fx: matrix.fy,  // 交换焦距
            fy: matrix.fx,
            cx: matrix.cy,  // 交换主点
            cy: matrix.cx,
        };

        // 畸变系数保持不变
        CameraIntrinsics::new(rotated_matrix, intrinsics.distortion.clone())
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
        let (rgb_data, mut width, mut height) = self.decode_mjpeg(mjpeg_data)?;

        // 2. 应用畸变校正（如果启用且可用）
        // 注意：畸变校正包含旋转补偿，但最终会旋转回来，所以宽高不变
        let corrected_data = if self.distortion_correction_enabled {
            self.apply_distortion_correction(&rgb_data, width, height, is_left_camera)?
        } else {
            rgb_data
        };

        // 3. 应用 RGA 视频变换（旋转、翻转、反色）
        // 需要检查是否有90度或270度旋转，这会交换宽高
        let rotation = {
            let video_transform = self.video_transform.read().unwrap();
            video_transform.get_config().rotation_degrees
        }; // 锁自动释放

        // 传递所有权，避免不必要的复制
        let final_data = self.apply_video_transforms(corrected_data, width, height)?;

        // 如果用户旋转了90度或270度，需要交换宽高
        if rotation == 90 || rotation == 270 {
            std::mem::swap(&mut width, &mut height);
        }

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

        Ok((rgb_buffer, width, height))
    }

    /// 应用畸变校正（使用预先调整好的旋转内参）
    ///
    /// 超高性能优化版本：
    /// 1. 直接在RGB数据上操作，完全避免RGB↔BGR颜色转换
    /// 2. 使用OpenCV的RGB支持特性，零拷贝处理
    /// 3. 采用最近邻插值(FAST_INTERPOLATION)获得最佳性能
    /// 4. 消除了所有不必要的内存分配和拷贝
    fn apply_distortion_correction(
        &mut self,
        rgb_data: &[u8],
        width: u32,
        height: u32,
        is_left_camera: bool,
    ) -> Result<Vec<u8>> {
        // 获取对应相机的校正器
        let corrector = if is_left_camera {
            &mut self.distortion_corrector_left
        } else {
            &mut self.distortion_corrector_right
        };

        if let Some(corrector) = corrector {
            // 初始化校正映射表（如果还未初始化）
            // 使用原始图像尺寸（已经是旋转后的方向）
            if !corrector.are_maps_initialized() {
                corrector.initialize_maps(width, height)
                    .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to initialize distortion maps: {}", e)))?;
                info!("Initialized distortion correction maps for {}x{}", width, height);
            }

            // 使用优化的RGB直接校正方法
            let corrected_data = corrector.correct_distortion_rgb(rgb_data, width, height)
                .map_err(|e| SmartScopeError::VideoProcessingError(format!("Distortion correction failed: {}", e)))?;

            Ok(corrected_data)
        } else {
            // 没有校正器，返回原始数据（避免不必要的拷贝）
            Ok(rgb_data.to_vec())
        }
    }

    /// 应用视频变换（旋转、翻转、反色）- 优化版本
    fn apply_video_transforms(
        &self,
        rgb_data: Vec<u8>,  // 接收所有权，避免不必要的复制
        width: u32,
        height: u32,
    ) -> Result<Vec<u8>> {
        let video_transform = self.video_transform.read().unwrap();
        let transforms = video_transform.get_config().get_transforms();

        // 如果没有变换，直接返回原数据（零复制）
        if transforms.is_empty() {
            return Ok(rgb_data);
        }

        // 使用 RGA 硬件加速（如果可用）
        if video_transform.is_rga_available() {
            video_transform.apply_transforms(
                &rgb_data,
                width,
                height,
                RgaFormat::Rgb888,
                &transforms,
            )
        } else {
            // RGA 不可用，返回原始数据（零复制）
            warn!("RGA not available, video transforms skipped");
            Ok(rgb_data)
        }
    }

    /// 获取或创建缓冲区，减少内存分配开销
    #[allow(dead_code)]
    fn get_buffer_from_pool(&self, size: usize) -> Vec<u8> {
        let mut pool = self.buffer_pool.lock().unwrap();
        if let Some(mut buffer) = pool.pop() {
            if buffer.capacity() >= size {
                buffer.clear();
                buffer.resize(size, 0);
                return buffer;
            }
        }

        // 创建新缓冲区
        vec![0u8; size]
    }

    /// 将缓冲区返回到池中
    #[allow(dead_code)]
    fn return_buffer_to_pool(&self, mut buffer: Vec<u8>) {
        let mut pool = self.buffer_pool.lock().unwrap();
        if pool.len() < 5 { // 最多保留5个缓冲区
            buffer.clear();
            pool.push(buffer);
        }
    }

    /// 带性能监控的处理函数
    pub fn process_mjpeg_frame_with_timing(
        &mut self,
        mjpeg_data: &[u8],
        is_left_camera: bool,
    ) -> Result<(Vec<u8>, u32, u32)> {
        use std::time::Instant;

        let start_time = Instant::now();

        // 1. 解码 MJPEG 到 RGB
        let decode_start = Instant::now();
        let (rgb_data, mut width, mut height) = self.decode_mjpeg(mjpeg_data)?;
        let decode_time = decode_start.elapsed();

        // 2. 应用畸变校正
        let correction_start = Instant::now();
        let corrected_data = if self.distortion_correction_enabled {
            self.apply_distortion_correction(&rgb_data, width, height, is_left_camera)?
        } else {
            rgb_data
        };
        let correction_time = correction_start.elapsed();

        // 3. 应用 RGA 视频变换
        let transform_start = Instant::now();
        let rotation = {
            let video_transform = self.video_transform.read().unwrap();
            video_transform.get_config().rotation_degrees
        };

        let final_data = self.apply_video_transforms(corrected_data, width, height)?;
        let transform_time = transform_start.elapsed();

        // 如果用户旋转了90度或270度，需要交换宽高
        if rotation == 90 || rotation == 270 {
            std::mem::swap(&mut width, &mut height);
        }

        let total_time = start_time.elapsed();

        // 性能日志（每100帧输出一次）
        static FRAME_COUNT: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);
        let count = FRAME_COUNT.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        if count % 100 == 0 {
            info!(
                "Frame {}: decode={:.2}ms, correction={:.2}ms, transform={:.2}ms, total={:.2}ms, fps={:.1}",
                count,
                decode_time.as_millis(),
                correction_time.as_millis(),
                transform_time.as_millis(),
                total_time.as_millis(),
                1000.0 / total_time.as_secs_f32()
            );
        }

        Ok((final_data, width, height))
    }

    /// 将 RGB 数据转换为 OpenCV Mat (BGR格式) - 优化版本
    #[allow(dead_code)]
    fn rgb_to_mat(&self, rgb_data: &[u8], width: u32, height: u32) -> Result<Mat> {
        use opencv::core::{Mat_AUTO_STEP, CV_8UC3};

        // 从 RGB 数据创建 Mat（使用 unsafe 接口以获得更好的性能）
        let rgb_mat = unsafe {
            Mat::new_rows_cols_with_data_unsafe(
                height as i32,
                width as i32,
                CV_8UC3,
                rgb_data.as_ptr() as *mut std::ffi::c_void,
                Mat_AUTO_STEP,
            ).map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to create Mat from RGB data: {}", e)))?
        };

        // RGB → BGR 颜色转换（使用 OpenCV 优化的函数）
        let mut bgr_mat = Mat::default();
        opencv::imgproc::cvt_color(&rgb_mat, &mut bgr_mat, opencv::imgproc::COLOR_RGB2BGR, 0)
            .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to convert RGB to BGR: {}", e)))?;

        Ok(bgr_mat)
    }

    /// 将 OpenCV Mat (BGR格式) 转换为 RGB 数据 - 优化版本
    #[allow(dead_code)]
    fn mat_to_rgb(&self, mat: &Mat) -> Result<Vec<u8>> {
        let height = mat.rows();
        let width = mat.cols();

        // BGR → RGB 颜色转换（使用 OpenCV 优化的函数）
        let mut rgb_mat = Mat::default();
        opencv::imgproc::cvt_color(mat, &mut rgb_mat, opencv::imgproc::COLOR_BGR2RGB, 0)
            .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to convert BGR to RGB: {}", e)))?;

        // 检查是否连续存储
        if rgb_mat.is_continuous() {
            // 连续存储，直接复制整个数据块（最快）
            let data_size = (width * height * 3) as usize;
            let mut rgb_data = vec![0u8; data_size];

            unsafe {
                let src_ptr = rgb_mat.ptr(0).map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to get Mat data pointer: {}", e)))?;
                std::ptr::copy_nonoverlapping(src_ptr, rgb_data.as_mut_ptr(), data_size);
            }

            Ok(rgb_data)
        } else {
            // 非连续存储，按行复制
            let mut rgb_data = Vec::with_capacity((width * height * 3) as usize);

            for row in 0..height {
                let row_data = rgb_mat.at_row::<u8>(row)
                    .map_err(|e| SmartScopeError::VideoProcessingError(format!("Failed to get row data: {}", e)))?;
                rgb_data.extend_from_slice(row_data);
            }

            Ok(rgb_data)
        }
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
