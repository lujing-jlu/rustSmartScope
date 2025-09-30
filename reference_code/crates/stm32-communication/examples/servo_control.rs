//! STM32 Communication Example - Servo Control
//!
//! This example demonstrates the new servo control and lens management features
//! of the updated STM32 communication protocol.

use std::thread;
use std::time::Duration;
use stm32_communication::Stm32Controller;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize logging
    env_logger::init();

    println!("STM32 Communication - Servo Control Example");
    println!("============================================");

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

    // Demonstrate light control (new 0-5 range)
    println!("\n--- Light Control Demo ---");
    for level in 0..=5 {
        println!("Setting light level to {} ({}%)", level, level * 20);
        controller.set_light_level(level)?;
        thread::sleep(Duration::from_millis(500));
    }

    // Demonstrate lens control
    println!("\n--- Lens Control Demo ---");

    // Set lens to locked state
    println!("Locking lens...");
    controller.set_lens_locked(true)?;
    thread::sleep(Duration::from_millis(500));

    // Test different lens speeds
    let speeds = [(0, "快调"), (1, "慢调"), (2, "微调")];

    for (speed, name) in speeds.iter() {
        println!("Setting lens speed to {} ({})", speed, name);
        controller.set_lens_speed(*speed)?;
        thread::sleep(Duration::from_millis(500));
    }

    // Demonstrate servo control
    println!("\n--- Servo Control Demo ---");

    // Test individual servo control
    println!("Moving servo X to 45°");
    controller.set_servo_x_angle(45.0)?;
    thread::sleep(Duration::from_millis(1000));

    println!("Moving servo Y to 135°");
    controller.set_servo_y_angle(135.0)?;
    thread::sleep(Duration::from_millis(1000));

    // Test combined servo control
    println!("Moving both servos to (60°, 120°)");
    controller.set_servo_angles(60.0, 120.0)?;
    thread::sleep(Duration::from_millis(1000));

    // Test servo movement pattern
    println!("Demonstrating servo movement pattern...");
    let positions = [
        (90.0, 90.0),  // Center
        (30.0, 90.0),  // Left
        (150.0, 90.0), // Right
        (90.0, 30.0),  // Down
        (90.0, 150.0), // Up
        (90.0, 90.0),  // Back to center
    ];

    for (x, y) in positions.iter() {
        println!("Moving to ({:.0}°, {:.0}°)", x, y);
        controller.set_servo_angles(*x, *y)?;
        thread::sleep(Duration::from_millis(800));
    }

    // Read device status
    println!("\n--- Device Status ---");
    match controller.read_device_status() {
        Ok(status) => {
            println!("Temperature: {:.1}°C", status.temperature);
            println!(
                "Battery: {:.1}% ({} level)",
                status.battery_value, status.battery_level
            );
            println!("Brightness: {} (0-5 scale)", status.brightness);
            println!("Lens locked: {}", status.lens_locked);
            println!(
                "Lens speed: {} ({})",
                status.lens_speed,
                match status.lens_speed {
                    0 => "快调",
                    1 => "慢调",
                    2 => "微调",
                    _ => "未知",
                }
            );
            println!("Servo X angle: {:.1}°", status.servo_x_angle);
            println!("Servo Y angle: {:.1}°", status.servo_y_angle);
            println!("Data valid: {}", status.is_valid);
        }
        Err(e) => println!("Failed to read device status: {}", e),
    }

    // Reset to defaults
    println!("\n--- Resetting to Defaults ---");
    controller.reset_servo_angles()?;
    controller.set_lens_locked(false)?;
    controller.set_lens_speed(0)?;
    controller.set_light_level(0)?; // Turn off light

    println!("✓ Demo completed successfully!");

    Ok(())
}
