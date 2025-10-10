use stereo_calibration_reader::*;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("测试 stereo-calibration-reader 基本功能...");
    
    // 测试加载标定数据
    let test_dir = "test_data";
    match load_from_dir(test_dir) {
        Ok(calibration) => {
            println!("✅ 成功加载标定数据");
            
            // 验证左相机内参
            println!("左相机内参矩阵:");
            for row in &calibration.left.camera_matrix {
                println!("  {:?}", row);
            }
            println!("左相机畸变系数: {:?}", calibration.left.dist_coeffs);
            
            // 验证右相机内参
            println!("右相机内参矩阵:");
            for row in &calibration.right.camera_matrix {
                println!("  {:?}", row);
            }
            println!("右相机畸变系数: {:?}", calibration.right.dist_coeffs);
            
            // 验证外参
            println!("旋转矩阵:");
            for row in &calibration.extrinsics.rotation {
                println!("  {:?}", row);
            }
            println!("平移向量: {:?}", calibration.extrinsics.translation);
            
            // 验证数据合理性
            let left_fx = calibration.left.camera_matrix[0][0];
            let left_fy = calibration.left.camera_matrix[1][1];
            let baseline = calibration.extrinsics.translation[0].abs();
            
            println!("\n📊 关键参数:");
            println!("  左相机焦距: fx={:.1}, fy={:.1}", left_fx, left_fy);
            println!("  基线距离: {:.1}mm", baseline);
            
            if left_fx > 0.0 && left_fy > 0.0 && baseline > 0.0 {
                println!("✅ 参数验证通过");
            } else {
                println!("❌ 参数验证失败");
                return Err("Invalid calibration parameters".into());
            }
        }
        Err(e) => {
            println!("❌ 加载标定数据失败: {}", e);
            return Err(e.into());
        }
    }
    
    // 测试错误处理
    println!("\n测试错误处理...");
    match load_from_dir("nonexistent") {
        Ok(_) => {
            println!("❌ 应该返回错误");
            return Err("Expected error for nonexistent directory".into());
        }
        Err(e) => {
            println!("✅ 正确处理不存在目录错误: {}", e);
        }
    }
    
    println!("\n🎉 stereo-calibration-reader 测试完成！");
    Ok(())
}
