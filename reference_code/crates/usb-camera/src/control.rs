//! Camera parameter control module
//! 
//! This module provides V4L2 camera parameter control functionality.

use std::collections::HashMap;
use std::path::Path;
use serde::{Serialize, Deserialize};
use log::{info, warn, debug};
use crate::error::{CameraError, CameraResult};

/// Camera parameter types
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum CameraProperty {
    /// Brightness (亮度)
    Brightness,
    /// Contrast (对比度)
    Contrast,
    /// Saturation (饱和度)
    Saturation,
    /// Hue (色调)
    Hue,
    /// Gain (增益)
    Gain,
    /// Exposure (曝光)
    Exposure,
    /// Focus (焦距)
    Focus,
    /// White Balance (白平衡)
    WhiteBalance,
    /// Frame Rate (帧率)
    FrameRate,
    /// Resolution (分辨率)
    Resolution,
}

/// Parameter range information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParameterRange {
    /// Minimum value
    pub min: i32,
    /// Maximum value
    pub max: i32,
    /// Step size
    pub step: i32,
    /// Default value
    pub default: i32,
    /// Current value
    pub current: i32,
}

/// Camera parameter control
pub struct CameraControl {
    /// Device path
    device_path: String,
    /// Parameter ranges cache
    parameter_ranges: HashMap<CameraProperty, ParameterRange>,
}

impl CameraControl {
    /// Create a new camera control instance
    pub fn new(device_path: &str) -> Self {
        Self {
            device_path: device_path.to_string(),
            parameter_ranges: HashMap::new(),
        }
    }
    
    /// Get parameter range and current value
    pub fn get_parameter_range(&mut self, property: &CameraProperty) -> CameraResult<ParameterRange> {
        if let Some(range) = self.parameter_ranges.get(property) {
            return Ok(range.clone());
        }
        
        let v4l2_property = self.property_to_v4l2(property)?;
        let range = self.query_v4l2_parameter(&v4l2_property)?;
        
        self.parameter_ranges.insert(property.clone(), range.clone());
        Ok(range)
    }
    
    /// Set camera parameter
    pub fn set_parameter(&mut self, property: &CameraProperty, value: i32) -> CameraResult<()> {
        let v4l2_property = self.property_to_v4l2(property)?;
        
        // Check if value is within range
        let range = self.get_parameter_range(property)?;
        if value < range.min || value > range.max {
            return Err(CameraError::ConfigurationError(
                format!("Value {} out of range [{}, {}] for property {:?}", 
                        value, range.min, range.max, property)
            ));
        }
        
        // Set parameter using v4l2-ctl
        let output = std::process::Command::new("v4l2-ctl")
            .arg("--device")
            .arg(&self.device_path)
            .arg("--set-ctrl")
            .arg(format!("{}={}", v4l2_property, value))
            .output()
            .map_err(|e| CameraError::ConfigurationError(
                format!("Failed to run v4l2-ctl: {}", e)
            ))?;
        
        if !output.status.success() {
            let error_msg = String::from_utf8_lossy(&output.stderr);
            return Err(CameraError::ConfigurationError(
                format!("Failed to set parameter {}: {}", v4l2_property, error_msg)
            ));
        }
        
        // Update cached range
        if let Some(range) = self.parameter_ranges.get_mut(property) {
            range.current = value;
        }
        
        info!("Set {} to {} on device {}", v4l2_property, value, self.device_path);
        Ok(())
    }
    
    /// Get current parameter value
    pub fn get_parameter(&mut self, property: &CameraProperty) -> CameraResult<i32> {
        let range = self.get_parameter_range(property)?;
        Ok(range.current)
    }
    
