//! Main STM32 device controller

use crossbeam_channel::{unbounded, Receiver, Sender};
use log::{error, info, warn};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crate::device::{DeviceInfo, DeviceManager};
use crate::error::{Stm32Error, Stm32Result};
use crate::hid::{HidCommunication, HidConfig};
use crate::protocol::DeviceStatus;
use serde::{Deserialize, Serialize};

/// Device configuration snapshot
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceConfig {
    pub light_level: u8,
    pub lens_locked: bool,
    pub lens_speed: u8,
    pub servo_x_angle: f32,
    pub servo_y_angle: f32,
}

impl Default for DeviceConfig {
    fn default() -> Self {
        Self {
            light_level: 0,
            lens_locked: false,
            lens_speed: 0,
            servo_x_angle: 90.0,
            servo_y_angle: 90.0,
        }
    }
}

/// Controller message types
#[derive(Debug, Clone)]
pub enum ControllerMessage {
    /// Status update
    StatusUpdate(DeviceStatus),
    /// Connection status change
    ConnectionChanged(bool),
    /// Error occurred
    Error(Stm32Error),
    /// Shutdown request
    Shutdown,
}

/// STM32 device controller
pub struct Stm32Controller {
    hid_comm: Arc<Mutex<HidCommunication>>,
    device_manager: Arc<Mutex<DeviceManager>>,
    message_sender: Sender<ControllerMessage>,
    message_receiver: Receiver<ControllerMessage>,
    shutdown_flag: Arc<AtomicBool>,
    update_thread: Option<thread::JoinHandle<()>>,
}

impl Stm32Controller {
    /// Create a new STM32 controller
    pub fn new(config: Option<HidConfig>) -> Self {
        let config = config.unwrap_or_default();
        let hid_comm = Arc::new(Mutex::new(HidCommunication::new(config)));
        let device_manager = Arc::new(Mutex::new(DeviceManager::new()));
        let (message_sender, message_receiver) = unbounded();
        let shutdown_flag = Arc::new(AtomicBool::new(false));

        Self {
            hid_comm,
            device_manager,
            message_sender,
            message_receiver,
            shutdown_flag,
            update_thread: None,
        }
    }

    /// Initialize the controller
    pub fn initialize(&mut self) -> Stm32Result<()> {
        info!("Initializing STM32 controller...");

        // Open HID device
        let mut hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.open()?;

        // Send connection status
        let _ = self
            .message_sender
            .send(ControllerMessage::ConnectionChanged(true));

        // Set initial light level to maximum
        let mut device_manager = self.device_manager.lock().unwrap();
        device_manager.set_light_level(0)?; // 100% brightness

        info!("STM32 controller initialized successfully");
        Ok(())
    }

    /// Shutdown the controller
    pub fn shutdown(&mut self) {
        info!("Shutting down STM32 controller...");

        // Set shutdown flag
        self.shutdown_flag.store(true, Ordering::SeqCst);

        // Wait for update thread to finish
        if let Some(thread) = self.update_thread.take() {
            if let Err(e) = thread.join() {
                error!("Failed to join update thread: {:?}", e);
            }
        }

        // Close HID device
        let mut hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.close();

        // Send connection status
        let _ = self
            .message_sender
            .send(ControllerMessage::ConnectionChanged(false));

        info!("STM32 controller shutdown complete");
    }

    /// Check if connected
    pub fn is_connected(&self) -> bool {
        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.is_connected()
    }

    /// Set light level
    pub fn set_light_level(&mut self, level_index: u8) -> Stm32Result<()> {
        // Build the command first, then send it
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.set_light_level(level_index)?;
            let cmd = device_manager.build_write_command();
            info!(
                "Built write command for level {}: {:02X?}",
                level_index, cmd
            );
            cmd
        };

