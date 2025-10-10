//! Camera Correction Library for SmartScope
//!
//! This library provides camera distortion correction and stereo rectification
//! functionality, compatible with the existing C++ implementation.
//!
//! ## Features
//!
//! - **Parameter Loading**: Load camera parameters from SmartScope format
//! - **Parameter Validation**: Validate camera parameters
//! - **OpenCV Integration**: Distortion correction and stereo rectification
//! - **RGA Integration Ready**: Ready for hardware acceleration
//!
//! ## Usage
//!
//! ```rust,no_run
//! use camera_correction::{CameraParameters, StereoParameters};
//!
//! fn main() -> Result<(), Box<dyn std::error::Error>> {
//!     // Load camera parameters
//!     let params = CameraParameters::from_directory("./camera_parameters")?;
//!
//!     // Access stereo parameters
//!     let stereo = params.get_stereo_parameters()?;
//!     
//!     Ok(())
//! }
//! ```
//!
//! ## Architecture
//!
//! The library follows the same architecture as the C++ `StereoCalibrationHelper`:
//!
//! 1. **Parameter Loading**: Parse camera intrinsic and extrinsic parameters
//! 2. **Rectification Initialization**: Compute stereo rectification parameters
//! 3. **Image Correction**: Apply distortion correction and stereo rectification
//! 4. **ROI Processing**: Handle regions of interest for optimal stereo matching

pub mod distortion;
pub mod error;
pub mod parameters;
pub mod stereo;
pub mod utils;

pub use error::{CorrectionError, CorrectionResult};
pub use parameters::{CameraMatrix, CameraParameters, DistortionCoeffs, StereoParameters};

#[cfg(feature = "opencv")]
pub use distortion::DistortionCorrector;
#[cfg(feature = "opencv")]
pub use stereo::StereoRectifier;

/// Main result type for camera correction operations
pub type Result<T> = std::result::Result<T, CorrectionError>;

/// Default camera parameters directory
pub const DEFAULT_CAMERA_PARAMS_DIR: &str = "./camera_parameters";

/// Default camera parameter files
pub const CAMERA0_INTRINSICS_FILE: &str = "camera0_intrinsics.dat";
pub const CAMERA1_INTRINSICS_FILE: &str = "camera1_intrinsics.dat";
pub const CAMERA1_ROT_TRANS_FILE: &str = "camera1_rot_trans.dat";

/// Default interpolation method for remapping
pub const DEFAULT_INTERPOLATION: i32 = 1; // INTER_LINEAR

/// Default alpha value for stereo rectification (good compromise)
pub const DEFAULT_ALPHA: f64 = -1.0;

/// Default disparity calculation method
pub const DEFAULT_CALIB_FLAGS: i32 = 0; // CALIB_ZERO_DISPARITY

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::TempDir;

    fn create_test_parameters() -> (TempDir, String) {
        let temp_dir = TempDir::new().unwrap();
        let params_dir = temp_dir.path().to_string_lossy().to_string();

        // Create camera0_intrinsics.dat
        let camera0_content = "intrinsic:\n898.3346215154088 0.0 340.2721819853888\n0.0 898.1237909829308 671.8855665969087\n0.0 0.0 1.0\ndistortion:\n-0.3515080378928914 0.1384798471062375 0.00019573510166350308 0.0009257532860999716 -0.03016229279176024\n";
        fs::write(
            format!("{}/camera0_intrinsics.dat", params_dir),
            camera0_content,
        )
        .unwrap();

        // Create camera1_intrinsics.dat
        let camera1_content = "intrinsic:\n912.8658242343989 0.0 331.74463879829017\n0.0 912.3004482994678 618.8318033526601\n0.0 0.0 1.0\ndistortion:\n-0.3583185476313408 0.1652272378216768 -6.687808336748191e-05 -0.002014324977711934 -0.05231529279176024\n";
        fs::write(
            format!("{}/camera1_intrinsics.dat", params_dir),
            camera1_content,
        )
        .unwrap();

        // Create camera1_rot_trans.dat
        let rot_trans_content = "R:\n0.9999719036484747 -0.0053234228280792245 0.005277602015993865\n0.0053554980000618405 0.9999671621096281 -0.006082215351999532\n-0.005245050506627286 0.006110308650980747 0.9999675762610369\nT:\n-2.0530676905836547\n-0.08780168884016623\n0.06289618122342135\n";
        fs::write(
            format!("{}/camera1_rot_trans.dat", params_dir),
            rot_trans_content,
        )
        .unwrap();

        (temp_dir, params_dir)
    }

    #[test]
    fn test_load_parameters() {
        let (_temp_dir, params_dir) = create_test_parameters();

        let params = CameraParameters::from_directory(&params_dir).unwrap();
        let stereo = params.get_stereo_parameters().unwrap();

        // Test left camera
        assert!((stereo.left_intrinsics.matrix.fx - 898.3346215154088).abs() < 1e-6);
        assert!((stereo.left_intrinsics.matrix.fy - 898.1237909829308).abs() < 1e-6);
        assert!((stereo.left_intrinsics.matrix.cx - 340.2721819853888).abs() < 1e-6);
        assert!((stereo.left_intrinsics.matrix.cy - 671.8855665969087).abs() < 1e-6);

        // Test right camera
        assert!((stereo.right_intrinsics.matrix.fx - 912.8658242343989).abs() < 1e-6);
        assert!((stereo.right_intrinsics.matrix.fy - 912.3004482994678).abs() < 1e-6);
        assert!((stereo.right_intrinsics.matrix.cx - 331.74463879829017).abs() < 1e-6);
        assert!((stereo.right_intrinsics.matrix.cy - 618.8318033526601).abs() < 1e-6);

        // Test extrinsics
        assert!((stereo.right_extrinsics.translation[0] - (-2.0530676905836547)).abs() < 1e-6);
        assert!((stereo.right_extrinsics.translation[1] - (-0.08780168884016623)).abs() < 1e-6);
        assert!((stereo.right_extrinsics.translation[2] - (0.06289618122342135)).abs() < 1e-6);
    }

    #[test]
    fn test_parameter_validation() {
        let (_temp_dir, _params_dir) = create_test_parameters();

        let params = CameraParameters::from_directory("../../camera_parameters").unwrap();
        let stereo = params.get_stereo_parameters().unwrap();

        // This should not panic
        stereo.validate().unwrap();
    }
}