    /// Get all available parameters
    pub fn get_all_parameters(&mut self) -> CameraResult<HashMap<CameraProperty, ParameterRange>> {
        let properties = vec![
            CameraProperty::Brightness,
            CameraProperty::Contrast,
            CameraProperty::Saturation,
            CameraProperty::Hue,
            CameraProperty::Gain,
            CameraProperty::Exposure,
            CameraProperty::Focus,
            CameraProperty::WhiteBalance,
        ];
        
        let mut result = HashMap::new();
        for property in properties {
            match self.get_parameter_range(&property) {
                Ok(range) => {
                    result.insert(property, range);
                }
                Err(e) => {
                    debug!("Failed to get range for {:?}: {}", property, e);
                }
            }
        }
        
        Ok(result)
    }
    
    /// Reset all parameters to default values
    pub fn reset_to_defaults(&mut self) -> CameraResult<()> {
        let parameters = self.get_all_parameters()?;
        
        for (property, range) in parameters {
            if range.current != range.default {
                self.set_parameter(&property, range.default)?;
            }
        }
        
        info!("Reset all parameters to defaults on device {}", self.device_path);
        Ok(())
    }
    
    /// Convert our property enum to V4L2 control name
    fn property_to_v4l2(&self, property: &CameraProperty) -> CameraResult<&'static str> {
        match property {
            CameraProperty::Brightness => Ok("brightness"),
            CameraProperty::Contrast => Ok("contrast"),
            CameraProperty::Saturation => Ok("saturation"),
            CameraProperty::Hue => Ok("hue"),
            CameraProperty::Gain => Ok("gain"),
            CameraProperty::Exposure => Ok("exposure"),
            CameraProperty::Focus => Ok("focus"),
            CameraProperty::WhiteBalance => Ok("white_balance"),
            CameraProperty::FrameRate => Ok("frame_rate"),
            CameraProperty::Resolution => Err(CameraError::ConfigurationError(
                "Resolution is not a V4L2 control parameter".to_string()
            )),
        }
    }
    
    /// Query V4L2 parameter range and current value
    fn query_v4l2_parameter(&self, v4l2_property: &str) -> CameraResult<ParameterRange> {
        let output = std::process::Command::new("v4l2-ctl")
            .arg("--device")
            .arg(&self.device_path)
            .arg("--get-ctrl")
            .arg(v4l2_property)
            .output()
            .map_err(|e| CameraError::ConfigurationError(
                format!("Failed to run v4l2-ctl: {}", e)
            ))?;
        
        if !output.status.success() {
            return Err(CameraError::ConfigurationError(
                format!("Parameter {} not supported on device {}", v4l2_property, self.device_path)
            ));
        }
        
        let output_str = String::from_utf8_lossy(&output.stdout);
        self.parse_v4l2_parameter_output(&output_str, v4l2_property)
    }
    
    /// Parse v4l2-ctl output to extract parameter range
    fn parse_v4l2_parameter_output(&self, output: &str, _property: &str) -> CameraResult<ParameterRange> {
        // Expected format: "brightness: 0" or "brightness: 0 (int) min=0 max=255 step=1 default=128 value=128"
        let output = output.trim();
        
        // Simple format: "brightness: 0"
        if let Some(colon_pos) = output.find(':') {
            let value_str = output[colon_pos + 1..].trim();
            if let Ok(current) = value_str.parse::<i32>() {
                // For simple format, use default ranges
                return Ok(ParameterRange {
                    min: 0,
                    max: 255,
                    step: 1,
                    default: current,
                    current,
                });
            }
        }
        
        // Extended format: "brightness: 0 (int) min=0 max=255 step=1 default=128 value=128"
        let parts: Vec<&str> = output.split_whitespace().collect();
        
        if parts.len() < 2 {
            return Err(CameraError::ConfigurationError(
                format!("Unexpected v4l2-ctl output format: {}", output)
            ));
        }
        
        // Extract current value
        let current = parts[1].parse::<i32>()
            .map_err(|_| CameraError::ConfigurationError(
                format!("Failed to parse current value from: {}", parts[1])
            ))?;
        
        // Extract min, max, step, default from the rest
        let mut min = 0;
        let mut max = 255;
        let mut step = 1;
        let mut default = current;
        
        for part in &parts[2..] {
            if part.starts_with("min=") {
                min = part[4..].parse::<i32>()
                    .map_err(|_| CameraError::ConfigurationError(
                        format!("Failed to parse min value from: {}", part)
                    ))?;
            } else if part.starts_with("max=") {
                max = part[4..].parse::<i32>()
                    .map_err(|_| CameraError::ConfigurationError(
                        format!("Failed to parse max value from: {}", part)
                    ))?;
            } else if part.starts_with("step=") {
                step = part[5..].parse::<i32>()
                    .map_err(|_| CameraError::ConfigurationError(
                        format!("Failed to parse step value from: {}", part)
                    ))?;
            } else if part.starts_with("default=") {
                default = part[8..].parse::<i32>()
                    .map_err(|_| CameraError::ConfigurationError(
                        format!("Failed to parse default value from: {}", part)
                    ))?;
            }
        }
        
        Ok(ParameterRange {
            min,
            max,
            step,
            default,
            current,
        })
    }
}