        // Send write command (fire and forget - no response expected)
        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        Ok(())
    }

    /// Toggle light level
    pub fn toggle_light_level(&mut self) -> Stm32Result<()> {
        // Build the command first, then send it
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.toggle_light_level()?;
            device_manager.build_write_command()
        };

        // Send write command (fire and forget - no response expected)
        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        Ok(())
    }

    /// Get current light level
    pub fn get_current_light_level(&self) -> u8 {
        let device_manager = self.device_manager.lock().unwrap();
        device_manager.get_current_light_level()
    }

    /// Get current brightness percentage
    pub fn get_current_brightness_percentage(&self) -> u8 {
        let device_manager = self.device_manager.lock().unwrap();
        device_manager.get_current_brightness_percentage()
    }

    /// Set lens lock status
    pub fn set_lens_locked(&mut self, locked: bool) -> Stm32Result<()> {
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.set_lens_locked(locked)?;
            device_manager.build_write_command()
        };

        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        Ok(())
    }

    /// Get lens lock status
    pub fn is_lens_locked(&self) -> bool {
        let device_manager = self.device_manager.lock().unwrap();
        device_manager.is_lens_locked()
    }

    /// Set lens speed
    pub fn set_lens_speed(&mut self, speed: u8) -> Stm32Result<()> {
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.set_lens_speed(speed)?;
            device_manager.build_write_command()
        };

        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        Ok(())
    }

    /// Get lens speed
    pub fn get_lens_speed(&self) -> u8 {
        let device_manager = self.device_manager.lock().unwrap();
        device_manager.get_lens_speed()
    }

    /// Set servo X angle
    pub fn set_servo_x_angle(&mut self, angle: f32) -> Stm32Result<()> {
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.set_servo_x_angle(angle)?;
            device_manager.build_write_command()
        };

        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        Ok(())
    }

    /// Set servo Y angle
    pub fn set_servo_y_angle(&mut self, angle: f32) -> Stm32Result<()> {
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.set_servo_y_angle(angle)?;
            device_manager.build_write_command()
        };

        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        Ok(())
    }

    /// Set both servo angles
    pub fn set_servo_angles(&mut self, x_angle: f32, y_angle: f32) -> Stm32Result<()> {
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.set_servo_angles(x_angle, y_angle)?;
            device_manager.build_write_command()
        };

        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        Ok(())
    }

    /// Get servo X angle
    pub fn get_servo_x_angle(&self) -> f32 {
        let device_manager = self.device_manager.lock().unwrap();
        device_manager.get_servo_x_angle()
    }

    /// Get servo Y angle
    pub fn get_servo_y_angle(&self) -> f32 {
        let device_manager = self.device_manager.lock().unwrap();
        device_manager.get_servo_y_angle()
    }

    /// Reset servo angles to default (90°)
    pub fn reset_servo_angles(&mut self) -> Stm32Result<()> {
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.reset_servo_angles()?;
            device_manager.build_write_command()
        };

        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        Ok(())
    }

    /// Set complete device configuration in one operation
    pub fn set_device_config(
        &mut self,
        light_level: u8,
        lens_locked: bool,
        lens_speed: u8,
        servo_x: f32,
        servo_y: f32,
    ) -> Stm32Result<()> {
        let command = {
            let mut device_manager = self.device_manager.lock().unwrap();
            device_manager.set_light_level(light_level)?;
            device_manager.set_lens_locked(lens_locked)?;
            device_manager.set_lens_speed(lens_speed)?;
            device_manager.set_servo_angles(servo_x, servo_y)?;
            device_manager.build_write_command()
        };

        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_only(&command)?;

        info!("Device configuration updated: light={}, lens_locked={}, lens_speed={}, servo=({:.1}°, {:.1}°)",
              light_level, lens_locked, lens_speed, servo_x, servo_y);

        Ok(())
    }

    /// Get current device configuration
    pub fn get_device_config(&self) -> DeviceConfig {
        let device_manager = self.device_manager.lock().unwrap();
        let params = device_manager.get_device_params();

        DeviceConfig {
            light_level: device_manager.get_current_light_level(),
            lens_locked: params.lens_locked,
            lens_speed: params.lens_speed,
            servo_x_angle: params.servo_x_angle,
            servo_y_angle: params.servo_y_angle,
        }
    }

    /// Reset all settings to defaults
    pub fn reset_to_defaults(&mut self) -> Stm32Result<()> {
        self.set_device_config(0, false, 0, 90.0, 90.0)
    }

    /// Perform servo calibration sequence
    pub fn calibrate_servos(&mut self) -> Stm32Result<()> {
        info!("Starting servo calibration sequence...");

        let calibration_positions = [
            (30.0, 30.0),   // Bottom-left corner
            (150.0, 30.0),  // Bottom-right corner
            (150.0, 150.0), // Top-right corner
            (30.0, 150.0),  // Top-left corner
            (90.0, 90.0),   // Center (default)
        ];

        for (i, (x, y)) in calibration_positions.iter().enumerate() {
            info!(
                "Calibration step {}: Moving to ({:.1}°, {:.1}°)",
                i + 1,
                x,
                y
            );
            self.set_servo_angles(*x, *y)?;
            std::thread::sleep(std::time::Duration::from_millis(1000));
        }

        info!("Servo calibration completed");
        Ok(())
    }

    /// Read device status
    pub fn read_device_status(&mut self) -> Stm32Result<DeviceStatus> {
        // Send read command
        let response = self.send_read_command()?;

        // Parse response
        let device_manager = self.device_manager.lock().unwrap();
        let status = device_manager.parse_response(&response)?;

        // Update device manager
        drop(device_manager);
        let mut device_manager = self.device_manager.lock().unwrap();
        device_manager.update_status(status.clone());

        // Send status update message
        let _ = self
            .message_sender
            .send(ControllerMessage::StatusUpdate(status.clone()));

        Ok(status)
    }

    /// Get last device status
    pub fn get_last_status(&self) -> DeviceStatus {
        let device_manager = self.device_manager.lock().unwrap();
        device_manager.get_last_status().clone()
    }

    /// Start periodic status updates
    pub fn start_periodic_updates(&mut self, interval_ms: u64) {
        if self.update_thread.is_some() {
            warn!("Periodic updates already running");
            return;
        }

        let hid_comm = Arc::clone(&self.hid_comm);
        let device_manager = Arc::clone(&self.device_manager);
        let message_sender = self.message_sender.clone();
        let shutdown_flag = Arc::clone(&self.shutdown_flag);

        let thread_handle = thread::spawn(move || {
            let interval_duration = Duration::from_millis(interval_ms);

            while !shutdown_flag.load(Ordering::SeqCst) {
                thread::sleep(interval_duration);

                // Read device status
                match Self::read_status_internal(&hid_comm, &device_manager) {
                    Ok(status) => {
                        let _ = message_sender.send(ControllerMessage::StatusUpdate(status));
                    }
                    Err(e) => {
                        let _ = message_sender.send(ControllerMessage::Error(e));
                    }
                }
            }
        });

        self.update_thread = Some(thread_handle);
        info!(
            "Started periodic status updates ({}ms interval)",
            interval_ms
        );
    }

    /// Stop periodic status updates
    pub fn stop_periodic_updates(&mut self) {
        if let Some(thread) = self.update_thread.take() {
            if let Err(e) = thread.join() {
                error!("Failed to join update thread: {:?}", e);
            }
        }
        info!("Stopped periodic status updates");
    }

    /// Try to receive a message
    pub fn try_receive_message(&self) -> Option<ControllerMessage> {
        self.message_receiver.try_recv().ok()
    }

    /// Receive a message (blocking)
    pub fn receive_message(&self) -> Result<ControllerMessage, crossbeam_channel::RecvError> {
        self.message_receiver.recv()
    }

    /// Get device information
    pub fn get_device_info(&self) -> DeviceInfo {
        let hid_comm = self.hid_comm.lock().unwrap();
        DeviceInfo {
            manufacturer: hid_comm.get_manufacturer().to_string(),
            product: hid_comm.get_product().to_string(),
            vendor_id: 0x0001, // Default values
            product_id: 0xEDD1,
            firmware_version: None,
            serial_number: None,
        }
    }

    // Private methods

    fn send_read_command(&self) -> Stm32Result<Vec<u8>> {
        let device_manager = self.device_manager.lock().unwrap();
        let command = device_manager.build_read_command();
        drop(device_manager);

        let hid_comm = self.hid_comm.lock().unwrap();
        hid_comm.send_receive(&command)
    }

    fn read_status_internal(
        hid_comm: &Arc<Mutex<HidCommunication>>,
        device_manager: &Arc<Mutex<DeviceManager>>,
    ) -> Stm32Result<DeviceStatus> {
        let dm = device_manager.lock().unwrap();
        let command = dm.build_read_command();
        drop(dm);

        let hc = hid_comm.lock().unwrap();
        let response = hc.send_receive(&command)?;
        drop(hc);

        let dm = device_manager.lock().unwrap();
        let status = dm.parse_response(&response)?;

        Ok(status)
    }
}

