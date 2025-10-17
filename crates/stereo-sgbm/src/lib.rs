//! Stereo SGBM disparity computation (Rust + OpenCV)
//!
//! This crate implements SGBM-based disparity computation similar to
//! reference_code/SmartScope's C++ implementation, using the OpenCV Rust bindings.

#[cfg(feature = "opencv")]
use opencv::{
    calib3d,
    core::{self, Mat, CV_32F},
    imgproc,
    prelude::*,
    Result as CvResult,
};

#[cfg(feature = "opencv")]
#[derive(Clone, Debug)]
pub struct StereoSgbmParams {
    pub min_disparity: i32,
    pub num_disparities: i32, // must be divisible by 16
    pub block_size: i32,      // odd number
    pub uniqueness_ratio: i32,
    pub speckle_window_size: i32,
    pub speckle_range: i32,
    pub prefilter_cap: i32,
    pub disp12_max_diff: i32,
    /// Mode value, e.g. calib3d::StereoSGBM_MODE_SGBM_3WAY
    pub mode: i32,
}

#[cfg(feature = "opencv")]
impl Default for StereoSgbmParams {
    fn default() -> Self {
        Self {
            min_disparity: 0,
            num_disparities: 16 * 8,
            block_size: 5,
            uniqueness_ratio: 10,
            speckle_window_size: 100,
            speckle_range: 32,
            prefilter_cap: 63,
            disp12_max_diff: 1,
            mode: calib3d::StereoSGBM_MODE_SGBM_3WAY,
        }
    }
}

/// Stereo SGBM wrapper
#[cfg(feature = "opencv")]
pub struct StereoSgbm {
    params: StereoSgbmParams,
    sgbm: opencv::core::Ptr<opencv::calib3d::StereoSGBM>,
}

#[cfg(feature = "opencv")]
impl StereoSgbm {
    /// Create a new SGBM matcher from parameters.
    pub fn new(params: StereoSgbmParams) -> CvResult<Self> {
        // We convert to grayscale before matching -> cn = 1
        let cn = 1;
        let p1 = 8 * cn * params.block_size * params.block_size;
        let p2 = 32 * cn * params.block_size * params.block_size;
        let sgbm = calib3d::StereoSGBM::create(
            params.min_disparity,
            params.num_disparities,
            params.block_size,
            p1,
            p2,
            params.disp12_max_diff,
            params.prefilter_cap,
            params.uniqueness_ratio,
            params.speckle_window_size,
            params.speckle_range,
            params.mode,
        )?;
        Ok(Self { params, sgbm })
    }

    /// Create with default parameters matching the reference implementation.
    pub fn with_defaults() -> CvResult<Self> {
        Self::new(StereoSgbmParams::default())
    }

    /// Compute disparity (CV_32F, scaled by 1/16) from rectified BGR/GRAY stereo images.
    /// - Accepts `CV_8UC3` (BGR) or `CV_8UC1` input.
    pub fn compute_disparity(&mut self, left: &Mat, right: &Mat) -> CvResult<Mat> {
        Self::validate_inputs(left, right)?;

        let (gray_l, gray_r) = Self::to_grayscale_pair(left, right)?;

        // Run SGBM
        let mut disp16s = Mat::default();
        self.sgbm.compute(&gray_l, &gray_r, &mut disp16s)?;

        // Convert to CV_32F with scale 1/16.0, as in reference
        let mut disp32f = Mat::default();
        disp16s.convert_to(&mut disp32f, CV_32F, 1.0 / 16.0, 0.0)?;
        Ok(disp32f)
    }

    /// Compute depth Z (in the same metric as Q) from disparity using the reprojection matrix `Q`.
    /// Returns a single-channel `CV_32F` depth map (Z channel of reprojected 3D points).
    pub fn compute_depth_from_disparity(&self, disparity_32f: &Mat, q: &Mat) -> CvResult<Mat> {
        if disparity_32f.empty() {
            return Ok(Mat::default());
        }
        // Reproject to 3D and extract Z
        let mut points3d = Mat::default();
        calib3d::reproject_image_to_3d(
            disparity_32f,
            &mut points3d,
            q,
            true,
            CV_32F,
        )?;
        // Split and return Z channel
        let mut channels: core::Vector<Mat> = core::Vector::new();
        core::split(&points3d, &mut channels)?;
        if channels.len() < 3 {
            return Ok(Mat::default());
        }
        Ok(channels.get(2)?)
    }

    fn validate_inputs(left: &Mat, right: &Mat) -> CvResult<()> {
        if left.empty() || right.empty() {
            return Err(opencv::Error::new(0, "Empty input images"));
        }
        let ls = left.size()?;
        let rs = right.size()?;
        if ls != rs {
            return Err(opencv::Error::new(0, "Input sizes mismatch"));
        }
        Ok(())
    }

    fn to_grayscale_pair(left: &Mat, right: &Mat) -> CvResult<(Mat, Mat)> {
        let mut gray_l = Mat::default();
        let mut gray_r = Mat::default();

        let l_ch = left.channels();
        let r_ch = right.channels();

        match (l_ch, r_ch) {
            (3, 3) => {
                imgproc::cvt_color(left, &mut gray_l, imgproc::COLOR_BGR2GRAY, 0)?;
                imgproc::cvt_color(right, &mut gray_r, imgproc::COLOR_BGR2GRAY, 0)?;
            }
            (1, 1) => {
                gray_l = left.clone();
                gray_r = right.clone();
            }
            (3, 1) => {
                imgproc::cvt_color(left, &mut gray_l, imgproc::COLOR_BGR2GRAY, 0)?;
                gray_r = right.clone();
            }
            (1, 3) => {
                gray_l = left.clone();
                imgproc::cvt_color(right, &mut gray_r, imgproc::COLOR_BGR2GRAY, 0)?;
            }
            _ => {
                // Fallback: try to convert both
                imgproc::cvt_color(left, &mut gray_l, imgproc::COLOR_BGR2GRAY, 0)?;
                imgproc::cvt_color(right, &mut gray_r, imgproc::COLOR_BGR2GRAY, 0)?;
            }
        }

        Ok((gray_l, gray_r))
    }
}

// Non-OpenCV builds: provide stubs to keep the crate importable
#[cfg(not(feature = "opencv"))]
pub mod no_opencv {
    /// Build with `--features opencv` to enable functionality.
    pub const FEATURE_HINT: &str = "stereo-sgbm compiled without OpenCV; enable the `opencv` feature.";
}
