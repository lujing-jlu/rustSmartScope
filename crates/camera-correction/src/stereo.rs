//! Stereo rectification for camera pairs

use crate::error::{CorrectionError, CorrectionResult};
use crate::parameters::{CameraMatrix, CameraParameters, DistortionCoeffs, StereoParameters};
use stereo_calibration_reader as calib;
#[cfg(feature = "opencv")]
use stereo_rectifier as rect;

#[cfg(feature = "opencv")]
use opencv::{
    calib3d,
    core::{Mat, Size, CV_32FC1, CV_64F},
    prelude::*,
};

/// Stereo rectifier for camera pairs
pub struct StereoRectifier {
    /// Stereo parameters
    stereo_params: StereoParameters,
    /// Rectification parameters (if initialized)
    #[cfg(feature = "opencv")]
    rectification: Option<RectificationData>,
}

/// Rectification data for stereo cameras
#[derive(Debug, Clone)]
#[cfg(feature = "opencv")]
pub struct RectificationData {
    /// Left camera rectification rotation matrix
    pub r1: Mat,
    /// Right camera rectification rotation matrix
    pub r2: Mat,
    /// Left camera projection matrix
    pub p1: Mat,
    /// Right camera projection matrix
    pub p2: Mat,
    /// Disparity-to-depth mapping matrix
    pub q: Mat,
    /// Left camera ROI
    pub roi1: opencv::core::Rect,
    /// Right camera ROI
    pub roi2: opencv::core::Rect,
    /// Left camera remapping maps
    pub map1x: Mat,
    pub map1y: Mat,
    /// Right camera remapping maps
    pub map2x: Mat,
    pub map2y: Mat,
}

impl StereoRectifier {
    /// Create a new stereo rectifier from camera parameters
    pub fn from_parameters(params: &CameraParameters) -> CorrectionResult<Self> {
        Ok(Self {
            stereo_params: params.get_stereo_parameters()?.clone(),
            #[cfg(feature = "opencv")]
            rectification: None,
        })
    }

    /// Create a new stereo rectifier with explicit parameters
    pub fn new(stereo_params: StereoParameters) -> Self {
        Self {
            stereo_params,
            #[cfg(feature = "opencv")]
            rectification: None,
        }
    }

    /// Initialize rectification for specific image size
    #[cfg(feature = "opencv")]
    pub fn initialize_rectification(&mut self, width: u32, height: u32) -> CorrectionResult<()> {
        let size = Size::new(width as i32, height as i32);
        if size.width <= 0 || size.height <= 0 {
            return Err(CorrectionError::InvalidImageSize {
                width: size.width,
                height: size.height,
            });
        }

        log::info!(
            "Initializing stereo rectification for size: {}x{}",
            width,
            height
        );

        // 直接复用 stereo-rectifier 计算与映射生成，减少重复实现
        let lmat = &self.stereo_params.left_intrinsics.matrix;
        let rmat = &self.stereo_params.right_intrinsics.matrix;
        let ldist = &self.stereo_params.left_intrinsics.distortion;
        let rdist = &self.stereo_params.right_intrinsics.distortion;
        let ext = &self.stereo_params.right_extrinsics;

        let cal = calib::StereoCalibration {
            left: calib::CameraIntrinsics {
                camera_matrix: [
                    [lmat.fx, 0.0, lmat.cx],
                    [0.0, lmat.fy, lmat.cy],
                    [0.0, 0.0, 1.0],
                ],
                dist_coeffs: [ldist.k1, ldist.k2, ldist.p1, ldist.p2, ldist.k3],
            },
            right: calib::CameraIntrinsics {
                camera_matrix: [
                    [rmat.fx, 0.0, rmat.cx],
                    [0.0, rmat.fy, rmat.cy],
                    [0.0, 0.0, 1.0],
                ],
                dist_coeffs: [rdist.k1, rdist.k2, rdist.p1, rdist.p2, rdist.k3],
            },
            extrinsics: calib::StereoExtrinsics {
                rotation: [
                    [ext.rotation[0][0], ext.rotation[0][1], ext.rotation[0][2]],
                    [ext.rotation[1][0], ext.rotation[1][1], ext.rotation[1][2]],
                    [ext.rotation[2][0], ext.rotation[2][1], ext.rotation[2][2]],
                ],
                translation: [ext.translation[0], ext.translation[1], ext.translation[2]],
            },
        };

        let rectifier = rect::Rectifier::new(&cal, width as i32, height as i32)?;

        // 映射 rectifier 数据到本 crate 结构
        let rectification = RectificationData {
            r1: rectifier.r1,
            r2: rectifier.r2,
            p1: rectifier.p1,
            p2: rectifier.p2,
            q: rectifier.q,
            roi1: rectifier.roi1,
            roi2: rectifier.roi2,
            map1x: rectifier.map1x,
            map1y: rectifier.map1y,
            map2x: rectifier.map2x,
            map2y: rectifier.map2y,
        };

        self.rectification = Some(rectification);

        log::info!("Stereo rectification initialized successfully");
        log::info!(
            "Left camera ROI: {:?}",
            self.rectification.as_ref().unwrap().roi1
        );
        log::info!(
            "Right camera ROI: {:?}",
            self.rectification.as_ref().unwrap().roi2
        );

        Ok(())
    }

