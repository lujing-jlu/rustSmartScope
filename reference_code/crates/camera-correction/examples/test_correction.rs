use camera_correction::{
    CameraMatrix, CameraParameters, CorrectionError, CorrectionResult, DistortionCoeffs,
};
use std::fs;
use std::path::Path;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("测试 camera-correction 基本功能...");

    // 创建测试目录和文件
    setup_test_data()?;

    // 测试 CameraMatrix
    println!("✅ 测试 CameraMatrix...");
    test_camera_matrix()?;

    // 测试 DistortionCoeffs
    println!("✅ 测试 DistortionCoeffs...");
    test_distortion_coeffs()?;

    // 测试 CameraParameters
    println!("✅ 测试 CameraParameters...");
    test_camera_parameters()?;

    // 测试 StereoParameters
    println!("✅ 测试 StereoParameters...");
    test_stereo_parameters()?;

    // 清理测试数据
    cleanup_test_data()?;

    println!("\n🎉 camera-correction 测试完成！");
    println!("📊 测试结果:");
    println!("  - CameraMatrix: ✅");
    println!("  - DistortionCoeffs: ✅");
    println!("  - CameraParameters: ✅");
    println!("  - StereoParameters: ✅");
    println!("  - 错误处理: ✅");

    Ok(())
}

fn test_camera_matrix() -> CorrectionResult<()> {
    // 测试正常创建
    let matrix = CameraMatrix::new(800.0, 800.0, 320.0, 240.0);
    matrix.validate()?;

    let mat = matrix.to_matrix();
    assert_eq!(mat[0][0], 800.0);
    assert_eq!(mat[1][1], 800.0);
    assert_eq!(mat[0][2], 320.0);
    assert_eq!(mat[1][2], 240.0);
    assert_eq!(mat[2][2], 1.0);

    println!("  - 正常创建和转换: ✅");

    // 测试错误处理
    let invalid_matrix = CameraMatrix::new(-800.0, 800.0, 320.0, 240.0);
    match invalid_matrix.validate() {
        Err(CorrectionError::ParameterParseError(_)) => {
            println!("  - 负焦距错误处理: ✅");
        }
        _ => {
            return Err(CorrectionError::ParameterParseError(
                "Expected error for negative focal length".to_string(),
            ))
        }
    }

    let invalid_matrix2 = CameraMatrix::new(800.0, 800.0, -320.0, 240.0);
    match invalid_matrix2.validate() {
        Err(CorrectionError::ParameterParseError(_)) => {
            println!("  - 负主点错误处理: ✅");
        }
        _ => {
            return Err(CorrectionError::ParameterParseError(
                "Expected error for negative principal point".to_string(),
            ))
        }
    }

    Ok(())
}

fn test_distortion_coeffs() -> CorrectionResult<()> {
    // 测试正常创建
    let coeffs = DistortionCoeffs::new(-0.1, 0.05, 0.001, 0.002, -0.01);

    let vec = coeffs.to_vector();
    assert_eq!(vec.len(), 5);
    assert_eq!(vec[0], -0.1);
    assert_eq!(vec[4], -0.01);

    println!("  - 正常创建和转换: ✅");

    // 测试极端值（这里只是创建，实际验证在 CameraIntrinsics 层面）
    let _extreme_coeffs = DistortionCoeffs::new(10.0, -10.0, 1.0, -1.0, 5.0);
    println!("  - 极端值创建: ✅");

    Ok(())
}

fn test_camera_parameters() -> CorrectionResult<()> {
    // 测试从目录加载
    let params = CameraParameters::from_directory("test_camera_data")?;

    // 验证参数（通过 stereo 字段）
    params.stereo.validate()?;

    println!("  - 从目录加载: ✅");
    println!("  - 参数验证: ✅");

    Ok(())
}

fn test_stereo_parameters() -> CorrectionResult<()> {
    // 测试从目录加载
    let params = CameraParameters::from_directory("test_camera_data")?;
    let stereo = params.get_stereo_parameters()?;

    // 验证立体参数
    stereo.validate()?;

    println!("  - 立体参数创建: ✅");
    println!("  - 立体参数验证: ✅");

    Ok(())
}

fn setup_test_data() -> Result<(), Box<dyn std::error::Error>> {
    fs::create_dir_all("test_camera_data")?;

    // 创建 camera0_intrinsics.dat
    fs::write("test_camera_data/camera0_intrinsics.dat", 
        "intrinsic:\n800.0 0.0 320.0\n0.0 800.0 240.0\n0.0 0.0 1.0\ndistortion:\n-0.1 0.05 0.001 0.002 -0.01")?;

    // 创建 camera1_intrinsics.dat
    fs::write("test_camera_data/camera1_intrinsics.dat", 
        "intrinsic:\n810.0 0.0 325.0\n0.0 810.0 245.0\n0.0 0.0 1.0\ndistortion:\n-0.12 0.06 0.0015 0.0025 -0.012")?;

    // 创建 camera1_rot_trans.dat
    fs::write(
        "test_camera_data/camera1_rot_trans.dat",
        "R:\n0.999 -0.001 0.002\n0.001 0.999 -0.003\n-0.002 0.003 0.999\nT:\n-120.5\n0.8\n-1.2",
    )?;

    Ok(())
}

fn cleanup_test_data() -> Result<(), Box<dyn std::error::Error>> {
    if Path::new("test_camera_data").exists() {
        fs::remove_dir_all("test_camera_data")?;
    }
    Ok(())
}
