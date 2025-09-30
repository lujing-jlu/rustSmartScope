//! HID communication implementation

use crate::error::{Stm32Error, Stm32Result};
use hidapi::{HidApi, HidDevice};
use log::{debug, error, info};
use std::time::Duration;

/// HID communication configuration
#[derive(Debug, Clone)]
pub struct HidConfig {
    pub vendor_id: u16,
    pub product_id: u16,
    pub usage_page: u16,
    pub usage: u16,
    pub report_size: usize,
    pub timeout_ms: u32,
}

impl Default for HidConfig {
    fn default() -> Self {
        Self {
            vendor_id: 0x0001,
            product_id: 0xEDD1,
            usage_page: 0xFF60,
            usage: 0x61,
            report_size: 33,
            timeout_ms: 1000,
        }
    }
}

/// HID communication handler
pub struct HidCommunication {
    config: HidConfig,
    device: Option<HidDevice>,
    manufacturer: String,
    product: String,
}

impl HidCommunication {
    /// Create a new HID communication instance
    pub fn new(config: HidConfig) -> Self {
        Self {
            config,
            device: None,
            manufacturer: String::new(),
            product: String::new(),
        }
    }

    /// Create with default configuration
    pub fn new_default() -> Self {
        Self::new(HidConfig::default())
    }

    /// Open HID device
    pub fn open(&mut self) -> Stm32Result<()> {
        if self.device.is_some() {
            return Ok(());
        }

        let api = HidApi::new()
            .map_err(|e| Stm32Error::HidApiError(format!("Failed to initialize HID API: {}", e)))?;

        // Enumerate devices
        let devices = api.device_list();
        let mut target_device = None;

        for device_info in devices {
            if device_info.vendor_id() == self.config.vendor_id
                && device_info.product_id() == self.config.product_id
            {
                debug!(
                    "Found matching device: {:04X}:{:04X}",
                    device_info.vendor_id(),
                    device_info.product_id()
                );

                // Check usage page and usage if specified
                if self.config.usage_page != 0 || self.config.usage != 0 {
                    let usage_page = device_info.usage_page();
                    let usage = device_info.usage();
                    if usage_page == self.config.usage_page && usage == self.config.usage {
                        target_device = Some(device_info);
                        break;
                    }
                } else {
                    target_device = Some(device_info);
                    break;
                }
            }
        }

        if let Some(device_info) = target_device {
            let device = device_info
                .open_device(&api)
                .map_err(|e| Stm32Error::HidApiError(format!("Failed to open device: {}", e)))?;

            // Get device strings
            self.manufacturer = device_info
                .manufacturer_string()
                .unwrap_or("Unknown")
                .to_string();
            self.product = device_info
                .product_string()
                .unwrap_or("Unknown")
                .to_string();

            self.device = Some(device);

            info!(
                "HID device connected: {} - {}",
                self.manufacturer, self.product
            );
            Ok(())
        } else {
            Err(Stm32Error::DeviceNotFound)
        }
    }

    /// Close HID device
    pub fn close(&mut self) {
        self.device = None;
        info!("HID device disconnected");
    }

    /// Check if device is connected
    pub fn is_connected(&self) -> bool {
        self.device.is_some()
    }

    /// Send data only (no response expected)
    pub fn send_only(&self, data: &[u8]) -> Stm32Result<()> {
        let device = self.device.as_ref().ok_or(Stm32Error::DeviceNotConnected)?;

        // Prepare report buffer with Report ID prefix (0x00)
        let mut report = vec![0u8; self.config.report_size];
        report[0] = 0x00; // Report ID
        let payload_space = self.config.report_size.saturating_sub(1);
        let copy_len = data.len().min(payload_space);
        report[1..1 + copy_len].copy_from_slice(&data[..copy_len]);

        info!("=== SENDING HID REPORT ===");
        info!("Payload (32B): {:02X?}", data);
        info!("Full Report (33B): {:02X?}", report);
        info!("Report length: {}", report.len());

        // Send data only
        let write_result = device.write(&report);
        match write_result {
            Ok(bytes_written) => {
                info!("Write successful: {} bytes sent", bytes_written);
                Ok(())
            }
            Err(e) => {
                error!("Write failed: {}", e);
                Err(Stm32Error::HidApiError(format!("Write failed: {}", e)))
            }
        }
    }

    /// Send data and receive response
    pub fn send_receive(&self, data: &[u8]) -> Stm32Result<Vec<u8>> {
        let device = self.device.as_ref().ok_or(Stm32Error::DeviceNotConnected)?;

        // Prepare report buffer with Report ID prefix (0x00)
        // HIDAPI expects the first byte to be report ID when writing
        let mut report = vec![0u8; self.config.report_size];
        report[0] = 0x00; // Report ID
        let payload_space = self.config.report_size.saturating_sub(1);
        let copy_len = data.len().min(payload_space);
        report[1..1 + copy_len].copy_from_slice(&data[..copy_len]);

        debug!("Sending report (len={}): {:02X?}", report.len(), report);

        // Send data
        let write_result = device.write(&report);
        if let Err(e) = write_result {
            return Err(Stm32Error::HidApiError(format!("Write failed: {}", e)));
        }

        // Receive response
        let mut response = vec![0u8; self.config.report_size];
        let timeout = Duration::from_millis(self.config.timeout_ms as u64);

        let read_result = device.read_timeout(&mut response, timeout.as_millis() as i32);
        match read_result {
            Ok(bytes_read) => {
                response.truncate(bytes_read);
                info!("=== RECEIVED HID RESPONSE ===");
                info!("Raw response ({} bytes): {:02X?}", bytes_read, response);

                // Some platforms include Report ID (0x00) as the first byte.
                // If present, strip it so the caller always sees the 32-byte payload starting with 0x55 0xAA.
                let payload: Vec<u8> = if !response.is_empty() && response[0] == 0x00 {
                    response.into_iter().skip(1).collect()
                } else {
                    response
                };

                info!(
                    "Normalized payload ({} bytes): {:02X?}",
                    payload.len(),
                    payload
                );
                Ok(payload)
            }
            Err(e) => {
                if e.to_string().contains("timeout") {
                    Err(Stm32Error::Timeout)
                } else {
                    Err(Stm32Error::HidApiError(format!("Read failed: {}", e)))
                }
            }
        }
    }

    /// Get manufacturer string
    pub fn get_manufacturer(&self) -> &str {
        &self.manufacturer
    }

    /// Get product string
    pub fn get_product(&self) -> &str {
        &self.product
    }

    /// Set timeout
    pub fn set_timeout(&mut self, timeout_ms: u32) {
        self.config.timeout_ms = timeout_ms;
    }
}

impl Drop for HidCommunication {
    fn drop(&mut self) {
        self.close();
    }
}
