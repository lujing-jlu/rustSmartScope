//! Device management and status monitoring

use crate::error::{Stm32Error, Stm32Result};
use crate::protocol::{Command, DeviceParams, DeviceStatus, LightLevel, PacketBuilder};
use log::{debug, info};
use serde::{Deserialize, Serialize};

/// Device information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceInfo {
    pub manufacturer: String,
    pub product: String,
    pub vendor_id: u16,
    pub product_id: u16,
    pub firmware_version: Option<String>,
    pub serial_number: Option<String>,
}

/// Device capabilities
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceCapabilities {
    pub supports_light_control: bool,
    pub supports_temperature: bool,
    pub supports_battery: bool,
    pub max_light_levels: u8,
    pub temperature_range: (f32, f32),
    pub battery_range: (f32, f32),
}

impl Default for DeviceCapabilities {
    fn default() -> Self {
        Self {
            supports_light_control: true,
            supports_temperature: true,
            supports_battery: true,
            max_light_levels: 6, // 0-5 共6个等级
            temperature_range: (-40.0, 85.0),
            battery_range: (0.0, 100.0),
        }
    }
}

/// Device manager
pub struct DeviceManager {
    packet_builder: PacketBuilder,
    capabilities: DeviceCapabilities,
    light_levels: Vec<LightLevel>,
    current_light_level: u8,
    last_status: DeviceStatus,
    device_params: DeviceParams,
}

impl DeviceManager {
    /// Create a new device manager
    pub fn new() -> Self {
        let mut light_levels = Vec::new();
        light_levels.push(LightLevel::new(0, 0)); // 0 - 关闭
        light_levels.push(LightLevel::new(1, 20)); // 1 - 20%
        light_levels.push(LightLevel::new(2, 40)); // 2 - 40%
        light_levels.push(LightLevel::new(3, 60)); // 3 - 60%
        light_levels.push(LightLevel::new(4, 80)); // 4 - 80%
        light_levels.push(LightLevel::new(5, 100)); // 5 - 100% 最大亮度

        Self {
            packet_builder: PacketBuilder::new(),
            capabilities: DeviceCapabilities::default(),
            light_levels,
            current_light_level: 0,
            last_status: DeviceStatus::default(),
            device_params: DeviceParams::default(),
        }
    }

    /// Get light levels
    pub fn get_light_levels(&self) -> &[LightLevel] {
        &self.light_levels
    }

    /// Get current light level index
    pub fn get_current_light_level(&self) -> u8 {
        self.current_light_level
    }

    /// Get current brightness percentage
    pub fn get_current_brightness_percentage(&self) -> u8 {
        if self.current_light_level < self.light_levels.len() as u8 {
            self.light_levels[self.current_light_level as usize].percentage
        } else {
            0
        }
    }

    /// Set light level
    pub fn set_light_level(&mut self, level_index: u8) -> Stm32Result<()> {
        if level_index >= self.light_levels.len() as u8 {
            return Err(Stm32Error::ConfigurationError(format!(
                "Light level index out of range: {}",
                level_index
            )));
        }

        self.current_light_level = level_index;
        let level = &self.light_levels[level_index as usize];

        info!(
            "Setting light level to {} ({}%)",
            level_index, level.percentage
        );

        // Update device parameters
        self.device_params.brightness = level.brightness;

        Ok(())
    }

    /// Toggle light level (cycle through levels)
    pub fn toggle_light_level(&mut self) -> Stm32Result<()> {
        let next_level = (self.current_light_level + 1) % self.light_levels.len() as u8;
        self.set_light_level(next_level)
    }

    /// Set lens lock status
    pub fn set_lens_locked(&mut self, locked: bool) -> Stm32Result<()> {
        self.device_params.lens_locked = locked;
        info!("Setting lens lock to: {}", locked);
        Ok(())
    }

    /// Get lens lock status
    pub fn is_lens_locked(&self) -> bool {
        self.device_params.lens_locked
    }

