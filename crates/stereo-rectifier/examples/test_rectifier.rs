use opencv::{
    core::{Mat, CV_8UC3},
    prelude::*,
};
use stereo_calibration_reader as calib;
use stereo_rectifier::{Rectifier, RectifyError};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("测试 stereo-rectifier 基本功能...");

    // 创建测试标定数据
    let test_calibration = create_test_calibration();

    // 测试 Rectifier 创建
    println!("✅ 创建 Rectifier...");
    let rectifier = Rectifier::new(&test_calibration, 640, 480)?;
    println!(
        "✅ Rectifier 创建成功，图像尺寸: {}x{}",
        rectifier.image_size.width, rectifier.image_size.height
    );

    // 验证 ROI
    println!(
        "左相机 ROI: x={}, y={}, w={}, h={}",
        rectifier.roi1.x, rectifier.roi1.y, rectifier.roi1.width, rectifier.roi1.height
    );
    println!(
        "右相机 ROI: x={}, y={}, w={}, h={}",
        rectifier.roi2.x, rectifier.roi2.y, rectifier.roi2.width, rectifier.roi2.height
    );

    // 创建测试图像
    println!("✅ 创建测试图像...");
    let test_image = create_test_image(640, 480)?;
    println!("测试图像尺寸: {}x{}", test_image.cols(), test_image.rows());

    // 测试左相机校正
    println!("✅ 测试左相机校正...");
    let rectified_left = rectifier.rectify_left(&test_image)?;
    println!(
        "左相机校正后图像尺寸: {}x{}",
        rectified_left.cols(),
        rectified_left.rows()
    );

    // 测试右相机校正
    println!("✅ 测试右相机校正...");
    let rectified_right = rectifier.rectify_right(&test_image)?;
    println!(
        "右相机校正后图像尺寸: {}x{}",
        rectified_right.cols(),
        rectified_right.rows()
    );

    // 测试双相机同时校正
    println!("✅ 测试双相机同时校正...");
    let (left_rect, right_rect) = rectifier.rectify_pair(&test_image, &test_image)?;
    println!(
        "双相机校正完成，左: {}x{}, 右: {}x{}",
        left_rect.cols(),
        left_rect.rows(),
        right_rect.cols(),
        right_rect.rows()
    );

    // 测试错误处理
    println!("✅ 测试错误处理...");

    // 测试无效图像尺寸
    match Rectifier::new(&test_calibration, 0, 480) {
        Err(RectifyError::Invalid(msg)) => {
            println!("✅ 正确捕获无效尺寸错误: {}", msg);
        }
        _ => {
            println!("❌ 应该返回无效尺寸错误");
            return Err("Expected invalid size error".into());
        }
    }

    // 测试图像尺寸不匹配
    let wrong_size_image = create_test_image(320, 240)?;
    match rectifier.rectify_left(&wrong_size_image) {
        Err(RectifyError::Invalid(msg)) => {
            println!("✅ 正确捕获尺寸不匹配错误: {}", msg);
        }
        _ => {
            println!("❌ 应该返回尺寸不匹配错误");
            return Err("Expected size mismatch error".into());
        }
    }

    println!("\n🎉 stereo-rectifier 测试完成！");
    println!("📊 测试结果:");
    println!("  - Rectifier 创建: ✅");
    println!("  - 左相机校正: ✅");
    println!("  - 右相机校正: ✅");
    println!("  - 双相机校正: ✅");
    println!("  - 错误处理: ✅");

    Ok(())
}

fn create_test_calibration() -> calib::StereoCalibration {
    calib::StereoCalibration {
        left: calib::CameraIntrinsics {
            camera_matrix: [[800.0, 0.0, 320.0], [0.0, 800.0, 240.0], [0.0, 0.0, 1.0]],
            dist_coeffs: [-0.1, 0.05, 0.001, 0.002, -0.01],
        },
        right: calib::CameraIntrinsics {
            camera_matrix: [[810.0, 0.0, 325.0], [0.0, 810.0, 245.0], [0.0, 0.0, 1.0]],
            dist_coeffs: [-0.12, 0.06, 0.0015, 0.0025, -0.012],
        },
        extrinsics: calib::StereoExtrinsics {
            rotation: [
                [0.999, -0.001, 0.002],
                [0.001, 0.999, -0.003],
                [-0.002, 0.003, 0.999],
            ],
            translation: [-120.5, 0.8, -1.2],
        },
    }
}

fn create_test_image(width: i32, height: i32) -> opencv::Result<Mat> {
    // 创建一个简单的测试图像（彩色）
    let mut img = Mat::new_rows_cols_with_default(
        height,
        width,
        CV_8UC3,
        opencv::core::Scalar::new(128.0, 64.0, 192.0, 0.0),
    )?;

    // 添加一些简单的图案来验证校正效果
    for y in 0..height {
        for x in 0..width {
            if (x / 20 + y / 20) % 2 == 0 {
                let pixel = img.at_2d_mut::<opencv::core::Vec3b>(y, x)?;
                *pixel = opencv::core::Vec3b::from([255, 255, 255]);
            }
        }
    }

    Ok(img)
}