impl Drop for Stm32Controller {
    fn drop(&mut self) {
        self.shutdown();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_device_config_default() {
        let config = DeviceConfig::default();
        assert_eq!(config.light_level, 0);
        assert!(!config.lens_locked);
        assert_eq!(config.lens_speed, 0);
        assert_eq!(config.servo_x_angle, 90.0);
        assert_eq!(config.servo_y_angle, 90.0);
    }

    #[test]
    fn test_device_config_creation() {
        let config = DeviceConfig {
            light_level: 3,
            lens_locked: true,
            lens_speed: 2,
            servo_x_angle: 45.0,
            servo_y_angle: 135.0,
        };

        assert_eq!(config.light_level, 3);
        assert!(config.lens_locked);
        assert_eq!(config.lens_speed, 2);
        assert_eq!(config.servo_x_angle, 45.0);
        assert_eq!(config.servo_y_angle, 135.0);
    }

    #[test]
    fn test_controller_creation() {
        let controller = Stm32Controller::new(None);
        assert!(!controller.is_connected());
    }

    #[test]
    fn test_controller_message_types() {
        let status = DeviceStatus::default();
        let msg = ControllerMessage::StatusUpdate(status);

        match msg {
            ControllerMessage::StatusUpdate(_) => assert!(true),
            _ => assert!(false, "Expected StatusUpdate message"),
        }
    }
}
