//! Camera management module
//! 
//! This module provides high-level camera management functionality.

use crate::device::{V4L2Device, V4L2DeviceManager};
use crate::error::{CameraError, CameraResult};
use log::info;

/// Camera manager for discovering and managing USB cameras
pub struct CameraManager;

impl CameraManager {
    /// Create a new camera manager
    pub fn new() -> Self {
        Self
    }
    
    /// Discover all available USB cameras
    pub fn discover_cameras(&self) -> CameraResult<Vec<V4L2Device>> {
        info!("Discovering USB cameras...");
        
        let devices = V4L2DeviceManager::discover_devices()?;
        
        info!("Found {} camera(s)", devices.len());
        for device in &devices {
            info!("Camera: {} at {}", device.name, device.path.display());
        }
        
        Ok(devices)
    }
    
    /// Get camera by name
    pub fn get_camera_by_name(&self, name: &str) -> CameraResult<V4L2Device> {
        let devices = self.discover_cameras()?;
        
        for device in devices {
            if device.name == name {
                return Ok(device);
            }
        }
        
        Err(CameraError::DeviceNotFound(format!("Camera '{}' not found", name)))
    }
    
    /// Get camera by path
    pub fn get_camera_by_path(&self, path: &str) -> CameraResult<V4L2Device> {
        let devices = self.discover_cameras()?;
        
        for device in devices {
            if device.path.to_string_lossy() == path {
                return Ok(device);
            }
        }
        
        Err(CameraError::DeviceNotFound(format!("Camera at path '{}' not found", path)))
    }
    
    /// List all available cameras
    pub fn list_cameras(&self) -> CameraResult<()> {
        let devices = self.discover_cameras()?;
        
        if devices.is_empty() {
            println!("No USB cameras found");
            return Ok(());
        }
        
        println!("Found {} USB camera(s):", devices.len());
        println!();
        
        for (i, device) in devices.iter().enumerate() {
            println!("{}. {}", i + 1, device.name);
            println!("   Path: {}", device.path.display());
            println!("   Description: {}", device.description);
            if !device.video_paths.is_empty() {
                println!("   Video devices: {}", 
                    device.video_paths.iter()
                        .map(|p| p.to_string_lossy())
                        .collect::<Vec<_>>()
                        .join(", "));
            }
            if let Some(media_path) = &device.media_path {
                println!("   Media device: {}", media_path.display());
            }
            println!();
        }
        
        Ok(())
    }
}
