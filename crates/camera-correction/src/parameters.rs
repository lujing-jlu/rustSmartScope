//! Camera parameter management and validation

use crate::error::{CorrectionError, CorrectionResult};
use std::path::Path;
use stereo_calibration_reader as calib;

/// Camera intrinsic matrix (3x3)
#[derive(Debug, Clone)]
pub struct CameraMatrix {
    /// Focal length in x direction
    pub fx: f64,
    /// Focal length in y direction
    pub fy: f64,
    /// Principal point x coordinate
    pub cx: f64,
    /// Principal point y coordinate
    pub cy: f64,
}

impl CameraMatrix {
    /// Create a new camera matrix
    pub fn new(fx: f64, fy: f64, cx: f64, cy: f64) -> Self {
        Self { fx, fy, cx, cy }
    }

    /// Convert to 3x3 matrix as Vec<Vec<f64>>
    pub fn to_matrix(&self) -> Vec<Vec<f64>> {
        vec![
            vec![self.fx, 0.0, self.cx],
            vec![0.0, self.fy, self.cy],
            vec![0.0, 0.0, 1.0],
        ]
    }

    /// Validate matrix parameters
    pub fn validate(&self) -> CorrectionResult<()> {
        if self.fx <= 0.0 || self.fy <= 0.0 {
            return Err(CorrectionError::ParameterParseError(
                "Focal lengths must be positive".to_string(),
            ));
        }
        if self.cx < 0.0 || self.cy < 0.0 {
            return Err(CorrectionError::ParameterParseError(
                "Principal point coordinates must be non-negative".to_string(),
            ));
        }
        Ok(())
    }
}

/// Camera distortion coefficients
#[derive(Debug, Clone)]
pub struct DistortionCoeffs {
    /// Radial distortion coefficient k1
    pub k1: f64,
    /// Radial distortion coefficient k2
    pub k2: f64,
    /// Tangential distortion coefficient p1
    pub p1: f64,
    /// Tangential distortion coefficient p2
    pub p2: f64,
    /// Radial distortion coefficient k3
    pub k3: f64,
}

impl DistortionCoeffs {
    /// Create new distortion coefficients
    pub fn new(k1: f64, k2: f64, p1: f64, p2: f64, k3: f64) -> Self {
        Self { k1, k2, p1, p2, k3 }
    }

    /// Convert to vector
    pub fn to_vector(&self) -> Vec<f64> {
        vec![self.k1, self.k2, self.p1, self.p2, self.k3]
    }
}

/// Camera intrinsics (matrix + distortion)
#[derive(Debug, Clone)]
pub struct CameraIntrinsics {
    /// Camera matrix
    pub matrix: CameraMatrix,
    /// Distortion coefficients
    pub distortion: DistortionCoeffs,
}

impl CameraIntrinsics {
    /// Create new camera intrinsics
    pub fn new(matrix: CameraMatrix, distortion: DistortionCoeffs) -> Self {
        Self { matrix, distortion }
    }

    /// Validate intrinsics
    pub fn validate(&self) -> CorrectionResult<()> {
        self.matrix.validate()?;
        Ok(())
    }
}

/// Camera extrinsics (rotation + translation)
#[derive(Debug, Clone)]
pub struct CameraExtrinsics {
    /// Rotation matrix (3x3)
    pub rotation: Vec<Vec<f64>>,
    /// Translation vector (3x1)
    pub translation: Vec<f64>,
}

impl CameraExtrinsics {
    /// Create new camera extrinsics
    pub fn new(rotation: Vec<Vec<f64>>, translation: Vec<f64>) -> Self {
        Self {
            rotation,
            translation,
        }
    }

    /// Validate extrinsics
    pub fn validate(&self) -> CorrectionResult<()> {
        if self.rotation.len() != 3 {
            return Err(CorrectionError::ParameterParseError(
                "Rotation matrix must have 3 rows".to_string(),
            ));
        }
        for row in &self.rotation {
            if row.len() != 3 {
                return Err(CorrectionError::ParameterParseError(
                    "Rotation matrix must have 3 columns".to_string(),
                ));
            }
        }
        if self.translation.len() != 3 {
            return Err(CorrectionError::ParameterParseError(
                "Translation vector must have 3 elements".to_string(),
            ));
        }
        Ok(())
    }
}

/// Stereo camera parameters
#[derive(Debug, Clone)]
pub struct StereoParameters {
    /// Left camera intrinsics
    pub left_intrinsics: CameraIntrinsics,
    /// Right camera intrinsics
    pub right_intrinsics: CameraIntrinsics,
    /// Right camera extrinsics (relative to left camera)
    pub right_extrinsics: CameraExtrinsics,
}

impl StereoParameters {
    /// Create new stereo parameters
    pub fn new(
        left_intrinsics: CameraIntrinsics,
        right_intrinsics: CameraIntrinsics,
        right_extrinsics: CameraExtrinsics,
    ) -> Self {
        Self {
            left_intrinsics,
            right_intrinsics,
            right_extrinsics,
        }
    }

    /// Validate stereo parameters
    pub fn validate(&self) -> CorrectionResult<()> {
        self.left_intrinsics.validate()?;
        self.right_intrinsics.validate()?;
        self.right_extrinsics.validate()?;
        Ok(())
    }
}

/// Complete camera parameters
#[derive(Debug, Clone)]
pub struct CameraParameters {
    /// Stereo parameters
    pub stereo: StereoParameters,
}

