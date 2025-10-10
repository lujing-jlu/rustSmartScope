use opencv::{
    calib3d,
    core::{self, Mat, MatTraitConst, Rect, Size},
    imgproc,
    prelude::*,
};
use stereo_calibration_reader as calib;

#[derive(thiserror::Error, Debug)]
pub enum RectifyError {
    #[error("opencv error: {0}")]
    OpenCV(#[from] opencv::Error),
    #[error("invalid argument: {0}")]
    Invalid(String),
}

pub struct Rectifier {
    pub image_size: Size,
    pub r1: Mat,
    pub r2: Mat,
    pub p1: Mat,
    pub p2: Mat,
    pub q: Mat,
    pub map1x: Mat,
    pub map1y: Mat,
    pub map2x: Mat,
    pub map2y: Mat,
    pub roi1: Rect,
    pub roi2: Rect,
}

impl Rectifier {
    pub fn new(
        cal: &calib::StereoCalibration,
        width: i32,
        height: i32,
    ) -> Result<Self, RectifyError> {
        if width <= 0 || height <= 0 {
            return Err(RectifyError::Invalid(format!(
                "invalid image size: {}x{}",
                width, height
            )));
        }
        let size = Size::new(width, height);

        let k1 = mat_from_3x3(&cal.left.camera_matrix)?;
        let k2 = mat_from_3x3(&cal.right.camera_matrix)?;
        let d1 = mat_from_row(&cal.left.dist_coeffs)?;
        let d2 = mat_from_row(&cal.right.dist_coeffs)?;
        let r = mat_from_3x3(&cal.extrinsics.rotation)?;
        let t = mat_from_col3(&cal.extrinsics.translation)?;

        let mut r1 = Mat::default();
        let mut r2 = Mat::default();
        let mut p1 = Mat::default();
        let mut p2 = Mat::default();
        let mut q = Mat::default();
        let mut roi1 = Rect::default();
        let mut roi2 = Rect::default();

        calib3d::stereo_rectify(
            &k1,
            &d1,
            &k2,
            &d2,
            size,
            &r,
            &t,
            &mut r1,
            &mut r2,
            &mut p1,
            &mut p2,
            &mut q,
            calib3d::CALIB_ZERO_DISPARITY,
            -1.0,
            size,
            &mut roi1,
            &mut roi2,
        )?;

        let mut map1x = Mat::default();
        let mut map1y = Mat::default();
        let mut map2x = Mat::default();
        let mut map2y = Mat::default();

        calib3d::init_undistort_rectify_map(
            &k1,
            &d1,
            &r1,
            &p1,
            size,
            core::CV_32FC1,
            &mut map1x,
            &mut map1y,
        )?;
        calib3d::init_undistort_rectify_map(
            &k2,
            &d2,
            &r2,
            &p2,
            size,
            core::CV_32FC1,
            &mut map2x,
            &mut map2y,
        )?;

        Ok(Self {
            image_size: size,
            r1,
            r2,
            p1,
            p2,
            q,
            map1x,
            map1y,
            map2x,
            map2y,
            roi1,
            roi2,
        })
    }

    pub fn rectify_left(&self, src: &Mat) -> Result<Mat, RectifyError> {
        ensure_size(src, self.image_size)?;
        let mut dst = Mat::default();
        imgproc::remap(
            src,
            &mut dst,
            &self.map1x,
            &self.map1y,
            imgproc::INTER_LINEAR,
            core::BORDER_CONSTANT,
            core::Scalar::all(0.0),
        )?;
        Ok(dst)
    }

    pub fn rectify_right(&self, src: &Mat) -> Result<Mat, RectifyError> {
        ensure_size(src, self.image_size)?;
        let mut dst = Mat::default();
        imgproc::remap(
            src,
            &mut dst,
            &self.map2x,
            &self.map2y,
            imgproc::INTER_LINEAR,
            core::BORDER_CONSTANT,
            core::Scalar::all(0.0),
        )?;
        Ok(dst)
    }

    pub fn rectify_pair(&self, left: &Mat, right: &Mat) -> Result<(Mat, Mat), RectifyError> {
        Ok((self.rectify_left(left)?, self.rectify_right(right)?))
    }
}

fn ensure_size(mat: &Mat, size: Size) -> Result<(), RectifyError> {
    let s = mat.size()?;
    if s.width != size.width || s.height != size.height {
        return Err(RectifyError::Invalid(format!(
            "image size mismatch: got {}x{}, expect {}x{}",
            s.width, s.height, size.width, size.height
        )));
    }
    Ok(())
}

fn mat_from_3x3(m: &[[f64; 3]; 3]) -> opencv::Result<Mat> {
    Mat::from_slice_2d(m)
}

fn mat_from_row(v: &[f64; 5]) -> opencv::Result<Mat> {
    let mut m = Mat::new_rows_cols_with_default(1, 5, core::CV_64F, core::Scalar::all(0.0))?;
    for (j, &val) in v.iter().enumerate() {
        *m.at_2d_mut::<f64>(0, j as i32)? = val;
    }
    Ok(m)
}

fn mat_from_col3(v: &[f64; 3]) -> opencv::Result<Mat> {
    let mut m = Mat::new_rows_cols_with_default(3, 1, core::CV_64F, core::Scalar::all(0.0))?;
    for (i, &val) in v.iter().enumerate() {
        *m.at_2d_mut::<f64>(i as i32, 0)? = val;
    }
    Ok(m)
}
