//! Simple Camera Control Test
//!
//! This is a minimal test to verify the camera control functionality works.

use usb_camera::{CameraControl, CameraProperty, V4L2DeviceManager};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("🎛️ Simple Camera Control Test");
    println!("=============================");

    // Find available cameras
    let devices = V4L2DeviceManager::discover_devices()?;

    if devices.is_empty() {
        println!("❌ 未发现任何相机设备");
        println!("ℹ️  请确保有USB相机连接到系统");
        return Ok(());
    }

    println!("✅ 发现 {} 个相机设备:", devices.len());
    for (i, device) in devices.iter().enumerate() {
        println!("  {}. {} ({})", i + 1, device.name, device.path.display());
    }

    // Test with the first available camera
    let device = &devices[0];
    println!("\n📷 测试设备: {} ({})", device.name, device.path.display());

    // Create camera control
    let mut controller = CameraControl::new(&device.path.to_string_lossy());

    // Get all available parameters
    println!("\n🔍 获取可用参数...");
    match controller.get_all_parameters() {
        Ok(params) => {
            if params.is_empty() {
                println!("⚠️  该设备不支持参数控制");
            } else {
                println!("✅ 发现 {} 个可控制参数:", params.len());
                for (property, range) in &params {
                    println!("  {:?}: {} (范围: {}-{})", 
                             property, range.current, range.min, range.max);
                }

                // Test setting brightness if available
                if let Some(brightness_range) = params.get(&CameraProperty::Brightness) {
                    println!("\n💡 测试亮度控制...");
                    let original_value = brightness_range.current;
                    let middle_value = (brightness_range.min + brightness_range.max) / 2;
                    
                    println!("  原始亮度: {}", original_value);
                    println!("  设置亮度为中间值: {}", middle_value);
                    
                    match controller.set_parameter(&CameraProperty::Brightness, middle_value) {
                        Ok(_) => {
                            println!("  ✅ 亮度设置成功");
                            
                            // Restore original value
                            std::thread::sleep(std::time::Duration::from_millis(500));
                            match controller.set_parameter(&CameraProperty::Brightness, original_value) {
                                Ok(_) => println!("  ✅ 亮度已恢复到原始值"),
                                Err(e) => println!("  ⚠️  恢复亮度失败: {}", e),
                            }
                        }
                        Err(e) => println!("  ❌ 亮度设置失败: {}", e),
                    }
                } else {
                    println!("\n⚠️  该设备不支持亮度控制");
                }
            }
        }
        Err(e) => {
            println!("❌ 获取参数失败: {}", e);
            println!("ℹ️  这可能是因为:");
            println!("   - 设备不支持V4L2控制");
            println!("   - 缺少v4l2-ctl工具");
            println!("   - 权限不足");
        }
    }

    println!("\n🎉 测试完成！");
    Ok(())
}