impl CameraParameters {
    /// Load camera parameters from directory
    pub fn from_directory<P: AsRef<Path>>(base_path: P) -> CorrectionResult<Self> {
        let base_path = base_path.as_ref();
        log::info!(
            "Loading camera parameters via stereo-calibration-reader from: {}",
            base_path.display()
        );

        let calib = calib::load_from_dir(base_path).map_err(|e| {
            CorrectionError::ParameterParseError(format!("Failed to load calibration: {}", e))
        })?;

        let left_intrinsics = Self::from_calib_intrinsics(&calib.left);
        let right_intrinsics = Self::from_calib_intrinsics(&calib.right);
        let right_extrinsics = Self::from_calib_extrinsics(&calib.extrinsics);

        let stereo = StereoParameters::new(left_intrinsics, right_intrinsics, right_extrinsics);
        stereo.validate()?;

        log::info!("Camera parameters loaded successfully");
        Ok(Self { stereo })
    }

    /// Get stereo parameters
    pub fn get_stereo_parameters(&self) -> CorrectionResult<&StereoParameters> {
        Ok(&self.stereo)
    }

    /// Parse intrinsics file
    #[allow(dead_code)]
    fn parse_intrinsics_file<P: AsRef<Path>>(file_path: P) -> CorrectionResult<CameraIntrinsics> {
        let content = std::fs::read_to_string(file_path)?;
        let lines: Vec<&str> = content.lines().collect();

        if lines.len() < 6 {
            return Err(CorrectionError::ParameterParseError(
                "Intrinsics file has insufficient lines".to_string(),
            ));
        }

        // Parse camera matrix
        let mut matrix_values = Vec::new();
        for line in &lines[1..4] {
            let values: Vec<f64> = line
                .split_whitespace()
                .filter_map(|s| s.parse::<f64>().ok())
                .collect();

            if values.len() != 3 {
                return Err(CorrectionError::ParameterParseError(format!(
                    "Invalid camera matrix line: {}",
                    line
                )));
            }
            matrix_values.extend(values);
        }

        let camera_matrix = CameraMatrix::new(
            matrix_values[0], // fx
            matrix_values[4], // fy
            matrix_values[2], // cx
            matrix_values[5], // cy
        );

        // Parse distortion coefficients
        let distortion_line = lines[5];
        let values: Vec<f64> = distortion_line
            .split_whitespace()
            .filter_map(|s| s.parse::<f64>().ok())
            .collect();

        if values.len() != 5 {
            return Err(CorrectionError::ParameterParseError(format!(
                "Invalid distortion coefficients: expected 5, got {}",
                values.len()
            )));
        }

        let distortion = DistortionCoeffs::new(
            values[0], // k1
            values[1], // k2
            values[2], // p1
            values[3], // p2
            values[4], // k3
        );

        Ok(CameraIntrinsics::new(camera_matrix, distortion))
    }

    /// Parse rotation/translation file
    #[allow(dead_code)]
    fn parse_rot_trans_file<P: AsRef<Path>>(file_path: P) -> CorrectionResult<CameraExtrinsics> {
        let content = std::fs::read_to_string(file_path)?;
        let lines: Vec<&str> = content.lines().collect();

        if lines.len() < 8 {
            return Err(CorrectionError::ParameterParseError(
                "Rotation/translation file has insufficient lines".to_string(),
            ));
        }

        // Parse rotation matrix
        let mut rotation = Vec::new();
        for line in &lines[1..4] {
            let values: Vec<f64> = line
                .split_whitespace()
                .filter_map(|s| s.parse::<f64>().ok())
                .collect();

            if values.len() != 3 {
                return Err(CorrectionError::ParameterParseError(format!(
                    "Invalid rotation matrix line: {}",
                    line
                )));
            }
            rotation.push(values);
        }

        // Parse translation vector
        let mut translation = Vec::new();
        for line in &lines[5..8] {
            let value: f64 = line.trim().parse().map_err(|_| {
                CorrectionError::ParameterParseError(format!("Invalid translation value: {}", line))
            })?;
            translation.push(value);
        }

        Ok(CameraExtrinsics::new(rotation, translation))
    }

    /// Build CameraIntrinsics from stereo-calibration-reader type
    fn from_calib_intrinsics(ci: &calib::CameraIntrinsics) -> CameraIntrinsics {
        let m = &ci.camera_matrix;
        let matrix = CameraMatrix::new(m[0][0], m[1][1], m[0][2], m[1][2]);
        let d = &ci.dist_coeffs;
        let distortion = DistortionCoeffs::new(d[0], d[1], d[2], d[3], d[4]);
        CameraIntrinsics::new(matrix, distortion)
    }

    /// Build CameraExtrinsics from stereo-calibration-reader type
    fn from_calib_extrinsics(ex: &calib::StereoExtrinsics) -> CameraExtrinsics {
        let rotation = vec![
            vec![ex.rotation[0][0], ex.rotation[0][1], ex.rotation[0][2]],
            vec![ex.rotation[1][0], ex.rotation[1][1], ex.rotation[1][2]],
            vec![ex.rotation[2][0], ex.rotation[2][1], ex.rotation[2][2]],
        ];
        let translation = vec![ex.translation[0], ex.translation[1], ex.translation[2]];
        CameraExtrinsics::new(rotation, translation)
    }
}
