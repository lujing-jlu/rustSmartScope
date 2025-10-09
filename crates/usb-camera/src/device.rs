//! Device abstraction module
//!
//! This module provides device abstraction and management.

use crate::error::{CameraError, CameraResult};
use log::warn;
use serde::{Deserialize, Serialize};
use std::path::PathBuf;

/// V4L2 device information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct V4L2Device {
    /// Device name (e.g., "cameraR", "cameraL")
    pub name: String,

    /// Primary device path (first video device)
    pub path: PathBuf,

    /// All video device paths for this camera
    pub video_paths: Vec<PathBuf>,

    /// Media device path (if available)
    pub media_path: Option<PathBuf>,

    /// Device description
    pub description: String,
}

impl V4L2Device {
    /// Create a new V4L2 device
    pub fn new(name: String, path: PathBuf, description: String) -> Self {
        Self {
            name,
            path: path.clone(),
            video_paths: if path.as_os_str().is_empty() {
                vec![]
            } else {
                vec![path]
            },
            media_path: None,
            description,
        }
    }

    /// Add a video path to this device
    pub fn add_video_path(&mut self, path: PathBuf) {
        if !self.video_paths.contains(&path) {
            self.video_paths.push(path);
        }
    }

    /// Set media path for this device
    pub fn set_media_path(&mut self, path: PathBuf) {
        self.media_path = Some(path);
    }
}

/// V4L2 device manager
pub struct V4L2DeviceManager;

impl V4L2DeviceManager {
    /// Discover all V4L2 devices, excluding HDMI receivers
    pub fn discover_devices() -> CameraResult<Vec<V4L2Device>> {
        let output = std::process::Command::new("v4l2-ctl")
            .arg("--list-devices")
            .output()
            .map_err(|e| {
                CameraError::DeviceEnumerationFailed(format!("Failed to run v4l2-ctl: {}", e))
            })?;

        if !output.status.success() {
            return Err(CameraError::DeviceEnumerationFailed(
                String::from_utf8_lossy(&output.stderr).to_string(),
            ));
        }

        let output_str = String::from_utf8_lossy(&output.stdout);
        Self::parse_v4l2_output(&output_str)
    }

    /// Parse v4l2-ctl --list-devices output
    fn parse_v4l2_output(output: &str) -> CameraResult<Vec<V4L2Device>> {
        let mut devices = Vec::new();
        let mut current_device: Option<V4L2Device> = None;

        for line in output.lines() {
            let line = line.trim();

            // Skip empty lines
            if line.is_empty() {
                continue;
            }

            // Check if this is a device name line (ends with ':')
            if line.ends_with(':') {
                // Save previous device if exists
                if let Some(device) = current_device.take() {
                    // Skip HDMI receivers
                    if !device.name.to_lowercase().contains("hdmi")
                        && !device.name.to_lowercase().contains("rk_hdmirx")
                    {
                        // Only add device if it has at least one video path
                        if !device.video_paths.is_empty() {
                            devices.push(device);
                        } else {
                            warn!("Skipping device '{}' - no video paths found", device.name);
                        }
                    }
                }

                // Parse device name and description
                let device_info = line[..line.len() - 1].trim(); // Remove trailing ':'
                if let Some((name, description)) = Self::parse_device_info(device_info) {
                    // Skip HDMI receivers
                    if !name.to_lowercase().contains("hdmi")
                        && !name.to_lowercase().contains("rk_hdmirx")
                    {
                        current_device = Some(V4L2Device::new(
                            name.to_string(),
                            PathBuf::new(), // Will be set when we find video paths
                            description.to_string(),
                        ));
                    }
                }
            }
            // Check if this is a device path line (starts with '/')
            else if line.starts_with('/') && current_device.is_some() {
                let path = PathBuf::from(line);

                // Determine if this is a video device or media device
                if line.contains("/video") {
                    if let Some(device) = &mut current_device {
                        if device.path.as_os_str().is_empty() {
                            // This is the primary video device
                            device.path = path.clone();
                        }
                        device.add_video_path(path);
                    }
                } else if line.contains("/media") {
                    if let Some(device) = &mut current_device {
                        device.set_media_path(path);
                    }
                }
            }
        }

        // Don't forget the last device
        if let Some(device) = current_device {
            if !device.name.to_lowercase().contains("hdmi")
                && !device.name.to_lowercase().contains("rk_hdmirx")
            {
                // Only add device if it has at least one video path
                // This prevents adding devices that are not actually accessible
                if !device.video_paths.is_empty() {
                    devices.push(device);
                } else {
                    warn!("Skipping device '{}' - no video paths found", device.name);
                }
            }
        }

        Ok(devices)
    }

    /// Parse device info line (e.g., "cameraR: cameraR (usb-fc800000.usb-1.2.3)")
    fn parse_device_info(info: &str) -> Option<(&str, &str)> {
        if let Some(colon_pos) = info.find(':') {
            let name = info[..colon_pos].trim();
            let description = info[colon_pos + 1..].trim();
            Some((name, description))
        } else {
            // No colon found, treat entire string as name
            Some((info, ""))
        }
    }
}
