//! Advanced STM32 Communication Example
//! 
//! This example demonstrates advanced features including:
//! - Batch configuration updates
//! - Servo calibration
//! - Status monitoring
//! - Error handling and recovery

use std::thread;
use std::time::Duration;
use stm32_communication::{Stm32Controller, DeviceConfig, ControllerMessage};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize logging
    env_logger::init();
    
    println!("STM32 Communication - Advanced Control Example");
    println!("==============================================");
    
    // Create controller with default configuration
    let mut controller = Stm32Controller::new(None);
    
    // Initialize the controller
    match controller.initialize() {
        Ok(()) => println!("✓ STM32 controller initialized successfully"),
        Err(e) => {
            println!("✗ Failed to initialize controller: {}", e);
            return Err(e.into());
        }
    }
    
    // Check if device is connected
    if !controller.is_connected() {
        println!("✗ Device not connected");
        return Ok(());
    }
    
    println!("\n--- Device Information ---");
    let device_info = controller.get_device_info();
    println!("Manufacturer: {}", device_info.manufacturer);
    println!("Product: {}", device_info.product);
    
    // Demonstrate batch configuration
    println!("\n--- Batch Configuration Demo ---");
    let configs = [
        DeviceConfig {
            light_level: 1,
            lens_locked: false,
            lens_speed: 0,
            servo_x_angle: 45.0,
            servo_y_angle: 45.0,
        },
        DeviceConfig {
            light_level: 3,
            lens_locked: true,
            lens_speed: 1,
            servo_x_angle: 90.0,
            servo_y_angle: 90.0,
        },
        DeviceConfig {
            light_level: 5,
            lens_locked: true,
            lens_speed: 2,
            servo_x_angle: 135.0,
            servo_y_angle: 135.0,
        },
    ];
    
    for (i, config) in configs.iter().enumerate() {
        println!("Applying configuration {}: light={}, lens_locked={}, servo=({:.0}°, {:.0}°)", 
                 i + 1, config.light_level, config.lens_locked, 
                 config.servo_x_angle, config.servo_y_angle);
        
        controller.set_device_config(
            config.light_level,
            config.lens_locked,
            config.lens_speed,
            config.servo_x_angle,
            config.servo_y_angle,
        )?;
        
        thread::sleep(Duration::from_millis(1000));
        
        // Read back configuration
        let current_config = controller.get_device_config();
        println!("  Current config: {:?}", current_config);
    }
    
    // Demonstrate servo calibration
    println!("\n--- Servo Calibration Demo ---");
    controller.calibrate_servos()?;
    
    // Demonstrate status monitoring
    println!("\n--- Status Monitoring Demo ---");
    controller.start_periodic_updates(500); // 500ms interval
    
    let mut status_count = 0;
    let max_status_updates = 10;
    
    while status_count < max_status_updates {
        if let Some(message) = controller.try_receive_message() {
            match message {
                ControllerMessage::StatusUpdate(status) => {
                    status_count += 1;
                    println!("Status update {}: temp={:.1}°C, battery={:.1}%, brightness={}, servo=({:.1}°, {:.1}°)",
                             status_count, status.temperature, status.battery_value, 
                             status.brightness, status.servo_x_angle, status.servo_y_angle);
                }
                ControllerMessage::ConnectionChanged(connected) => {
                    println!("Connection status changed: {}", if connected { "Connected" } else { "Disconnected" });
                }
                ControllerMessage::Error(e) => {
                    println!("Error received: {}", e);
                }
                ControllerMessage::Shutdown => {
                    println!("Shutdown message received");
                    break;
                }
            }
        }
        thread::sleep(Duration::from_millis(100));
    }
    
    controller.stop_periodic_updates();
    
    // Demonstrate error handling and recovery
    println!("\n--- Error Handling Demo ---");
    
    // Try to set invalid servo angles
    match controller.set_servo_x_angle(200.0) {
        Ok(_) => println!("Unexpected success setting invalid angle"),
        Err(e) => println!("Expected error for invalid angle: {}", e),
    }
    
    // Try to set invalid lens speed
    match controller.set_lens_speed(5) {
        Ok(_) => println!("Unexpected success setting invalid lens speed"),
        Err(e) => println!("Expected error for invalid lens speed: {}", e),
    }
    
    // Try to set invalid light level
    match controller.set_light_level(10) {
        Ok(_) => println!("Unexpected success setting invalid light level"),
        Err(e) => println!("Expected error for invalid light level: {}", e),
    }
    
    // Demonstrate smooth servo movement
    println!("\n--- Smooth Servo Movement Demo ---");
    let start_x = 30.0;
    let end_x = 150.0;
    let start_y = 30.0;
    let end_y = 150.0;
    let steps = 10;
    
    for i in 0..=steps {
        let progress = i as f32 / steps as f32;
        let x = start_x + (end_x - start_x) * progress;
        let y = start_y + (end_y - start_y) * progress;
        
        println!("Smooth movement step {}: ({:.1}°, {:.1}°)", i + 1, x, y);
        controller.set_servo_angles(x, y)?;
        thread::sleep(Duration::from_millis(200));
    }
    
    // Reset to defaults
    println!("\n--- Resetting to Defaults ---");
    controller.reset_to_defaults()?;
    
    // Final status check
    println!("\n--- Final Status Check ---");
    match controller.read_device_status() {
        Ok(status) => {
            println!("Final device status:");
            println!("  Temperature: {:.1}°C", status.temperature);
            println!("  Battery: {:.1}% ({} level)", status.battery_value, status.battery_level);
            println!("  Brightness: {} (0-5 scale)", status.brightness);
            println!("  Lens locked: {}", status.lens_locked);
            println!("  Lens speed: {} ({})", status.lens_speed, 
                     match status.lens_speed {
                         0 => "快调",
                         1 => "慢调",
                         2 => "微调",
                         _ => "未知"
                     });
            println!("  Servo X angle: {:.1}°", status.servo_x_angle);
            println!("  Servo Y angle: {:.1}°", status.servo_y_angle);
            println!("  Data valid: {}", status.is_valid);
            println!("  Timestamp: {}", status.timestamp);
        }
        Err(e) => println!("Failed to read final device status: {}", e),
    }
    
    println!("\n✓ Advanced control demo completed successfully!");
    
    Ok(())
}