    /// Set lens speed
    pub fn set_lens_speed(&mut self, speed: u8) -> Stm32Result<()> {
        if speed > 2 {
            return Err(Stm32Error::ConfigurationError(format!(
                "Invalid lens speed: {} (must be 0-2)",
                speed
            )));
        }
        self.device_params.lens_speed = speed;
        info!(
            "Setting lens speed to: {} ({})",
            speed,
            match speed {
                0 => "快调",
                1 => "慢调",
                2 => "微调",
                _ => "未知",
            }
        );
        Ok(())
    }

    /// Get lens speed
    pub fn get_lens_speed(&self) -> u8 {
        self.device_params.lens_speed
    }

    /// Set servo X angle
    pub fn set_servo_x_angle(&mut self, angle: f32) -> Stm32Result<()> {
        if angle < 30.0 || angle > 150.0 {
            return Err(Stm32Error::ConfigurationError(format!(
                "Servo X angle out of range: {:.1} (must be 30.0-150.0)",
                angle
            )));
        }
        self.device_params.servo_x_angle = angle;
        info!("Setting servo X angle to: {:.1}°", angle);
        Ok(())
    }

    /// Set servo Y angle
    pub fn set_servo_y_angle(&mut self, angle: f32) -> Stm32Result<()> {
        if angle < 30.0 || angle > 150.0 {
            return Err(Stm32Error::ConfigurationError(format!(
                "Servo Y angle out of range: {:.1} (must be 30.0-150.0)",
                angle
            )));
        }
        self.device_params.servo_y_angle = angle;
        info!("Setting servo Y angle to: {:.1}°", angle);
        Ok(())
    }

    /// Set both servo angles
    pub fn set_servo_angles(&mut self, x_angle: f32, y_angle: f32) -> Stm32Result<()> {
        self.set_servo_x_angle(x_angle)?;
        self.set_servo_y_angle(y_angle)?;
        Ok(())
    }

    /// Get servo X angle
    pub fn get_servo_x_angle(&self) -> f32 {
        self.device_params.servo_x_angle
    }

    /// Get servo Y angle
    pub fn get_servo_y_angle(&self) -> f32 {
        self.device_params.servo_y_angle
    }

    /// Reset servo angles to default (90°)
    pub fn reset_servo_angles(&mut self) -> Stm32Result<()> {
        self.device_params.servo_x_angle = 90.0;
        self.device_params.servo_y_angle = 90.0;
        info!("Reset servo angles to default (90°, 90°)");
        Ok(())
    }

    /// Get last device status
    pub fn get_last_status(&self) -> &DeviceStatus {
        &self.last_status
    }

    /// Update device status
    pub fn update_status(&mut self, status: DeviceStatus) {
        let old_status = self.last_status.clone();
        self.last_status = status.clone();

        // Check for significant changes
        if (status.temperature - old_status.temperature).abs() > 0.1 {
            info!(
                "Temperature changed: {:.1}°C -> {:.1}°C",
                old_status.temperature, status.temperature
            );
        }

        if (status.battery_value - old_status.battery_value).abs() > 0.1 {
            info!(
                "Battery level changed: {:.1}% -> {:.1}%",
                old_status.battery_value, status.battery_value
            );
        }

        if status.brightness != old_status.brightness {
            info!(
                "Brightness changed: {} -> {}",
                old_status.brightness, status.brightness
            );
        }

        if status.lens_locked != old_status.lens_locked {
            info!(
                "Lens lock changed: {} -> {}",
                old_status.lens_locked, status.lens_locked
            );
        }

        if status.lens_speed != old_status.lens_speed {
            info!(
                "Lens speed changed: {} -> {}",
                old_status.lens_speed, status.lens_speed
            );
        }

        if (status.servo_x_angle - old_status.servo_x_angle).abs() > 0.1 {
            info!(
                "Servo X angle changed: {:.1}° -> {:.1}°",
                old_status.servo_x_angle, status.servo_x_angle
            );
        }

        if (status.servo_y_angle - old_status.servo_y_angle).abs() > 0.1 {
            info!(
                "Servo Y angle changed: {:.1}° -> {:.1}°",
                old_status.servo_y_angle, status.servo_y_angle
            );
        }
    }

    /// Get device parameters
    pub fn get_device_params(&self) -> &DeviceParams {
        &self.device_params
    }

    /// Update device parameters
    pub fn update_device_params(&mut self, params: DeviceParams) {
        self.device_params = params;
    }

