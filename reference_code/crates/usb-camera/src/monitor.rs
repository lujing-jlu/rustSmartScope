//! Camera status monitoring module
//! 
//! This module provides continuous camera status monitoring and mode detection.

use std::collections::HashSet;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;
use crossbeam_channel::{Receiver, Sender, unbounded};
use serde::{Serialize, Deserialize};
use log::{info, warn, debug};
use crate::device::{V4L2Device, V4L2DeviceManager};
use crate::error::{CameraError, CameraResult};

/// Camera operation mode
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum CameraMode {
    /// No cameras connected
    NoCamera,
    /// Single camera mode (only one camera connected)
    SingleCamera,
    /// Stereo camera mode (both cameraL and cameraR connected)
    StereoCamera,
}

/// Camera status information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraStatus {
    /// Current camera mode
    pub mode: CameraMode,
    /// List of connected cameras
    pub cameras: Vec<V4L2Device>,
    /// Timestamp of last update
    pub timestamp: std::time::SystemTime,
}

/// Status monitoring message
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum StatusMessage {
    /// Status update
    StatusUpdate(CameraStatus),
    /// Error occurred during monitoring
    Error(String),
}

/// Camera status monitor for continuous monitoring
pub struct CameraStatusMonitor {
    /// Sender for sending messages to the monitor thread
    sender: Sender<StatusMessage>,
    /// Receiver for receiving messages from the monitor thread
    receiver: Receiver<StatusMessage>,
    /// Monitor thread handle
    thread_handle: Option<thread::JoinHandle<()>>,
    /// Monitoring interval in milliseconds
    interval_ms: u64,
    /// Shutdown flag
    shutdown_flag: Arc<AtomicBool>,
}

impl CameraStatusMonitor {
    /// Create a new camera status monitor
    pub fn new(interval_ms: u64) -> Self {
        let (sender, receiver) = unbounded();
        Self {
            sender,
            receiver,
            thread_handle: None,
            interval_ms,
            shutdown_flag: Arc::new(AtomicBool::new(false)),
        }
    }
    
    /// Start status monitoring in a separate thread
    pub fn start(&mut self) -> CameraResult<()> {
        if self.thread_handle.is_some() {
            return Err(CameraError::ConfigurationError("Status monitor already started".to_string()));
        }
        
        let sender = self.sender.clone();
        let interval_ms = self.interval_ms;
        let shutdown_flag = self.shutdown_flag.clone();
        
        let handle = thread::spawn(move || {
            Self::monitor_loop(sender, interval_ms, shutdown_flag);
        });
        
        self.thread_handle = Some(handle);
        info!("Camera status monitor started with {}ms interval", interval_ms);
        
        // Send initial status immediately
        if let Ok(status) = Self::get_current_status() {
            if let Err(e) = self.sender.send(StatusMessage::StatusUpdate(status)) {
                warn!("Failed to send initial status: {}", e);
            } else {
                info!("Initial camera status sent");
            }
        }
        
        Ok(())
    }
    
    /// Stop status monitoring
    pub fn stop(&mut self) -> CameraResult<()> {
        if let Some(handle) = self.thread_handle.take() {
            // Set shutdown flag
            self.shutdown_flag.store(true, Ordering::Relaxed);
            
            // Wait for thread to finish
            if let Err(e) = handle.join() {
                warn!("Status monitor thread panicked: {:?}", e);
            }
            
            info!("Camera status monitor stopped");
        }
        
        Ok(())
    }
    
    /// Get the latest status (non-blocking)
    pub fn try_get_status(&self) -> Option<CameraStatus> {
        if let Ok(StatusMessage::StatusUpdate(status)) = self.receiver.try_recv() {
            Some(status)
        } else {
            None
        }
    }
    
    /// Get the latest status (blocking)
    pub fn get_status(&self) -> CameraResult<CameraStatus> {
        match self.receiver.recv() {
            Ok(StatusMessage::StatusUpdate(status)) => Ok(status),
            Ok(StatusMessage::Error(error)) => Err(CameraError::DeviceEnumerationFailed(error)),
            Err(e) => Err(CameraError::DeviceEnumerationFailed(format!("Channel error: {}", e))),
        }
    }
    
