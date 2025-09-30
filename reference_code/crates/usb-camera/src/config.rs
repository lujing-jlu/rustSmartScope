//! Configuration management module
//! 
//! This module provides configuration management for USB cameras.

use serde::{Serialize, Deserialize};
use std::collections::HashMap;

/// Camera configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraConfig {
    /// Resolution width
    pub width: u32,
    
    /// Resolution height
    pub height: u32,
    
    /// Frame rate (fps)
    pub framerate: u32,
    
    /// Pixel format (e.g., "YUYV", "MJPG")
    pub pixel_format: String,
    
    /// Additional configuration parameters
    pub parameters: HashMap<String, String>,
}

impl Default for CameraConfig {
    fn default() -> Self {
        Self {
            width: 1920,
            height: 1080,
            framerate: 30,
            pixel_format: "YUYV".to_string(),
            parameters: HashMap::new(),
        }
    }
}

impl CameraConfig {
    /// Create a new camera configuration
    pub fn new() -> Self {
        Self::default()
    }
    
    /// Set resolution
    pub fn with_resolution(mut self, width: u32, height: u32) -> Self {
        self.width = width;
        self.height = height;
        self
    }
    
    /// Set frame rate
    pub fn with_framerate(mut self, framerate: u32) -> Self {
        self.framerate = framerate;
        self
    }
    
    /// Set pixel format
    pub fn with_pixel_format(mut self, format: &str) -> Self {
        self.pixel_format = format.to_string();
        self
    }
    
    /// Add a custom parameter
    pub fn with_parameter(mut self, key: &str, value: &str) -> Self {
        self.parameters.insert(key.to_string(), value.to_string());
        self
    }
    
    /// Get a parameter value
    pub fn get_parameter(&self, key: &str) -> Option<&String> {
        self.parameters.get(key)
    }
}