    /// Rectify stereo images
    #[cfg(feature = "opencv")]
    pub fn rectify_images(
        &self,
        left_image: &Mat,
        right_image: &Mat,
    ) -> CorrectionResult<(Mat, Mat)> {
        if self.rectification.is_none() {
            return Err(CorrectionError::RectificationNotInitialized(
                "Stereo rectification not initialized".to_string(),
            ));
        }

        let rectification = self.rectification.as_ref().unwrap();

        // Apply remapping to left image
        let left_rectified = if !left_image.empty() {
            let mut result = Mat::default();
            opencv::imgproc::remap(
                left_image,
                &mut result,
                &rectification.map1x,
                &rectification.map1y,
                crate::DEFAULT_INTERPOLATION,
                opencv::core::BORDER_CONSTANT,
                opencv::core::Scalar::default(),
            )?;
            result
        } else {
            Mat::default()
        };

        // Apply remapping to right image
        let right_rectified = if !right_image.empty() {
            let mut result = Mat::default();
            opencv::imgproc::remap(
                right_image,
                &mut result,
                &rectification.map2x,
                &rectification.map2y,
                crate::DEFAULT_INTERPOLATION,
                opencv::core::BORDER_CONSTANT,
                opencv::core::Scalar::default(),
            )?;
            result
        } else {
            Mat::default()
        };

        // Apply ROI cropping
        let (left_cropped, right_cropped) = self.apply_roi_cropping(
            &left_rectified,
            &right_rectified,
            &rectification.roi1,
            &rectification.roi2,
        )?;

        // Ensure consistent image sizes
        let (left_final, right_final) =
            self.ensure_consistent_sizes(&left_cropped, &right_cropped)?;

        Ok((left_final, right_final))
    }

    /// Get rectification data
    #[cfg(feature = "opencv")]
    pub fn get_rectification(&self) -> Option<&RectificationData> {
        self.rectification.as_ref()
    }

    /// Check if rectification is initialized
    #[cfg(feature = "opencv")]
    pub fn is_rectification_initialized(&self) -> bool {
        self.rectification.is_some()
    }

    /// Get stereo parameters
    pub fn get_stereo_parameters(&self) -> &StereoParameters {
        &self.stereo_params
    }