    /// Get current camera status (synchronous, non-blocking)
    pub fn get_current_status() -> CameraResult<CameraStatus> {
        let devices = V4L2DeviceManager::discover_devices()?;
        let camera_names: HashSet<String> = devices.iter()
            .map(|d| d.name.clone())
            .collect();
        
        let mode = Self::determine_camera_mode(&camera_names);
        
        Ok(CameraStatus {
            mode,
            cameras: devices,
            timestamp: std::time::SystemTime::now(),
        })
    }
    
    /// Check if status monitor is running
    pub fn is_running(&self) -> bool {
        self.thread_handle.is_some()
    }
    
    /// Monitor loop running in separate thread
    fn monitor_loop(sender: Sender<StatusMessage>, interval_ms: u64, shutdown_flag: Arc<AtomicBool>) {
        let mut last_cameras = HashSet::new();
        
        loop {
            // Check shutdown flag
            if shutdown_flag.load(Ordering::Relaxed) {
                debug!("Status monitor thread received shutdown signal");
                break;
            }
            
            // Discover cameras
            match V4L2DeviceManager::discover_devices() {
                Ok(devices) => {
                    // Create set of current camera names for comparison
                    let current_cameras: HashSet<String> = devices.iter()
                        .map(|d| d.name.clone())
                        .collect();
                    
                    // Check if camera list has changed
                    if current_cameras != last_cameras {
                        debug!("Camera list changed: {:?} -> {:?}", last_cameras, current_cameras);
                        
                        // Determine camera mode
                        let mode = Self::determine_camera_mode(&current_cameras);
                        
                        // Create status update
                        let status = CameraStatus {
                            mode,
                            cameras: devices,
                            timestamp: std::time::SystemTime::now(),
                        };
                        
                        // Send status update
                        if let Err(e) = sender.send(StatusMessage::StatusUpdate(status)) {
                            warn!("Failed to send status update: {}", e);
                            break;
                        }
                        
                        last_cameras = current_cameras;
                    }
                }
                Err(e) => {
                    warn!("Failed to discover devices: {}", e);
                    if let Err(e) = sender.send(StatusMessage::Error(e.to_string())) {
                        warn!("Failed to send error message: {}", e);
                        break;
                    }
                }
            }
            
            // Sleep for the specified interval, checking shutdown flag periodically
            for _ in 0..interval_ms {
                if shutdown_flag.load(Ordering::Relaxed) {
                    return;
                }
                thread::sleep(Duration::from_millis(1));
            }
        }
    }
    
    /// Determine camera mode based on connected cameras
    fn determine_camera_mode(cameras: &HashSet<String>) -> CameraMode {
        if cameras.is_empty() {
            CameraMode::NoCamera
        } else if cameras.contains("cameraL") && cameras.contains("cameraR") {
            CameraMode::StereoCamera
        } else {
            CameraMode::SingleCamera
        }
    }
}

impl Drop for CameraStatusMonitor {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}

/// Camera status detector (simplified version without threading)
pub struct CameraStatusDetector;

impl CameraStatusDetector {
    /// Detect current camera mode
    pub fn detect_mode() -> CameraResult<CameraMode> {
        let devices = V4L2DeviceManager::discover_devices()?;
        let camera_names: HashSet<String> = devices.iter()
            .map(|d| d.name.clone())
            .collect();
        
        Ok(Self::determine_camera_mode(&camera_names))
    }
    
    /// Get detailed camera status
    pub fn get_status() -> CameraResult<CameraStatus> {
        let devices = V4L2DeviceManager::discover_devices()?;
        let camera_names: HashSet<String> = devices.iter()
            .map(|d| d.name.clone())
            .collect();
        
        let mode = Self::determine_camera_mode(&camera_names);
        
        Ok(CameraStatus {
            mode,
            cameras: devices,
            timestamp: std::time::SystemTime::now(),
        })
    }
    
    /// Determine camera mode based on connected cameras
    fn determine_camera_mode(cameras: &HashSet<String>) -> CameraMode {
        if cameras.is_empty() {
            CameraMode::NoCamera
        } else if cameras.contains("cameraL") && cameras.contains("cameraR") {
            CameraMode::StereoCamera
        } else {
            CameraMode::SingleCamera
        }
    }
}