/// Camera parameter configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraParameters {
    /// Auto exposure enabled
    pub auto_exposure: bool,
    /// Exposure time in microseconds
    pub exposure_time: u32,
    /// Gain value
    pub gain: f32,
    /// Auto white balance enabled
    pub auto_white_balance: bool,
    /// Brightness value
    pub brightness: i32,
    /// Contrast value
    pub contrast: i32,
    /// Saturation value
    pub saturation: i32,
}

impl Default for CameraParameters {
    fn default() -> Self {
        Self {
            auto_exposure: true,
            exposure_time: 10000,
            gain: 1.0,
            auto_white_balance: true,
            brightness: 0,
            contrast: 0,
            saturation: 0,
        }
    }
}

impl CameraParameters {
    /// Apply parameters to camera
    pub fn apply_to_camera(&self, control: &mut CameraControl) -> CameraResult<()> {
        // Set brightness
        if let Err(e) = control.set_parameter(&CameraProperty::Brightness, self.brightness) {
            warn!("Failed to set brightness: {}", e);
        }
        
        // Set contrast
        if let Err(e) = control.set_parameter(&CameraProperty::Contrast, self.contrast) {
            warn!("Failed to set contrast: {}", e);
        }
        
        // Set saturation
        if let Err(e) = control.set_parameter(&CameraProperty::Saturation, self.saturation) {
            warn!("Failed to set saturation: {}", e);
        }
        
        // Set gain if not auto exposure
        if !self.auto_exposure {
            if let Err(e) = control.set_parameter(&CameraProperty::Gain, (self.gain * 100.0) as i32) {
                warn!("Failed to set gain: {}", e);
            }
        }
        
        // Set exposure if not auto exposure
        if !self.auto_exposure {
            if let Err(e) = control.set_parameter(&CameraProperty::Exposure, self.exposure_time as i32) {
                warn!("Failed to set exposure: {}", e);
            }
        }
        
        info!("Applied camera parameters to device");
        Ok(())
    }
    
    /// Load parameters from file
    pub fn load_from_file<P: AsRef<Path>>(path: P) -> CameraResult<Self> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| CameraError::ConfigurationError(
                format!("Failed to read parameter file: {}", e)
            ))?;
        
        toml::from_str(&content)
            .map_err(|e| CameraError::ConfigurationError(
                format!("Failed to parse parameter file: {}", e)
            ))
    }
    
    /// Save parameters to file
    pub fn save_to_file<P: AsRef<Path>>(&self, path: P) -> CameraResult<()> {
        let content = toml::to_string_pretty(self)
            .map_err(|e| CameraError::ConfigurationError(
                format!("Failed to serialize parameters: {}", e)
            ))?;
        
        std::fs::write(path, content)
            .map_err(|e| CameraError::ConfigurationError(
                format!("Failed to write parameter file: {}", e)
            ))?;
        
        Ok(())
    }
}