    /// Build read command
    pub fn build_read_command(&self) -> Vec<u8> {
        let mut params = self.device_params.clone();
        params.command = Command::Read as u8;

        // Ensure current light level is preserved
        if self.current_light_level < self.light_levels.len() as u8 {
            let level = &self.light_levels[self.current_light_level as usize];
            params.brightness = level.brightness;
        }

        self.packet_builder.build_command(Command::Read, &params)
    }

    /// Build write command
    pub fn build_write_command(&self) -> Vec<u8> {
        debug!("DeviceManager: Building write command");
        let mut params = self.device_params.clone();
        debug!("DeviceManager: Cloned device params: {:?}", params);
        params.command = Command::Write as u8;
        debug!("DeviceManager: Set command to Write");

        debug!("DeviceManager: Calling packet_builder.build_command");
        let result = self.packet_builder.build_command(Command::Write, &params);
        debug!("DeviceManager: build_command returned");
        result
    }

    /// Parse response
    pub fn parse_response(&self, data: &[u8]) -> Stm32Result<DeviceStatus> {
        let status = self.packet_builder.parse_response(data)?;

        // Note: brightness is already parsed from the response
        // We don't need to preserve current light level here anymore

        Ok(status)
    }

    /// Get device capabilities
    pub fn get_capabilities(&self) -> &DeviceCapabilities {
        &self.capabilities
    }

    /// Set device capabilities
    pub fn set_capabilities(&mut self, capabilities: DeviceCapabilities) {
        self.capabilities = capabilities;
    }
}

impl Default for DeviceManager {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::protocol::{Command, PACKET_SIZE, PROTOCOL_HEADER};

    #[test]
    fn test_device_manager_creation() {
        let manager = DeviceManager::new();
        assert_eq!(manager.get_light_levels().len(), 6); // 0-5 共6个等级
        assert_eq!(manager.get_current_light_level(), 0);
        assert_eq!(manager.get_current_brightness_percentage(), 0); // 等级0对应0%
    }

    #[test]
    fn test_light_level_setting() {
        let mut manager = DeviceManager::new();

        // Test setting light level 2 (40%)
        assert!(manager.set_light_level(2).is_ok());
        assert_eq!(manager.get_current_light_level(), 2);
        assert_eq!(manager.get_current_brightness_percentage(), 40);

        // Test setting light level 5 (100%)
        assert!(manager.set_light_level(5).is_ok());
        assert_eq!(manager.get_current_light_level(), 5);
        assert_eq!(manager.get_current_brightness_percentage(), 100);
    }

    #[test]
    fn test_light_level_toggle() {
        let mut manager = DeviceManager::new();

        // Start at level 0 (0%)
        assert_eq!(manager.get_current_light_level(), 0);
        assert_eq!(manager.get_current_brightness_percentage(), 0);

        // Toggle to level 1 (20%)
        assert!(manager.toggle_light_level().is_ok());
        assert_eq!(manager.get_current_light_level(), 1);
        assert_eq!(manager.get_current_brightness_percentage(), 20);

        // Toggle to level 2 (40%)
        assert!(manager.toggle_light_level().is_ok());
        assert_eq!(manager.get_current_light_level(), 2);
        assert_eq!(manager.get_current_brightness_percentage(), 40);

        // Toggle to level 3 (60%)
        assert!(manager.toggle_light_level().is_ok());
        assert_eq!(manager.get_current_light_level(), 3);
        assert_eq!(manager.get_current_brightness_percentage(), 60);

        // Toggle to level 4 (80%)
        assert!(manager.toggle_light_level().is_ok());
        assert_eq!(manager.get_current_light_level(), 4);
        assert_eq!(manager.get_current_brightness_percentage(), 80);

        // Toggle to level 5 (100%)
        assert!(manager.toggle_light_level().is_ok());
        assert_eq!(manager.get_current_light_level(), 5);
        assert_eq!(manager.get_current_brightness_percentage(), 100);

        // Toggle back to level 0 (0%)
        assert!(manager.toggle_light_level().is_ok());
        assert_eq!(manager.get_current_light_level(), 0);
        assert_eq!(manager.get_current_brightness_percentage(), 0);
    }

