//! Camera Parameter Control Example
//!
//! This example demonstrates:
//! - Reading camera parameter ranges and current values
//! - Setting camera parameters (brightness, contrast, etc.)
//! - Parameter validation and error handling
//! - Batch parameter operations

use std::collections::HashMap;
use usb_camera::{
    control::{CameraControl, CameraProperty, ParameterRange},
    device::V4L2DeviceManager,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize logging
    env_logger::init();

    println!("🎛️ USB Camera Parameter Control Example");
    println!("{}", "=".repeat(50));

    // Find available cameras
    let devices = V4L2DeviceManager::discover_devices()?;

    if devices.is_empty() {
        println!("❌ 未发现任何相机设备");
        return Ok(());
    }

    // Test with the first available camera
    let device = &devices[0];
    println!("📷 测试设备: {} ({})", device.name, device.path.display());

    // Test parameter control
    test_parameter_reading(&device.path.to_string_lossy())?;
    test_parameter_setting(&device.path.to_string_lossy())?;
    test_batch_operations(&device.path.to_string_lossy())?;

    println!("\n🎉 示例完成！");
    Ok(())
}

/// Test reading camera parameters
fn test_parameter_reading(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n📖 测试参数读取功能");
    println!("{}", "-".repeat(30));

    let mut controller = CameraControl::new(device_path);

    // Get all available parameters
    println!("🔍 获取所有可用参数...");
    let available_params = controller.get_all_parameters()?;

    if available_params.is_empty() {
        println!("⚠️  该设备不支持参数控制");
        return Ok(());
    }

    println!("✅ 发现 {} 个可控制参数:", available_params.len());

    for (property, range) in &available_params {
        println!("\n📊 {:?}:", property);
        println!(
            "   范围: {} - {} (步长: {})",
            range.min, range.max, range.step
        );
        println!("   默认值: {}", range.default);
        println!("   当前值: {}", range.current);

        // Calculate percentage of current value
        let percentage = if range.max > range.min {
            ((range.current - range.min) as f64 / (range.max - range.min) as f64 * 100.0) as i32
        } else {
            0
        };
        println!("   百分比: {}%", percentage);
    }

    Ok(())
}

/// Test setting camera parameters
fn test_parameter_setting(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n🎛️ 测试参数设置功能");
    println!("{}", "-".repeat(30));

    let mut controller = CameraControl::new(device_path);

    // Test setting brightness if available
    let available_params = controller.get_all_parameters()?;
    if available_params.contains_key(&CameraProperty::Brightness) {
        println!("💡 测试亮度控制...");

        // Get current brightness
        let original_range = controller.get_parameter_range(&CameraProperty::Brightness)?;
        let original_value = original_range.current;
        println!(
            "   原始亮度: {} ({}%)",
            original_value,
            ((original_value - original_range.min) as f64
                / (original_range.max - original_range.min) as f64
                * 100.0) as i32
        );

        // Set to middle value
        let middle_value = (original_range.min + original_range.max) / 2;
        println!("   设置亮度为中间值: {}", middle_value);
        controller.set_parameter(&CameraProperty::Brightness, middle_value)?;

        // Verify the change
        let new_range = controller.get_parameter_range(&CameraProperty::Brightness)?;
        println!(
            "   新亮度值: {} ({}%)",
            new_range.current,
            ((new_range.current - new_range.min) as f64 / (new_range.max - new_range.min) as f64
                * 100.0) as i32
        );

        // Restore original value
        std::thread::sleep(std::time::Duration::from_secs(1));
        println!("   恢复原始亮度: {}", original_value);
        controller.set_parameter(&CameraProperty::Brightness, original_value)?;
    }

    // Test setting contrast if available
    let available_params = controller.get_all_parameters()?;
    if available_params.contains_key(&CameraProperty::Contrast) {
        println!("\n🌈 测试对比度控制...");

        let original_range = controller.get_parameter_range(&CameraProperty::Contrast)?;
        let original_value = original_range.current;
        println!("   原始对比度: {}", original_value);

        // Increase contrast by 20%
        let range_size = original_range.max - original_range.min;
        let new_value = (original_value + range_size / 5).min(original_range.max);

        if new_value != original_value {
            println!("   增加对比度: {} -> {}", original_value, new_value);
            controller.set_parameter(&CameraProperty::Contrast, new_value)?;

            std::thread::sleep(std::time::Duration::from_secs(1));

            // Restore
            println!("   恢复原始对比度: {}", original_value);
            controller.set_parameter(&CameraProperty::Contrast, original_value)?;
        }
    }

    Ok(())
}

/// Test batch parameter operations
fn test_batch_operations(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    println!("\n📦 测试批量参数操作");
    println!("{}", "-".repeat(30));

    let mut controller = CameraControl::new(device_path);
    let available_params = controller.get_all_parameters()?;

    // Save current parameters
    println!("💾 保存当前参数配置...");
    let mut original_values = HashMap::new();

    for (property, range) in &available_params {
        original_values.insert(property.clone(), range.current);
        println!("   {:?}: {}", property, range.current);
    }

    // Apply a preset configuration
    println!("\n🎨 应用预设配置 (高对比度模式)...");
    let mut preset_applied = false;

    if available_params.contains_key(&CameraProperty::Contrast) {
        if let Some(range) = available_params.get(&CameraProperty::Contrast) {
            let high_contrast = range.min + (range.max - range.min) * 3 / 4;
            controller.set_parameter(&CameraProperty::Contrast, high_contrast)?;
            println!("   对比度设置为: {}", high_contrast);
            preset_applied = true;
        }
    }

    if available_params.contains_key(&CameraProperty::Saturation) {
        if let Some(range) = available_params.get(&CameraProperty::Saturation) {
            let high_saturation = range.min + (range.max - range.min) * 3 / 4;
            controller.set_parameter(&CameraProperty::Saturation, high_saturation)?;
            println!("   饱和度设置为: {}", high_saturation);
            preset_applied = true;
        }
    }

    if preset_applied {
        println!("✅ 预设配置已应用，等待2秒...");
        std::thread::sleep(std::time::Duration::from_secs(2));
    }

    // Restore original parameters
    println!("\n🔄 恢复原始参数配置...");
    for (property, value) in original_values {
        match controller.set_parameter(&property, value) {
            Ok(_) => println!("   {:?}: {} ✅", property, value),
            Err(e) => println!("   {:?}: {} ❌ ({})", property, value, e),
        }
    }

    println!("✅ 参数配置已恢复");

    Ok(())
}