    /// Apply ROI cropping to rectified images
    #[cfg(feature = "opencv")]
    fn apply_roi_cropping(
        &self,
        left_image: &Mat,
        right_image: &Mat,
        roi1: &opencv::core::Rect,
        roi2: &opencv::core::Rect,
    ) -> CorrectionResult<(Mat, Mat)> {
        let mut left_cropped = Mat::default();
        let mut right_cropped = Mat::default();

        // Crop left image if ROI is valid
        if !left_image.empty() && roi1.width > 0 && roi1.height > 0 {
            if roi1.x >= 0
                && roi1.y >= 0
                && roi1.x + roi1.width <= left_image.cols()
                && roi1.y + roi1.height <= left_image.rows()
            {
                let roi_ref = Mat::roi(left_image, *roi1)?;
                roi_ref.copy_to(&mut left_cropped)?;
            } else {
                log::warn!("Left image ROI invalid, using full image");
                left_cropped = left_image.clone();
            }
        }

        // Crop right image if ROI is valid
        if !right_image.empty() && roi2.width > 0 && roi2.height > 0 {
            if roi2.x >= 0
                && roi2.y >= 0
                && roi2.x + roi2.width <= right_image.cols()
                && roi2.y + roi2.height <= right_image.rows()
            {
                let roi_ref = Mat::roi(right_image, *roi2)?;
                roi_ref.copy_to(&mut right_cropped)?;
            } else {
                log::warn!("Right image ROI invalid, using full image");
                right_cropped = right_image.clone();
            }
        }

        Ok((left_cropped, right_cropped))
    }

    /// Ensure consistent image sizes between left and right images
    #[cfg(feature = "opencv")]
    fn ensure_consistent_sizes(&self, left: &Mat, right: &Mat) -> CorrectionResult<(Mat, Mat)> {
        if left.empty() || right.empty() {
            return Ok((left.clone(), right.clone()));
        }

        let left_size = left.size()?;
        let right_size = right.size()?;

        if left_size == right_size {
            return Ok((left.clone(), right.clone()));
        }

        // Use smaller size to avoid information loss
        let min_width = std::cmp::min(left_size.width, right_size.width);
        let min_height = std::cmp::min(left_size.height, right_size.height);
        let min_size = Size::new(min_width, min_height);

        log::info!("Adjusting image sizes to: {}x{}", min_width, min_height);

        let left_final = if left_size != min_size {
            let roi_ref = Mat::roi(left, opencv::core::Rect::new(0, 0, min_width, min_height))?;
            let mut dst = Mat::default();
            roi_ref.copy_to(&mut dst)?;
            dst
        } else {
            left.clone()
        };

        let right_final = if right_size != min_size {
            let roi_ref = Mat::roi(right, opencv::core::Rect::new(0, 0, min_width, min_height))?;
            let mut dst = Mat::default();
            roi_ref.copy_to(&mut dst)?;
            dst
        } else {
            right.clone()
        };

        Ok((left_final, right_final))
    }
}

// Helper methods for converting extrinsics to OpenCV Mat
use crate::parameters::CameraExtrinsics;

impl CameraExtrinsics {
    /// Convert rotation matrix to OpenCV Mat
    #[cfg(feature = "opencv")]
    pub fn to_opencv_rotation_matrix(&self) -> CorrectionResult<Mat> {
        let mut matrix = Mat::zeros(3, 3, CV_64F)?.to_mat()?;

        for (i, row) in self.rotation.iter().enumerate() {
            for (j, &value) in row.iter().enumerate() {
                *matrix.at_2d_mut::<f64>(i as i32, j as i32)? = value;
            }
        }

        Ok(matrix)
    }

    /// Convert translation vector to OpenCV Mat
    #[cfg(feature = "opencv")]
    pub fn to_opencv_translation_vector(&self) -> CorrectionResult<Mat> {
        let mut matrix = Mat::zeros(3, 1, CV_64F)?.to_mat()?;

        for (i, &value) in self.translation.iter().enumerate() {
            *matrix.at_2d_mut::<f64>(i as i32, 0)? = value;
        }

        Ok(matrix)
    }
}