    #[test]
    fn test_invalid_light_level() {
        let mut manager = DeviceManager::new();

        // Test setting invalid light level (now 6+ is invalid since we have 0-5)
        assert!(manager.set_light_level(6).is_err());
        assert!(manager.set_light_level(255).is_err());
    }

    #[test]
    fn test_device_status_update() {
        let mut manager = DeviceManager::new();

        let mut status = DeviceStatus::default();
        status.temperature = 25.5;
        status.battery_value = 85.5;
        status.is_valid = true;

        manager.update_status(status);

        let last_status = manager.get_last_status();
        assert_eq!(last_status.temperature, 25.5);
        assert_eq!(last_status.battery_value, 85.5);
        assert!(last_status.is_valid);
    }

    #[test]
    fn test_device_capabilities() {
        let manager = DeviceManager::new();

        let capabilities = manager.get_capabilities();
        assert!(capabilities.supports_light_control);
        assert!(capabilities.supports_temperature);
        assert!(capabilities.supports_battery);
        assert_eq!(capabilities.max_light_levels, 6); // 0-5 共6个等级
        assert_eq!(capabilities.temperature_range, (-40.0, 85.0));
        assert_eq!(capabilities.battery_range, (0.0, 100.0));
    }

    #[test]
    fn test_lens_control() {
        let mut manager = DeviceManager::new();

        // Test lens lock
        assert!(!manager.is_lens_locked());
        assert!(manager.set_lens_locked(true).is_ok());
        assert!(manager.is_lens_locked());

        // Test lens speed
        assert_eq!(manager.get_lens_speed(), 0);
        assert!(manager.set_lens_speed(1).is_ok());
        assert_eq!(manager.get_lens_speed(), 1);
        assert!(manager.set_lens_speed(2).is_ok());
        assert_eq!(manager.get_lens_speed(), 2);

        // Test invalid lens speed
        assert!(manager.set_lens_speed(3).is_err());
    }

    #[test]
    fn test_servo_control() {
        let mut manager = DeviceManager::new();

        // Test default angles
        assert_eq!(manager.get_servo_x_angle(), 90.0);
        assert_eq!(manager.get_servo_y_angle(), 90.0);

        // Test setting valid angles
        assert!(manager.set_servo_x_angle(45.0).is_ok());
        assert_eq!(manager.get_servo_x_angle(), 45.0);

        assert!(manager.set_servo_y_angle(135.0).is_ok());
        assert_eq!(manager.get_servo_y_angle(), 135.0);

        // Test setting both angles
        assert!(manager.set_servo_angles(60.0, 120.0).is_ok());
        assert_eq!(manager.get_servo_x_angle(), 60.0);
        assert_eq!(manager.get_servo_y_angle(), 120.0);

        // Test boundary values
        assert!(manager.set_servo_x_angle(30.0).is_ok());
        assert!(manager.set_servo_x_angle(150.0).is_ok());

        // Test invalid angles
        assert!(manager.set_servo_x_angle(29.9).is_err());
        assert!(manager.set_servo_x_angle(150.1).is_err());
        assert!(manager.set_servo_y_angle(29.9).is_err());
        assert!(manager.set_servo_y_angle(150.1).is_err());

        // Test reset
        assert!(manager.reset_servo_angles().is_ok());
        assert_eq!(manager.get_servo_x_angle(), 90.0);
        assert_eq!(manager.get_servo_y_angle(), 90.0);
    }

    #[test]
    fn test_command_building() {
        let manager = DeviceManager::new();

        let read_command = manager.build_read_command();
        assert_eq!(read_command.len(), PACKET_SIZE);
        assert_eq!(read_command[0], PROTOCOL_HEADER[0]);
        assert_eq!(read_command[1], PROTOCOL_HEADER[1]);
        assert_eq!(read_command[2], Command::Read as u8);

        let write_command = manager.build_write_command();
        assert_eq!(write_command.len(), PACKET_SIZE);
        assert_eq!(write_command[0], PROTOCOL_HEADER[0]);
        assert_eq!(write_command[1], PROTOCOL_HEADER[1]);
        assert_eq!(write_command[2], Command::Write as u8);
    }
}
