use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::Path;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct CameraIntrinsics {
    pub camera_matrix: [[f64; 3]; 3],
    pub dist_coeffs: [f64; 5],
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct StereoExtrinsics {
    pub rotation: [[f64; 3]; 3],
    pub translation: [f64; 3],
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct StereoCalibration {
    pub left: CameraIntrinsics,
    pub right: CameraIntrinsics,
    pub extrinsics: StereoExtrinsics,
}

#[derive(thiserror::Error, Debug)]
pub enum CalibrationError {
    #[error("io error: {0}")]
    Io(#[from] std::io::Error),
    #[error("parse error: {0}")]
    Parse(String),
    #[error("missing file: {0}")]
    Missing(String),
}

pub fn load_from_dir<P: AsRef<Path>>(dir: P) -> Result<StereoCalibration, CalibrationError> {
    let dir = dir.as_ref();
    let left = dir.join("camera0_intrinsics.dat");
    let right = dir.join("camera1_intrinsics.dat");
    let rt = dir.join("camera1_rot_trans.dat");

    if !left.exists() {
        return Err(CalibrationError::Missing(left.display().to_string()));
    }
    if !right.exists() {
        return Err(CalibrationError::Missing(right.display().to_string()));
    }
    if !rt.exists() {
        return Err(CalibrationError::Missing(rt.display().to_string()));
    }

    let left_intr = parse_intrinsics_file(&left)?;
    let right_intr = parse_intrinsics_file(&right)?;
    let extr = parse_rot_trans_file(&rt)?;

    Ok(StereoCalibration {
        left: left_intr,
        right: right_intr,
        extrinsics: extr,
    })
}

pub fn parse_intrinsics_file(path: &Path) -> Result<CameraIntrinsics, CalibrationError> {
    let f = File::open(path)?;
    let mut rd = BufReader::new(f);

    let mut line = String::new();
    // intrinsic:
    rd.read_line(&mut line)?;
    if !line.to_lowercase().contains("intrinsic:") {
        return Err(CalibrationError::Parse(format!(
            "{}: expect 'intrinsic:' header",
            path.display()
        )));
    }
    let mut m = [[0.0; 3]; 3];
    for i in 0..3 {
        line.clear();
        let n = rd.read_line(&mut line)?;
        if n == 0 {
            return Err(CalibrationError::Parse(format!(
                "{}: unexpected EOF reading camera matrix row {}",
                path.display(),
                i
            )));
        }
        let vals = split_numbers(&line, 3, path)?;
        for j in 0..3 {
            m[i][j] = vals[j];
        }
    }
    // distortion:
    line.clear();
    rd.read_line(&mut line)?;
    if !line.to_lowercase().contains("distortion:") {
        return Err(CalibrationError::Parse(format!(
            "{}: expect 'distortion:' header",
            path.display()
        )));
    }
    line.clear();
    rd.read_line(&mut line)?;
    let vals = split_numbers(&line, 5, path)?;
    let dist = [vals[0], vals[1], vals[2], vals[3], vals[4]];

    Ok(CameraIntrinsics {
        camera_matrix: m,
        dist_coeffs: dist,
    })
}

pub fn parse_rot_trans_file(path: &Path) -> Result<StereoExtrinsics, CalibrationError> {
    let f = File::open(path)?;
    let mut rd = BufReader::new(f);
    let mut line = String::new();

    // R:
    rd.read_line(&mut line)?;
    if !line.to_lowercase().contains("r:") {
        return Err(CalibrationError::Parse(format!(
            "{}: expect 'R:' header",
            path.display()
        )));
    }
    let mut r = [[0.0; 3]; 3];
    for i in 0..3 {
        line.clear();
        let n = rd.read_line(&mut line)?;
        if n == 0 {
            return Err(CalibrationError::Parse(format!(
                "{}: unexpected EOF reading R row {}",
                path.display(),
                i
            )));
        }
        let vals = split_numbers(&line, 3, path)?;
        for j in 0..3 {
            r[i][j] = vals[j];
        }
    }

    // T:
    line.clear();
    rd.read_line(&mut line)?;
    if !line.to_lowercase().contains("t:") {
        return Err(CalibrationError::Parse(format!(
            "{}: expect 'T:' header",
            path.display()
        )));
    }
    let mut t = [0.0; 3];
    for i in 0..3 {
        line.clear();
        let n = rd.read_line(&mut line)?;
        if n == 0 {
            return Err(CalibrationError::Parse(format!(
                "{}: unexpected EOF reading T row {}",
                path.display(),
                i
            )));
        }
        let vals = split_numbers(&line, 1, path)?;
        t[i] = vals[0];
    }

    Ok(StereoExtrinsics {
        rotation: r,
        translation: t,
    })
}

fn split_numbers(line: &str, expect: usize, path: &Path) -> Result<Vec<f64>, CalibrationError> {
    let parts: Vec<&str> = line
        .split(|c: char| c.is_whitespace() || c == ',')
        .filter(|s| !s.is_empty())
        .collect();
    if parts.len() != expect {
        return Err(CalibrationError::Parse(format!(
            "{}: expected {} numbers, got {} in '{}'",
            path.display(),
            expect,
            parts.len(),
            line.trim()
        )));
    }
    let mut out = Vec::with_capacity(expect);
    for p in parts {
        let v: f64 = p.parse().map_err(|_| {
            CalibrationError::Parse(format!("{}: invalid number '{}'", path.display(), p))
        })?;
        out.push(v);
    }
    Ok(out)
}
