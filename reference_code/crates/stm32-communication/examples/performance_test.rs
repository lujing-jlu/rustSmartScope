//! STM32 Communication Performance Test
//!
//! This example tests the performance and reliability of the STM32 communication
//! under various conditions including rapid commands and stress testing.

use std::thread;
use std::time::{Duration, Instant};
use stm32_communication::{ControllerMessage, Stm32Controller};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize logging
    env_logger::init();

    println!("STM32 Communication - Performance Test");
    println!("======================================");

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
        println!("✗ Device not connected - running simulation mode");
        println!("Note: Some tests will be skipped without actual hardware");
    }

    // Test 1: Rapid light level changes
    println!("\n--- Test 1: Rapid Light Level Changes ---");
    let start_time = Instant::now();
    let iterations = 100;

    for i in 0..iterations {
        let level = (i % 6) as u8; // 0-5
        if let Err(e) = controller.set_light_level(level) {
            println!("Error setting light level {}: {}", level, e);
        }
    }

    let elapsed = start_time.elapsed();
    println!(
        "Completed {} light level changes in {:?}",
        iterations, elapsed
    );
    println!("Average time per command: {:?}", elapsed / iterations);

    // Test 2: Rapid servo movements
    println!("\n--- Test 2: Rapid Servo Movements ---");
    let start_time = Instant::now();
    let iterations = 50;

    for i in 0..iterations {
        let angle = 30.0 + (120.0 * (i as f32 / iterations as f32));
        if let Err(e) = controller.set_servo_angles(angle, angle) {
            println!("Error setting servo angles to {:.1}°: {}", angle, e);
        }
    }

    let elapsed = start_time.elapsed();
    println!("Completed {} servo movements in {:?}", iterations, elapsed);
    println!("Average time per movement: {:?}", elapsed / iterations);

    // Test 3: Batch configuration stress test
    println!("\n--- Test 3: Batch Configuration Stress Test ---");
    let start_time = Instant::now();
    let iterations = 25;

    for i in 0..iterations {
        let light_level = (i % 6) as u8;
        let lens_locked = i % 2 == 0;
        let lens_speed = (i % 3) as u8;
        let servo_x = 30.0 + (120.0 * ((i % 10) as f32 / 10.0));
        let servo_y = 30.0 + (120.0 * (((i + 5) % 10) as f32 / 10.0));

        if let Err(e) =
            controller.set_device_config(light_level, lens_locked, lens_speed, servo_x, servo_y)
        {
            println!("Error in batch config {}: {}", i, e);
        }
    }

    let elapsed = start_time.elapsed();
    println!(
        "Completed {} batch configurations in {:?}",
        iterations, elapsed
    );
    println!("Average time per batch: {:?}", elapsed / iterations);

    // Test 4: Status reading performance
    if controller.is_connected() {
        println!("\n--- Test 4: Status Reading Performance ---");
        let start_time = Instant::now();
        let iterations = 50;
        let mut successful_reads = 0;

        for i in 0..iterations {
            match controller.read_device_status() {
                Ok(status) => {
                    successful_reads += 1;
                    if i % 10 == 0 {
                        println!(
                            "Status read {}: temp={:.1}°C, battery={:.1}%",
                            i, status.temperature, status.battery_value
                        );
                    }
                }
                Err(e) => {
                    println!("Error reading status {}: {}", i, e);
                }
            }
        }

        let elapsed = start_time.elapsed();
        println!(
            "Completed {}/{} status reads in {:?}",
            successful_reads, iterations, elapsed
        );
        if successful_reads > 0 {
            println!("Average time per read: {:?}", elapsed / successful_reads);
        }
    }

    // Test 5: Continuous monitoring stress test
    if controller.is_connected() {
        println!("\n--- Test 5: Continuous Monitoring Stress Test ---");
        controller.start_periodic_updates(100); // 100ms interval

        let start_time = Instant::now();
        let test_duration = Duration::from_secs(10);
        let mut message_count = 0;
        let mut error_count = 0;

        while start_time.elapsed() < test_duration {
            if let Some(message) = controller.try_receive_message() {
                message_count += 1;
                match message {
                    ControllerMessage::StatusUpdate(status) => {
                        if message_count % 20 == 0 {
                            println!(
                                "Monitoring update {}: temp={:.1}°C, servo=({:.1}°, {:.1}°)",
                                message_count,
                                status.temperature,
                                status.servo_x_angle,
                                status.servo_y_angle
                            );
                        }
                    }
                    ControllerMessage::Error(_) => {
                        error_count += 1;
                    }
                    _ => {}
                }
            }
            thread::sleep(Duration::from_millis(10));
        }

        controller.stop_periodic_updates();

        let elapsed = start_time.elapsed();
        println!(
            "Received {} messages ({} errors) in {:?}",
            message_count, error_count, elapsed
        );
        if message_count > 0 {
            println!(
                "Average message rate: {:.1} messages/sec",
                message_count as f64 / elapsed.as_secs_f64()
            );
        }
    }

    // Test 6: Error recovery test
    println!("\n--- Test 6: Error Recovery Test ---");

    // Test invalid light level
    match controller.set_light_level(10) {
        Ok(_) => println!("❌ Invalid light level: Expected error but got success"),
        Err(e) => println!("✅ Invalid light level: Correctly caught error: {}", e),
    }

    // Test invalid servo X angle
    match controller.set_servo_x_angle(200.0) {
        Ok(_) => println!("❌ Invalid servo X angle: Expected error but got success"),
        Err(e) => println!("✅ Invalid servo X angle: Correctly caught error: {}", e),
    }

    // Test invalid servo Y angle
    match controller.set_servo_y_angle(-10.0) {
        Ok(_) => println!("❌ Invalid servo Y angle: Expected error but got success"),
        Err(e) => println!("✅ Invalid servo Y angle: Correctly caught error: {}", e),
    }

    // Test invalid lens speed
    match controller.set_lens_speed(5) {
        Ok(_) => println!("❌ Invalid lens speed: Expected error but got success"),
        Err(e) => println!("✅ Invalid lens speed: Correctly caught error: {}", e),
    }

    // Test 7: Memory and resource usage
    println!("\n--- Test 7: Memory Usage Test ---");
    let start_time = Instant::now();
    let iterations = 1000;

    for i in 0..iterations {
        // Create and drop device configs to test memory management
        let _config = controller.get_device_config();

        // Rapid servo movements
        let angle = 30.0 + (120.0 * ((i % 100) as f32 / 100.0));
        let _ = controller.set_servo_angles(angle, 150.0 - angle);

        if i % 100 == 0 {
            println!("Memory test iteration: {}/{}", i, iterations);
        }
    }

    let elapsed = start_time.elapsed();
    println!(
        "Completed {} memory test iterations in {:?}",
        iterations, elapsed
    );

    // Final cleanup and summary
    println!("\n--- Performance Test Summary ---");
    controller.reset_to_defaults()?;

    if controller.is_connected() {
        match controller.read_device_status() {
            Ok(status) => {
                println!("Final device status after stress testing:");
                println!("  Temperature: {:.1}°C", status.temperature);
                println!("  Battery: {:.1}%", status.battery_value);
                println!("  All systems operational: {}", status.is_valid);
            }
            Err(e) => println!("Failed to read final status: {}", e),
        }
    }

    println!("\n✓ Performance test completed successfully!");
    println!("Note: For accurate performance measurements, run with actual hardware connected.");

    Ok(())
}
