//! Distortion correction for single cameras

use crate::error::{CorrectionError, CorrectionResult};
use crate::parameters::{CameraIntrinsics, CameraMatrix, DistortionCoeffs};

#[cfg(feature = "opencv")]
use opencv::{
    calib3d,
    core::{Mat, Size, CV_32FC1, CV_64F},
    prelude::*,
};

/// Distortion corrector for single camera
pub struct DistortionCorrector {
    /// Camera intrinsics
    intrinsics: CameraIntrinsics,
    /// Remapping maps (if initialized)
    #[cfg(feature = "opencv")]
    map_x: Option<Mat>,
    #[cfg(feature = "opencv")]
    map_y: Option<Mat>,
    /// Image size for which maps were generated
    #[cfg(feature = "opencv")]
    map_size: Option<Size>,
}

impl DistortionCorrector {
    /// Create a new distortion corrector from camera intrinsics
    pub fn new(intrinsics: CameraIntrinsics) -> Self {
        Self {
            intrinsics,
            #[cfg(feature = "opencv")]
            map_x: None,
            #[cfg(feature = "opencv")]
            map_y: None,
            #[cfg(feature = "opencv")]
            map_size: None,
        }
    }

    /// Create a new distortion corrector from individual components
    pub fn from_components(matrix: CameraMatrix, distortion: DistortionCoeffs) -> Self {
        let intrinsics = CameraIntrinsics::new(matrix, distortion);
        Self::new(intrinsics)
    }

    /// Initialize remapping maps for specific image size
    #[cfg(feature = "opencv")]
    pub fn initialize_maps(&mut self, width: u32, height: u32) -> CorrectionResult<()> {
        let size = Size::new(width as i32, height as i32);
        if size.width <= 0 || size.height <= 0 {
            return Err(CorrectionError::InvalidImageSize {
                width: size.width,
                height: size.height,
            });
        }

        log::info!(
            "Initializing distortion correction maps for size: {}x{}",
            width,
            height
        );

        // Convert our data structures to OpenCV Mat
        let camera_matrix = self.intrinsics.matrix.to_opencv_matrix()?;
        let dist_coeffs = self.intrinsics.distortion.to_opencv_matrix()?;

        // Generate remapping maps (OpenCV expects output parameters)
        let mut map_x = Mat::default();
        let mut map_y = Mat::default();
        calib3d::init_undistort_rectify_map(
            &camera_matrix,
            &dist_coeffs,
            &Mat::eye(3, 3, CV_64F)?.to_mat()?, // Identity rotation matrix
            &camera_matrix,                     // Same projection matrix
            size,
            CV_32FC1,
            &mut map_x,
            &mut map_y,
        )?;

        self.map_x = Some(map_x);
        self.map_y = Some(map_y);
        self.map_size = Some(size);

        log::info!("Distortion correction maps initialized successfully");
        Ok(())
    }

    /// Correct distortion in an image
    #[cfg(feature = "opencv")]
    pub fn correct_distortion(&self, image: &Mat) -> CorrectionResult<Mat> {
        if self.map_x.is_none() || self.map_y.is_none() {
            return Err(CorrectionError::RemapMapsNotGenerated(
                "Distortion correction maps not initialized".to_string(),
            ));
        }

        let map_x = self.map_x.as_ref().unwrap();
        let map_y = self.map_y.as_ref().unwrap();

        // Check if image size matches map size
        let image_size = image.size()?;
        let map_size = self.map_size.as_ref().unwrap();

        if image_size != *map_size {
            return Err(CorrectionError::RemapMapsNotGenerated(format!(
                "Image size {}x{} doesn't match map size {}x{}",
                image_size.width, image_size.height, map_size.width, map_size.height
            )));
        }

        let mut corrected = Mat::default();
        opencv::imgproc::remap(
            image,
            &mut corrected,
            map_x,
            map_y,
            crate::DEFAULT_INTERPOLATION,
            opencv::core::BORDER_CONSTANT,
            opencv::core::Scalar::default(),
        )?;

        Ok(corrected)
    }

    /// Get camera intrinsics
    pub fn get_intrinsics(&self) -> &CameraIntrinsics {
        &self.intrinsics
    }

    /// Check if maps are initialized
    #[cfg(feature = "opencv")]
    pub fn are_maps_initialized(&self) -> bool {
        self.map_x.is_some() && self.map_y.is_some()
    }

    /// Get map size
    #[cfg(feature = "opencv")]
    pub fn get_map_size(&self) -> Option<Size> {
        self.map_size
    }
}

// Helper methods for converting our data structures to OpenCV Mat
impl CameraMatrix {
    /// Convert to OpenCV Mat
    #[cfg(feature = "opencv")]
    pub fn to_opencv_matrix(&self) -> CorrectionResult<Mat> {
        let mut matrix = Mat::zeros(3, 3, CV_64F)?.to_mat()?;

        *matrix.at_2d_mut::<f64>(0, 0)? = self.fx;
        *matrix.at_2d_mut::<f64>(0, 1)? = 0.0;
        *matrix.at_2d_mut::<f64>(0, 2)? = self.cx;
        *matrix.at_2d_mut::<f64>(1, 0)? = 0.0;
        *matrix.at_2d_mut::<f64>(1, 1)? = self.fy;
        *matrix.at_2d_mut::<f64>(1, 2)? = self.cy;
        *matrix.at_2d_mut::<f64>(2, 0)? = 0.0;
        *matrix.at_2d_mut::<f64>(2, 1)? = 0.0;
        *matrix.at_2d_mut::<f64>(2, 2)? = 1.0;

        Ok(matrix)
    }
}

impl DistortionCoeffs {
    /// Convert to OpenCV Mat
    #[cfg(feature = "opencv")]
    pub fn to_opencv_matrix(&self) -> CorrectionResult<Mat> {
        let mut matrix = Mat::zeros(5, 1, CV_64F)?.to_mat()?;

        *matrix.at_2d_mut::<f64>(0, 0)? = self.k1;
        *matrix.at_2d_mut::<f64>(1, 0)? = self.k2;
        *matrix.at_2d_mut::<f64>(2, 0)? = self.p1;
        *matrix.at_2d_mut::<f64>(3, 0)? = self.p2;
        *matrix.at_2d_mut::<f64>(4, 0)? = self.k3;

        Ok(matrix)
    }
}
