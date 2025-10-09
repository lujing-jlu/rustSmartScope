//! C++ Interface for Multi-threaded USB Camera Stream Management
//!
//! This module provides a C-compatible interface for the USB camera stream management
//! system. It uses independent threads for each camera to maximize FPS performance,
//! automatically handles camera mode detection, and provides data pipes for C++ integration.

use std::collections::HashMap;
use std::mem::ManuallyDrop;
use std::os::raw::{c_char, c_int, c_uint, c_void};
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use log::{debug, error, info, warn};

use crate::config::CameraConfig;
use crate::decoder::{DecodePriority, DecodedFormat, DecoderConfig, DecoderManager};
use crate::device::V4L2Device;
use crate::error::CameraResult;
use crate::monitor::{CameraMode, CameraStatusMonitor};
use crate::stream::{CameraStreamReader, VideoFrame};

/// Error codes for C++ interface
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CameraStreamError {
    Success = 0,
    InvalidInstance = -1,
    InitializationFailed = -2,
    DeviceNotFound = -3,
    StreamStartFailed = -4,
    StreamStopFailed = -5,
    NoFrameAvailable = -6,
    PipeWriteFailed = -7,
    InvalidParameter = -8,
}

/// Camera types for stereo mode
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CCameraType {
    Unknown = -1,
    Left = 0,
    Right = 1,
    Single = 2,
}

/// Frame status codes
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CFrameStatus {
    Ok = 0,
    Dropped = 1,
    Corrupted = 2,
    Timeout = 3,
}

/// Image format types (fourcc codes)
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CImageFormat {
    Yuyv = 0x56595559,
    Mjpg = 0x47504A4D,
    Rgb24 = 0x32424752,
    Unknown = 0,
}

/// Single frame metadata
#[repr(C)]
#[derive(Debug)]
pub struct CFrameMetadata {
    /// Raw frame data pointer
    pub data: *const u8,
    /// Data size
    pub size: c_uint,
    /// Frame width
    pub width: c_uint,
    /// Frame height
    pub height: c_uint,
    /// Pixel format
    pub format: CImageFormat,
    /// Frame ID
    pub frame_id: u64,
    /// Camera type
    pub camera_type: CCameraType,
    /// Frame status
    pub status: CFrameStatus,
    /// Timestamp (milliseconds since epoch)
    pub timestamp_ms: u64,
    /// Frame sequence number
    pub sequence_number: u64,
    /// Exposure time in microseconds
    pub exposure_us: c_uint,
    /// Gain value
    pub gain: c_uint,
    /// Frame processing latency
    pub latency_us: c_uint,
}

/// Camera status information
#[repr(C)]
#[derive(Debug)]
pub struct CCameraStatus {
    /// Camera name
    pub name: [c_char; 64],
    /// Device path
    pub device_path: [c_char; 256],
    /// Camera type
    pub camera_type: CCameraType,
    /// Is connected
    pub connected: c_int,
    /// Current FPS
    pub fps: f32,
    /// Total frames
    pub total_frames: u64,
    /// Dropped frames
    pub dropped_frames: u64,
    /// Temperature
    pub temperature: f32,
}

/// No camera data
#[repr(C)]
#[derive(Debug)]
pub struct CNoCameraData {
    /// Current mode
    pub mode: c_int, // CameraMode
    /// Timestamp
    pub timestamp_ms: u64,
    /// Detection attempts
    pub detection_attempts: c_uint,
    /// Error message
    pub error_message: [c_char; 256],
}

/// Single camera data
#[repr(C)]
#[derive(Debug)]
pub struct CSingleCameraData {
    /// Current mode
    pub mode: c_int, // CameraMode
    /// Timestamp
    pub timestamp_ms: u64,
    /// Camera status
    pub camera_status: CCameraStatus,
    /// Frame metadata
    pub frame: CFrameMetadata,
    /// System load
    pub system_load: f32,
}

/// Stereo camera data
#[repr(C)]
#[derive(Debug)]
pub struct CStereoCameraData {
    /// Current mode
    pub mode: c_int, // CameraMode
    /// Timestamp
    pub timestamp_ms: u64,
    /// Left camera status
    pub left_camera_status: CCameraStatus,
    /// Right camera status
    pub right_camera_status: CCameraStatus,
    /// Left frame
    pub left_frame: CFrameMetadata,
    /// Right frame
    pub right_frame: CFrameMetadata,
    /// Sync delta
    pub sync_delta_us: i32,
    /// Baseline distance
    pub baseline_mm: f32,
    /// System load
    pub system_load: f32,
}

/// Unified camera data
#[repr(C)]
pub union CCameraDataUnion {
    pub no_camera: ManuallyDrop<CNoCameraData>,
    pub single_camera: ManuallyDrop<CSingleCameraData>,
    pub stereo_camera: ManuallyDrop<CStereoCameraData>,
}

#[repr(C)]
pub struct CCameraData {
    /// Current mode
    pub mode: c_int, // CameraMode
    /// Data union
    pub data: CCameraDataUnion,
}

/// Unified camera data callback function type
pub type CameraDataCallback =
    extern "C" fn(camera_data: *const CCameraData, user_data: *mut std::os::raw::c_void);

// Mark the callback and user data as Send - this is safe because
// the C++ code is responsible for thread safety
unsafe impl Send for CameraThread {}
unsafe impl Sync for CameraThread {}

/// Camera thread controller for C++ interface
struct CameraThread {
    camera_name: String,
    thread_handle: Option<JoinHandle<()>>,
    shutdown_signal: Arc<AtomicBool>,
    // Only support unified callback
    data_callback: Option<CameraDataCallback>,
    user_data: *mut std::os::raw::c_void,
    camera_type: CCameraType,
    start_time: Instant,
    _frame_count: Arc<AtomicBool>,
}

impl CameraThread {
    /// Create and start a new camera thread with unified callback
    fn new_with_unified_callback(
        device: &V4L2Device,
        config: &CameraConfig,
        camera_type: CCameraType,
        callback: CameraDataCallback,
        user_data: *mut std::os::raw::c_void,
    ) -> Result<Self, Box<dyn std::error::Error>> {
        let camera_name = device.name.clone();
        let device_path = device.path.to_string_lossy().to_string();
        let config = config.clone();

        // Create shutdown signal
        let shutdown_signal = Arc::new(AtomicBool::new(false));
        let shutdown_clone = shutdown_signal.clone();
        let start_time = Instant::now();
        let frame_count = Arc::new(AtomicBool::new(false));
        let frame_count_clone = frame_count.clone();

        // Spawn the camera thread with unified callback
        let user_data_usize = user_data as usize;
        let camera_name_clone = camera_name.clone();
        let handle = thread::spawn(move || {
            let user_data_ptr = user_data_usize as *mut std::os::raw::c_void;
            Self::unified_camera_thread_loop(
                camera_name_clone,
                device_path,
                config,
                camera_type,
                callback,
                user_data_ptr,
                shutdown_clone,
                frame_count_clone,
            );
        });

        info!(
            "[{}] Camera thread started with unified callback",
            camera_name
        );

        Ok(Self {
            camera_name,
            thread_handle: Some(handle),
            shutdown_signal,
            data_callback: Some(callback),
            user_data,
            camera_type,
            start_time,
            _frame_count: frame_count,
        })
    }
    /// Unified camera thread main loop with data callback
    fn unified_camera_thread_loop(
        camera_name: String,
        device_path: String,
        config: CameraConfig,
        camera_type: CCameraType,
        callback: CameraDataCallback,
        user_data: *mut std::os::raw::c_void,
        shutdown_signal: Arc<AtomicBool>,
        _frame_count: Arc<AtomicBool>,
    ) {
        let thread_id = format!("{:?}", thread::current().id());
        info!(
            "[{}] Thread {} started with unified callback",
            camera_name, thread_id
        );

        // Create camera stream reader
        let mut reader = CameraStreamReader::new(&device_path, &camera_name, config);

        match reader.start() {
            Ok(_) => {
                info!("[{}] Stream started successfully", camera_name);
            }
            Err(e) => {
                error!("[{}] Failed to start stream: {}", camera_name, e);
                // Send error data
                Self::send_error_data(&camera_name, &e.to_string(), callback, user_data);
                return;
            }
        }

        // Create decoder manager
        let decoder_config = DecoderConfig {
            enabled: true,
            max_queue_size: 10,
            target_format: DecodedFormat::BGRA8888,
            max_resolution: (1920, 1080),
            enable_caching: false,
            cache_size: 0,
            priority: DecodePriority::Normal,
        };
        let mut decoder = DecoderManager::new(decoder_config);
        if let Err(e) = decoder.start() {
            error!("[{}] Failed to start decoder: {}", camera_name, e);
            Self::send_error_data(&camera_name, &e.to_string(), callback, user_data);
            return;
        }
        info!("[{}] Decoder started successfully", camera_name);

        let mut frame_counter = 0u64;
        let mut last_fps_time = Instant::now();
        let mut fps = 0.0f32;

        // Main reading loop
        while !shutdown_signal.load(Ordering::Relaxed) {
            // Try to get decoded frames first
            if let Some(result) = decoder.try_get_result() {
                match result {
                    crate::decoder::DecodeResult::Success(decoded_frame) => {
                        frame_counter += 1;

                        // Calculate FPS every 30 frames
                        if frame_counter % 30 == 0 {
                            let elapsed = last_fps_time.elapsed();
                            fps = 30.0 / elapsed.as_secs_f32();
                            last_fps_time = Instant::now();
                        }

                        // Create and send unified data with decoded frame
                        Self::send_decoded_frame_data(
                            &camera_name,
                            &decoded_frame,
                            camera_type,
                            frame_counter,
                            fps,
                            callback,
                            user_data,
                        );

                        // Log every N frames
                        if frame_counter <= 5 || frame_counter % 100 == 0 {
                            debug!(
                                "[{}] Decoded Frame {}: {}x{}, {} KB, ID: {}, FPS: {:.1} (Thread: {})",
                                camera_name,
                                frame_counter,
                                decoded_frame.width,
                                decoded_frame.height,
                                decoded_frame.data.len() / 1024,
                                decoded_frame.frame_id,
                                fps,
                                thread_id
                            );
                        }
                    }
                    crate::decoder::DecodeResult::Error(e) => {
                        warn!("[{}] Decode error: {}", camera_name, e);
                    }
                    crate::decoder::DecodeResult::Skipped => {
                        debug!("[{}] Frame skipped", camera_name);
                    }
                }
            } else {
                // No decoded frames available, try to read new frames
                match reader.read_frame() {
                    Ok(Some(frame)) => {
                        // Submit frame for decoding
                        info!(
                            "[{}] Submitting frame for decoding: {}x{}, size={}, format=0x{:x}",
                            camera_name,
                            frame.width,
                            frame.height,
                            frame.data.len(),
                            frame.format
                        );
                        if decoder.submit_frame(frame).is_none() {
                            debug!("[{}] Decoder queue full, dropping frame", camera_name);
                        } else {
                            debug!("[{}] Frame submitted to decoder successfully", camera_name);
                        }
                    }
                    Ok(None) => {
                        // No frame available, short wait
                        thread::sleep(Duration::from_millis(1));
                    }
                    Err(e) => {
                        warn!("[{}] Failed to read frame: {}", camera_name, e);
                        // Send error data occasionally
                        if frame_counter % 100 == 0 {
                            Self::send_error_data(
                                &camera_name,
                                &e.to_string(),
                                callback,
                                user_data,
                            );
                        }
                        thread::sleep(Duration::from_millis(10));
                    }
                }
            }
        }

        // Cleanup
        let _ = reader.stop();
        info!("[{}] Thread {} stopped", camera_name, thread_id);
    }

    /// Stop the camera thread
    fn stop(&mut self) {
        if let Some(handle) = self.thread_handle.take() {
            info!("[{}] Sending stop signal", self.camera_name);
            self.shutdown_signal.store(true, Ordering::Relaxed);

            // Wait for thread to finish
            if let Err(e) = handle.join() {
                warn!("[{}] Thread stop error: {:?}", self.camera_name, e);
            } else {
                info!("[{}] Thread stopped safely", self.camera_name);
            }
        }
    }

    /// Send decoded frame data as unified camera data
    fn send_decoded_frame_data(
        camera_name: &str,
        decoded_frame: &crate::decoder::DecodedFrame,
        camera_type: CCameraType,
        sequence: u64,
        fps: f32,
        callback: CameraDataCallback,
        user_data: *mut std::os::raw::c_void,
    ) {
        info!("[{}] send_decoded_frame_data: frame {}x{}, size={}, format={:?}, sequence={}, fps={:.1}", 
              camera_name, decoded_frame.width, decoded_frame.height, decoded_frame.data.len(), decoded_frame.format, sequence, fps);

        // Create frame metadata from decoded frame
        let frame_metadata =
            Self::create_frame_metadata_from_decoded(decoded_frame, camera_type, sequence);

        // Create camera status
        let camera_status = Self::create_camera_status_simple(camera_name, camera_type, fps);

        // Create single camera data
        let single_data = CSingleCameraData {
            mode: 1, // CAMERA_MODE_SINGLE_CAMERA
            timestamp_ms: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            camera_status,
            frame: frame_metadata,
            system_load: Self::get_cpu_usage(),
        };

        let unified_data = CCameraData {
            mode: 1, // CAMERA_MODE_SINGLE_CAMERA
            data: CCameraDataUnion {
                single_camera: ManuallyDrop::new(single_data),
            },
        };

        info!(
            "[{}] send_decoded_frame_data: calling callback with user_data={:p}, mode={}",
            camera_name, user_data, unified_data.mode
        );

        // Call callback
        callback(&unified_data, user_data);

        info!(
            "[{}] send_decoded_frame_data: callback completed",
            camera_name
        );
    }

    /// Send frame data as unified camera data (legacy - for raw frames)
    fn send_frame_data(
        camera_name: &str,
        frame: &VideoFrame,
        camera_type: CCameraType,
        sequence: u64,
        fps: f32,
        callback: CameraDataCallback,
        user_data: *mut std::os::raw::c_void,
    ) {
        info!(
            "[{}] send_frame_data: frame {}x{}, size={}, format=0x{:x}, sequence={}, fps={:.1}",
            camera_name,
            frame.width,
            frame.height,
            frame.data.len(),
            frame.format,
            sequence,
            fps
        );

        // Create frame metadata
        let frame_metadata = Self::create_frame_metadata_simple(frame, camera_type, sequence);

        // Create camera status
        let camera_status = Self::create_camera_status_simple(camera_name, camera_type, fps);

        // Create single camera data
        let single_data = CSingleCameraData {
            mode: 1, // CAMERA_MODE_SINGLE_CAMERA
            timestamp_ms: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            camera_status,
            frame: frame_metadata,
            system_load: Self::get_cpu_usage(),
        };

        let unified_data = CCameraData {
            mode: 1, // CAMERA_MODE_SINGLE_CAMERA
            data: CCameraDataUnion {
                single_camera: ManuallyDrop::new(single_data),
            },
        };

        info!(
            "[{}] send_frame_data: calling callback with user_data={:p}, mode={}",
            camera_name, user_data, unified_data.mode
        );

        // Call callback
        callback(&unified_data, user_data);

        info!("[{}] send_frame_data: callback completed", camera_name);
    }

    /// Send error data when no camera or error occurs
    fn send_error_data(
        camera_name: &str,
        error_msg: &str,
        callback: CameraDataCallback,
        user_data: *mut std::os::raw::c_void,
    ) {
        let mut error_message = [0; 256];
        let full_error = format!("{}: {}", camera_name, error_msg);
        Self::string_to_c_array(&full_error, &mut error_message);

        let no_camera_data = CNoCameraData {
            mode: 0, // CAMERA_MODE_NO_CAMERA
            timestamp_ms: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            detection_attempts: 1,
            error_message,
        };

        let unified_data = CCameraData {
            mode: 0, // CAMERA_MODE_NO_CAMERA
            data: CCameraDataUnion {
                no_camera: ManuallyDrop::new(no_camera_data),
            },
        };

        // Call callback
        callback(&unified_data, user_data);
    }

    /// Create frame metadata from decoded frame
    fn create_frame_metadata_from_decoded(
        decoded_frame: &crate::decoder::DecodedFrame,
        camera_type: CCameraType,
        sequence: u64,
    ) -> CFrameMetadata {
        let format = match decoded_frame.format {
            DecodedFormat::RGB888 => CImageFormat::Rgb24,
            DecodedFormat::BGR888 => CImageFormat::Rgb24, // Map to RGB24 for C++ compatibility
            DecodedFormat::RGBA8888 => CImageFormat::Rgb24, // Map to RGB24 for C++ compatibility
            DecodedFormat::BGRA8888 => CImageFormat::Rgb24, // Map to RGB24 for C++ compatibility
            _ => CImageFormat::Unknown,
        };

        CFrameMetadata {
            data: decoded_frame.data.as_ptr(),
            size: decoded_frame.data.len() as c_uint,
            width: decoded_frame.width,
            height: decoded_frame.height,
            format,
            frame_id: decoded_frame.frame_id,
            camera_type,
            status: CFrameStatus::Ok,
            timestamp_ms: decoded_frame
                .timestamp
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            sequence_number: sequence,
            exposure_us: 0, // TODO: get from camera if available
            gain: 0,        // TODO: get from camera if available
            latency_us: decoded_frame.decode_duration.as_micros() as c_uint,
        }
    }

    /// Create USB camera frame data for FFI
    fn create_usb_camera_frame_data(
        frame: &VideoFrame,
        camera_type: CCameraType,
        sequence: u64,
    ) -> UsbCameraFrameData {
        let format = match frame.format {
            0x56595559 => UsbCameraFormat::Yuyv,   // YUYV fourcc
            0x47504A4D => UsbCameraFormat::Mjpg,   // MJPG fourcc
            0x32424752 => UsbCameraFormat::Rgb888, // RGB24 fourcc
            _ => UsbCameraFormat::Rgb888,          // Default to RGB888
        };

        let usb_camera_type = match camera_type {
            CCameraType::Left => UsbCameraType::Left,
            CCameraType::Right => UsbCameraType::Right,
            CCameraType::Single => UsbCameraType::Single,
            _ => UsbCameraType::Unknown,
        };

        UsbCameraFrameData {
            data: frame.data.as_ptr(),
            size: frame.data.len() as c_uint,
            width: frame.width,
            height: frame.height,
            format,
            frame_id: frame.frame_id,
            camera_type: usb_camera_type,
            timestamp: frame
                .timestamp
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
        }
    }

    /// Create simplified frame metadata (legacy - for raw frames)
    fn create_frame_metadata_simple(
        frame: &VideoFrame,
        camera_type: CCameraType,
        sequence: u64,
    ) -> CFrameMetadata {
        let format = match frame.format {
            0x56595559 => CImageFormat::Yuyv,  // YUYV fourcc
            0x47504A4D => CImageFormat::Mjpg,  // MJPG fourcc
            0x32424752 => CImageFormat::Rgb24, // RGB24 fourcc
            _ => CImageFormat::Unknown,
        };

        CFrameMetadata {
            data: frame.data.as_ptr(),
            size: frame.data.len() as c_uint,
            width: frame.width,
            height: frame.height,
            format,
            frame_id: frame.frame_id,
            camera_type,
            status: CFrameStatus::Ok,
            timestamp_ms: frame
                .timestamp
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            sequence_number: sequence,
            exposure_us: 0, // TODO: get from camera if available
            gain: 0,        // TODO: get from camera if available
            latency_us: 0,  // TODO: calculate processing latency
        }
    }

    /// Create simplified camera status
    fn create_camera_status_simple(
        camera_name: &str,
        camera_type: CCameraType,
        fps: f32,
    ) -> CCameraStatus {
        let mut name = [0; 64];
        let mut device_path = [0; 256];

        Self::string_to_c_array(camera_name, &mut name);
        Self::string_to_c_array("/dev/videoX", &mut device_path); // Simplified

        CCameraStatus {
            name,
            device_path,
            camera_type,
            connected: 1,
            fps,
            total_frames: 0,
            dropped_frames: 0,
            temperature: 0.0,
        }
    }

    /// Convert string to C string array
    fn string_to_c_array(s: &str, array: &mut [c_char]) {
        let bytes = s.as_bytes();
        let len = std::cmp::min(bytes.len(), array.len() - 1);
        for (i, &byte) in bytes.iter().take(len).enumerate() {
            array[i] = byte as c_char;
        }
        array[len] = 0; // null terminator
    }

    /// Get CPU usage (simplified)
    fn get_cpu_usage() -> f32 {
        // TODO: implement actual CPU usage calculation
        // For now, return a simulated value
        25.5
    }
}

impl Drop for CameraThread {
    fn drop(&mut self) {
        self.stop();
    }
}

/// Multi-threaded camera stream manager for C++ interface
pub struct CameraStreamManager {
    /// Camera status monitor
    monitor: CameraStatusMonitor,
    /// Current camera mode
    current_mode: CameraMode,
    /// Left camera thread
    left_thread: Option<CameraThread>,
    /// Right camera thread
    right_thread: Option<CameraThread>,
    /// Single camera thread
    single_thread: Option<CameraThread>,
    /// Camera configuration
    config: CameraConfig,
    /// Running state
    running: Arc<AtomicBool>,
    /// Management thread handle
    management_thread: Option<JoinHandle<()>>,
    /// Unified data callback
    data_callback: Option<CameraDataCallback>,
    /// User data pointers
    data_user_data: *mut std::os::raw::c_void,
    /// Frame counters and statistics
    frame_sequence: Arc<AtomicBool>,
    detection_attempts: u32,
}

impl CameraStreamManager {
    /// Create a new camera stream manager
    pub fn new() -> Self {
        let config = CameraConfig {
            width: 1280,
            height: 720,
            framerate: 30,
            pixel_format: "MJPG".to_string(),
            parameters: HashMap::new(),
        };

        Self {
            monitor: CameraStatusMonitor::new(1000),
            current_mode: CameraMode::NoCamera,
            left_thread: None,
            right_thread: None,
            single_thread: None,
            config,
            running: Arc::new(AtomicBool::new(false)),
            management_thread: None,
            data_callback: None,
            data_user_data: std::ptr::null_mut(),
            frame_sequence: Arc::new(AtomicBool::new(false)),
            detection_attempts: 0,
        }
    }

    /// Register unified data callback
    pub fn register_data_callback(
        &mut self,
        callback: CameraDataCallback,
        user_data: *mut std::os::raw::c_void,
    ) {
        self.data_callback = Some(callback);
        self.data_user_data = user_data;
        info!("Unified data callback registered");
    }

    /// Start the camera stream manager
    pub fn start(&mut self) -> CameraResult<()> {
        if self.running.load(Ordering::Relaxed) {
            return Ok(());
        }

        info!("Starting multi-threaded camera stream manager");

        // Start status monitor
        self.monitor.start()?;

        // Set running flag
        self.running.store(true, Ordering::Relaxed);

        // Get initial camera status and start appropriate threads
        if let Ok(status) = CameraStatusMonitor::get_current_status() {
            self.current_mode = status.mode.clone();
            self.update_camera_threads(&status.mode, &status.cameras);
        }

        // Start management thread for dynamic mode switching
        let running = self.running.clone();
        let mut monitor = CameraStatusMonitor::new(1000);
        monitor.start()?;

        let handle = thread::spawn(move || {
            let mut current_mode = CameraMode::NoCamera;

            while running.load(Ordering::Relaxed) {
                if let Some(status) = monitor.try_get_status() {
                    if status.mode != current_mode {
                        info!(
                            "Camera mode changed: {:?} -> {:?}",
                            current_mode, status.mode
                        );
                        current_mode = status.mode;
                        // Note: In a real implementation, we'd need to communicate this change
                        // back to the main manager. For now, we just log it.
                    }
                }
                thread::sleep(Duration::from_millis(100));
            }
        });

        self.management_thread = Some(handle);

        info!("Camera stream manager started");
        Ok(())
    }

    /// Stop the camera stream manager
    pub fn stop(&mut self) -> CameraResult<()> {
        if !self.running.load(Ordering::Relaxed) {
            return Ok(());
        }

        info!("Stopping camera stream manager");

        // Set stop flag
        self.running.store(false, Ordering::Relaxed);

        // Stop all camera threads
        self.stop_all_threads();

        // Stop management thread
        if let Some(handle) = self.management_thread.take() {
            if let Err(e) = handle.join() {
                warn!("Management thread panicked: {:?}", e);
            }
        }

        // Stop monitoring
        self.monitor.stop()?;

        info!("Camera stream manager stopped");
        Ok(())
    }

    /// Get current camera mode
    pub fn get_mode(&self) -> CameraMode {
        self.current_mode.clone()
    }

    /// Update camera threads based on mode
    fn update_camera_threads(&mut self, mode: &CameraMode, cameras: &[V4L2Device]) {
        // Stop all current threads
        self.stop_all_threads();

        // Use unified callback if available
        if let Some(data_callback) = self.data_callback {
            self.update_camera_threads_unified(mode, cameras, data_callback);
        }
    }

    /// Update camera threads with unified callback
    fn update_camera_threads_unified(
        &mut self,
        mode: &CameraMode,
        cameras: &[V4L2Device],
        callback: CameraDataCallback,
    ) {
        match mode {
            CameraMode::NoCamera => {
                info!("No camera mode - sending no camera data");
                // Send no camera data periodically
                self.send_no_camera_data_periodically(callback);
            }
            CameraMode::SingleCamera => {
                if let Some(camera) = cameras.first() {
                    info!(
                        "Single camera mode - starting unified thread: {}",
                        camera.name
                    );
                    match CameraThread::new_with_unified_callback(
                        camera,
                        &self.config,
                        CCameraType::Single,
                        callback,
                        self.data_user_data,
                    ) {
                        Ok(thread) => {
                            self.single_thread = Some(thread);
                            info!("Single camera unified thread started successfully");
                        }
                        Err(e) => {
                            error!("Failed to start single camera unified thread: {}", e);
                        }
                    }
                } else {
                    warn!("No cameras available for single camera mode");
                }
            }
            CameraMode::StereoCamera => {
                info!("Stereo camera mode - starting unified dual threads");
                self.start_stereo_unified_threads(cameras, callback);
            }
        }
    }

    /// Start stereo camera threads with unified callback
    fn start_stereo_unified_threads(
        &mut self,
        cameras: &[V4L2Device],
        callback: CameraDataCallback,
    ) {
        // Identify left and right cameras
        let left_camera = cameras.iter().find(|c| {
            c.name.contains("left")
                || c.name.contains("Left")
                || c.name.contains("cameraL")
                || c.path.to_string_lossy().contains("video0")
        });
        let right_camera = cameras.iter().find(|c| {
            c.name.contains("right")
                || c.name.contains("Right")
                || c.name.contains("cameraR")
                || c.path.to_string_lossy().contains("video2")
        });

        // If no specific naming, use first two cameras
        let (left_cam, right_cam) = if left_camera.is_none() && right_camera.is_none() {
            (cameras.get(0), cameras.get(1))
        } else {
            (left_camera, right_camera)
        };

        // For stereo mode, we need special handling to combine frames
        // For now, start each camera independently and let them send single camera data
        // TODO: Implement proper stereo frame synchronization

        // Start left camera thread
        if let Some(left) = left_cam {
            match CameraThread::new_with_unified_callback(
                left,
                &self.config,
                CCameraType::Left,
                callback,
                self.data_user_data,
            ) {
                Ok(thread) => {
                    self.left_thread = Some(thread);
                    info!(
                        "Left camera unified thread started successfully: {}",
                        left.name
                    );
                }
                Err(e) => {
                    error!("Failed to start left camera unified thread: {}", e);
                }
            }
        }

        // Start right camera thread
        if let Some(right) = right_cam {
            match CameraThread::new_with_unified_callback(
                right,
                &self.config,
                CCameraType::Right,
                callback,
                self.data_user_data,
            ) {
                Ok(thread) => {
                    self.right_thread = Some(thread);
                    info!(
                        "Right camera unified thread started successfully: {}",
                        right.name
                    );
                }
                Err(e) => {
                    error!("Failed to start right camera unified thread: {}", e);
                }
            }
        }
    }

    /// Send no camera data periodically
    fn send_no_camera_data_periodically(&mut self, callback: CameraDataCallback) {
        // For now, just send once. In a real implementation, you might want
        // to send periodic updates
        let mut error_message = [0; 256];
        Self::string_to_c_array("No cameras detected", &mut error_message);

        let no_camera_data = CNoCameraData {
            mode: 0, // CAMERA_MODE_NO_CAMERA
            timestamp_ms: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            detection_attempts: self.detection_attempts,
            error_message,
        };

        let unified_data = CCameraData {
            mode: 0, // CAMERA_MODE_NO_CAMERA
            data: CCameraDataUnion {
                no_camera: ManuallyDrop::new(no_camera_data),
            },
        };

        // Call callback
        callback(&unified_data, self.data_user_data);
        info!("Sent no camera data");
    }

    /// Stop all camera threads
    fn stop_all_threads(&mut self) {
        if let Some(mut thread) = self.left_thread.take() {
            info!("Stopping left camera thread");
            thread.stop();
        }
        if let Some(mut thread) = self.right_thread.take() {
            info!("Stopping right camera thread");
            thread.stop();
        }
        if let Some(mut thread) = self.single_thread.take() {
            info!("Stopping single camera thread");
            thread.stop();
        }
    }

    /// Force update camera mode (for testing or manual control)
    pub fn update_mode(&mut self) -> CameraResult<()> {
        if let Ok(status) = CameraStatusMonitor::get_current_status() {
            if status.mode != self.current_mode {
                info!(
                    "Mode change detected: {:?} -> {:?}",
                    self.current_mode, status.mode
                );
                self.current_mode = status.mode.clone();
                self.update_camera_threads(&status.mode, &status.cameras);
            }
        }
        Ok(())
    }

    /// Is the stream manager running?
    pub fn is_running(&self) -> bool {
        self.running.load(Ordering::Relaxed)
    }

    /// Convert string to C string array
    fn string_to_c_array(s: &str, array: &mut [c_char]) {
        let bytes = s.as_bytes();
        let len = std::cmp::min(bytes.len(), array.len() - 1);
        for (i, &byte) in bytes.iter().take(len).enumerate() {
            array[i] = byte as c_char;
        }
        array[len] = 0; // null terminator
    }

    /// Create no camera data
    fn create_no_camera_data(&self) -> CCameraData {
        let mut error_msg = [0; 256];
        Self::string_to_c_array("No cameras detected", &mut error_msg);

        let no_camera_data = CNoCameraData {
            mode: 0, // CameraMode::NoCamera
            timestamp_ms: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            detection_attempts: self.detection_attempts,
            error_message: error_msg,
        };

        CCameraData {
            mode: 0, // CameraMode::NoCamera
            data: CCameraDataUnion {
                no_camera: ManuallyDrop::new(no_camera_data),
            },
        }
    }

    /// Create camera status from device
    #[allow(dead_code)]
    fn create_camera_status(
        device: &V4L2Device,
        camera_type: CCameraType,
        fps: f32,
    ) -> CCameraStatus {
        let mut name = [0; 64];
        let mut device_path = [0; 256];

        Self::string_to_c_array(&device.name, &mut name);
        Self::string_to_c_array(&device.path.to_string_lossy(), &mut device_path);

        CCameraStatus {
            name,
            device_path,
            camera_type,
            connected: 1,
            fps,
            total_frames: 0,   // TODO: implement frame counting
            dropped_frames: 0, // TODO: implement dropped frame counting
            temperature: 0.0,  // TODO: implement temperature reading if available
        }
    }

    /// Create frame metadata from video frame
    #[allow(dead_code)]
    fn create_frame_metadata(
        frame: &VideoFrame,
        camera_type: CCameraType,
        sequence: u64,
    ) -> CFrameMetadata {
        let format = match frame.format {
            0x56595559 => CImageFormat::Yuyv,  // YUYV fourcc
            0x47504A4D => CImageFormat::Mjpg,  // MJPG fourcc
            0x32424752 => CImageFormat::Rgb24, // RGB24 fourcc
            _ => CImageFormat::Unknown,
        };

        CFrameMetadata {
            data: frame.data.as_ptr(),
            size: frame.data.len() as c_uint,
            width: frame.width,
            height: frame.height,
            format,
            frame_id: frame.frame_id,
            camera_type,
            status: CFrameStatus::Ok,
            timestamp_ms: frame
                .timestamp
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_millis() as u64,
            sequence_number: sequence,
            exposure_us: 0, // TODO: get from camera if available
            gain: 0,        // TODO: get from camera if available
            latency_us: 0,  // TODO: calculate processing latency
        }
    }

    /// Get system load (simplified)
    fn get_system_load() -> f32 {
        // TODO: implement actual system load calculation
        0.0
    }
}

// C Interface Functions
/// Opaque handle for C++ interface
type CameraStreamHandle = *mut CameraStreamManager;

/// Create a new camera stream manager instance
#[no_mangle]
pub extern "C" fn camera_stream_create() -> CameraStreamHandle {
    let manager = Box::new(CameraStreamManager::new());
    Box::into_raw(manager)
}

/// Start the camera stream manager
#[no_mangle]
pub extern "C" fn camera_stream_start(handle: CameraStreamHandle) -> CameraStreamError {
    if handle.is_null() {
        return CameraStreamError::InvalidInstance;
    }

    let manager = unsafe { &mut *handle };
    match manager.start() {
        Ok(_) => CameraStreamError::Success,
        Err(_) => CameraStreamError::InitializationFailed,
    }
}

/// Stop the camera stream manager
#[no_mangle]
pub extern "C" fn camera_stream_stop(handle: CameraStreamHandle) -> CameraStreamError {
    if handle.is_null() {
        return CameraStreamError::InvalidInstance;
    }

    let manager = unsafe { &mut *handle };
    match manager.stop() {
        Ok(_) => CameraStreamError::Success,
        Err(_) => CameraStreamError::StreamStopFailed,
    }
}

/// Destroy the camera stream manager instance
#[no_mangle]
pub extern "C" fn camera_stream_destroy(handle: CameraStreamHandle) -> CameraStreamError {
    if handle.is_null() {
        return CameraStreamError::InvalidInstance;
    }

    let manager = unsafe { Box::from_raw(handle) };
    let _ = manager; // Drop the manager
    CameraStreamError::Success
}

/// Get current camera mode
#[no_mangle]
pub extern "C" fn camera_stream_get_mode(handle: CameraStreamHandle) -> c_int {
    if handle.is_null() {
        return -1;
    }

    let manager = unsafe { &*handle };
    match manager.get_mode() {
        CameraMode::NoCamera => 0,
        CameraMode::SingleCamera => 1,
        CameraMode::StereoCamera => 2,
    }
}

/// Register unified camera data callback (recommended)
#[no_mangle]
pub extern "C" fn camera_stream_register_data_callback(
    handle: CameraStreamHandle,
    callback: CameraDataCallback,
    user_data: *mut std::os::raw::c_void,
) -> CameraStreamError {
    if handle.is_null() {
        return CameraStreamError::InvalidInstance;
    }

    let manager = unsafe { &mut *handle };
    manager.register_data_callback(callback, user_data);
    CameraStreamError::Success
}

/// Force update camera mode (useful for testing)
#[no_mangle]
pub extern "C" fn camera_stream_update_mode(handle: CameraStreamHandle) -> CameraStreamError {
    if handle.is_null() {
        return CameraStreamError::InvalidInstance;
    }

    let manager = unsafe { &mut *handle };
    match manager.update_mode() {
        Ok(_) => CameraStreamError::Success,
        Err(_) => CameraStreamError::InitializationFailed,
    }
}

/// Check if camera stream manager is running
#[no_mangle]
pub extern "C" fn camera_stream_is_running(handle: CameraStreamHandle) -> c_int {
    if handle.is_null() {
        return 0;
    }

    let manager = unsafe { &*handle };
    if manager.is_running() {
        1
    } else {
        0
    }
}

// ========== FFI Interface for SmartScope Integration ==========

/// FFI error codes
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum UsbCameraError {
    Success = 0,
    InvalidHandle = -1,
    InitFailed = -2,
    DeviceNotFound = -3,
    StartFailed = -4,
    StopFailed = -5,
    NoFrame = -6,
    InvalidParam = -8,
}

/// FFI camera modes
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum UsbCameraMode {
    NoCamera = 0,
    Single = 1,
    Stereo = 2,
}

/// FFI camera types
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum UsbCameraType {
    Unknown = -1,
    Left = 0,
    Right = 1,
    Single = 2,
}

/// FFI image formats
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum UsbCameraFormat {
    Rgb888 = 0,
    Bgr888 = 1,
    Mjpg = 2,
    Yuyv = 3,
}

/// FFI camera status
#[repr(C)]
#[derive(Debug)]
pub struct UsbCameraStatus {
    pub mode: UsbCameraMode,
    pub camera_count: c_uint,
    pub left_connected: bool,
    pub right_connected: bool,
    pub timestamp: u64,
}

/// FFI frame data
#[repr(C)]
#[derive(Debug)]
pub struct UsbCameraFrameData {
    pub data: *const u8,
    pub size: c_uint,
    pub width: c_uint,
    pub height: c_uint,
    pub format: UsbCameraFormat,
    pub frame_id: u64,
    pub camera_type: UsbCameraType,
    pub timestamp: u64,
}

/// FFI camera info
#[repr(C)]
#[derive(Debug)]
pub struct UsbCameraInfo {
    pub name: [c_char; 64],
    pub device_path: [c_char; 256],
    pub camera_type: UsbCameraType,
    pub connected: c_int,
    pub fps: f32,
    pub total_frames: u64,
    pub dropped_frames: u64,
}

/// FFI no camera data
#[repr(C)]
#[derive(Debug)]
pub struct UsbNoCameraData {
    pub mode: UsbCameraMode,
    pub timestamp_ms: u64,
    pub detection_attempts: c_uint,
    pub error_message: [c_char; 256],
}

/// FFI single camera data
#[repr(C)]
#[derive(Debug)]
pub struct UsbSingleCameraData {
    pub mode: UsbCameraMode,
    pub timestamp_ms: u64,
    pub camera_info: UsbCameraInfo,
    pub frame: UsbCameraFrameData,
    pub system_load: f32,
}

/// FFI stereo camera data
#[repr(C)]
#[derive(Debug)]
pub struct UsbStereoCameraData {
    pub mode: UsbCameraMode,
    pub timestamp_ms: u64,
    pub left_camera_info: UsbCameraInfo,
    pub right_camera_info: UsbCameraInfo,
    pub left_frame: UsbCameraFrameData,
    pub right_frame: UsbCameraFrameData,
    pub sync_delta_us: i32,
    pub baseline_mm: f32,
    pub system_load: f32,
}

/// FFI unified camera data
#[repr(C)]
pub union UsbCameraDataUnion {
    pub no_camera: ManuallyDrop<UsbNoCameraData>,
    pub single_camera: ManuallyDrop<UsbSingleCameraData>,
    pub stereo_camera: ManuallyDrop<UsbStereoCameraData>,
}

#[repr(C)]
pub struct UsbCameraData {
    pub mode: UsbCameraMode,
    pub data: UsbCameraDataUnion,
}

/// FFI callback type
pub type UsbCameraDataCallback = extern "C" fn(*const UsbCameraData, *mut c_void);

/// FFI handle type
pub type UsbCameraHandle = *mut CameraStreamManager;

/// Global initialization - required before any other functions
#[no_mangle]
pub extern "C" fn usb_camera_init() -> UsbCameraError {
    // Initialize logging if not already done
    let _ = env_logger::try_init();
    info!("USB Camera FFI initialized");
    UsbCameraError::Success
}

/// Global cleanup
#[no_mangle]
pub extern "C" fn usb_camera_cleanup() {
    info!("USB Camera FFI cleanup");
}

/// Create a new camera manager instance
#[no_mangle]
pub extern "C" fn usb_camera_create_instance() -> UsbCameraHandle {
    info!("Creating USB camera instance");

    let manager = Box::new(CameraStreamManager::new());
    Box::into_raw(manager) as UsbCameraHandle
}

/// Destroy a camera manager instance
#[no_mangle]
pub extern "C" fn usb_camera_destroy_instance(handle: UsbCameraHandle) {
    if handle.is_null() {
        warn!("Attempted to destroy null handle");
        return;
    }

    info!("Destroying USB camera instance");

    let _manager = unsafe { Box::from_raw(handle) };
    // Manager will be dropped automatically
}

/// Start the camera manager
#[no_mangle]
pub extern "C" fn usb_camera_start(handle: UsbCameraHandle) -> UsbCameraError {
    if handle.is_null() {
        return UsbCameraError::InvalidHandle;
    }

    let manager = unsafe { &mut *handle };

    match manager.start() {
        Ok(_) => {
            info!("USB camera manager started");
            UsbCameraError::Success
        }
        Err(e) => {
            error!("Failed to start USB camera manager: {}", e);
            UsbCameraError::StartFailed
        }
    }
}

/// Stop the camera manager
#[no_mangle]
pub extern "C" fn usb_camera_stop(handle: UsbCameraHandle) -> UsbCameraError {
    if handle.is_null() {
        return UsbCameraError::InvalidHandle;
    }

    let manager = unsafe { &mut *handle };

    match manager.stop() {
        Ok(_) => {
            info!("USB camera manager stopped");
            UsbCameraError::Success
        }
        Err(e) => {
            error!("Failed to stop USB camera manager: {}", e);
            UsbCameraError::StopFailed
        }
    }
}

/// Register unified data callback
#[no_mangle]
pub extern "C" fn usb_camera_register_data_callback(
    handle: UsbCameraHandle,
    callback: UsbCameraDataCallback,
    user_data: *mut c_void,
) -> UsbCameraError {
    if handle.is_null() {
        error!("usb_camera_register_data_callback: handle is null");
        return UsbCameraError::InvalidHandle;
    }

    let manager = unsafe { &mut *handle };

    info!(
        "usb_camera_register_data_callback: registering callback with user_data={:p}",
        user_data
    );

    // Convert UsbCameraDataCallback to CameraDataCallback
    let converted_callback: CameraDataCallback = unsafe { std::mem::transmute(callback) };

    manager.register_data_callback(converted_callback, user_data);

    info!("usb_camera_register_data_callback: callback registered successfully");
    UsbCameraError::Success
}

/// Get camera status
#[no_mangle]
pub extern "C" fn usb_camera_get_status(
    handle: UsbCameraHandle,
    status: *mut UsbCameraStatus,
) -> UsbCameraError {
    if handle.is_null() || status.is_null() {
        return UsbCameraError::InvalidHandle;
    }

    let manager = unsafe { &*handle };
    let current_mode = manager.get_mode();

    let usb_status = unsafe { &mut *status };

    usb_status.mode = match current_mode {
        CameraMode::NoCamera => UsbCameraMode::NoCamera,
        CameraMode::SingleCamera => UsbCameraMode::Single,
        CameraMode::StereoCamera => UsbCameraMode::Stereo,
    };

    // For now, set basic values
    usb_status.camera_count = match current_mode {
        CameraMode::NoCamera => 0,
        CameraMode::SingleCamera => 1,
        CameraMode::StereoCamera => 2,
    };

    usb_status.left_connected = matches!(
        current_mode,
        CameraMode::SingleCamera | CameraMode::StereoCamera
    );
    usb_status.right_connected = matches!(current_mode, CameraMode::StereoCamera);
    usb_status.timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64;

    UsbCameraError::Success
}

/// Get left camera frame (optional direct access)
#[no_mangle]
pub extern "C" fn usb_camera_get_left_frame(
    handle: UsbCameraHandle,
    frame: *mut UsbCameraFrameData,
) -> UsbCameraError {
    if handle.is_null() || frame.is_null() {
        return UsbCameraError::InvalidHandle;
    }

    // TODO: Implement frame access
    warn!("Direct frame access not implemented yet");
    UsbCameraError::NoFrame
}

/// Get right camera frame (optional direct access)
#[no_mangle]
pub extern "C" fn usb_camera_get_right_frame(
    handle: UsbCameraHandle,
    frame: *mut UsbCameraFrameData,
) -> UsbCameraError {
    if handle.is_null() || frame.is_null() {
        return UsbCameraError::InvalidHandle;
    }

    // TODO: Implement frame access
    warn!("Direct frame access not implemented yet");
    UsbCameraError::NoFrame
}

// ========== SmartScope Core Compatibility Layer ==========

/// Global state for compatibility layer
static mut GLOBAL_INSTANCES: Option<HashMap<u32, UsbCameraHandle>> = None;
static mut NEXT_INSTANCE_ID: u32 = 1;
static GLOBAL_MUTEX: std::sync::Mutex<()> = std::sync::Mutex::new(());

/// SmartScope compatible error codes
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum SmartScopeResult {
    Success = 0,
    ErrorInvalidHandle = -1,
    ErrorInitFailed = -2,
    ErrorDeviceNotFound = -3,
    ErrorStartFailed = -4,
    ErrorStopFailed = -5,
    ErrorNoFrame = -6,
    ErrorInvalidParam = -8,
}

/// SmartScope compatible camera modes
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum SmartScopeCameraMode {
    NoCamera = 0,
    Single = 1,
    Stereo = 2,
}

/// SmartScope compatible image formats
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum SmartScopeImageFormat {
    Rgb888 = 0,
    Bgr888 = 1,
    Mjpg = 2,
    Yuyv = 3,
}

/// SmartScope compatible camera status
#[repr(C)]
#[derive(Debug)]
pub struct SmartScopeCameraStatus {
    pub mode: SmartScopeCameraMode,
    pub camera_count: c_uint,
    pub left_connected: bool,
    pub right_connected: bool,
    pub timestamp: u64,
}

/// SmartScope compatible frame data
#[repr(C)]
#[derive(Debug)]
pub struct SmartScopeFrameData {
    pub data: *const u8,
    pub size: c_uint,
    pub width: c_uint,
    pub height: c_uint,
    pub format: SmartScopeImageFormat,
    pub frame_id: u64,
    pub timestamp: u64,
}

/// SmartScope compatible stream config
#[repr(C)]
#[derive(Debug)]
pub struct SmartScopeStreamConfig {
    pub width: c_uint,
    pub height: c_uint,
    pub format: SmartScopeImageFormat,
    pub fps: c_uint,
    pub read_interval_ms: c_uint,
}

/// SmartScope compatible correction config
#[repr(C)]
#[derive(Debug)]
pub struct SmartScopeCorrectionConfig {
    pub correction_type: c_int,
    pub params_dir: *const c_char,
    pub enable_caching: bool,
}

/// Initialize global state
fn ensure_global_state() {
    unsafe {
        if GLOBAL_INSTANCES.is_none() {
            GLOBAL_INSTANCES = Some(HashMap::new());
        }
    }
}

/// SmartScope compatible initialization
#[no_mangle]
pub extern "C" fn smartscope_init() -> SmartScopeResult {
    let _lock = GLOBAL_MUTEX.lock().unwrap();
    ensure_global_state();

    // Initialize logging if not already done
    let _ = env_logger::try_init();
    info!("SmartScope compatibility layer initialized");

    SmartScopeResult::Success
}

/// SmartScope compatible cleanup
#[no_mangle]
pub extern "C" fn smartscope_cleanup() {
    let _lock = GLOBAL_MUTEX.lock().unwrap();

    unsafe {
        if let Some(instances) = &mut GLOBAL_INSTANCES {
            // Clean up all instances
            for (_, handle) in instances.drain() {
                usb_camera_destroy_instance(handle);
            }
        }
        GLOBAL_INSTANCES = None;
        NEXT_INSTANCE_ID = 1;
    }

    info!("SmartScope compatibility layer cleanup");
}

/// Create SmartScope compatible instance
#[no_mangle]
pub extern "C" fn smartscope_create_instance() -> u32 {
    let _lock = GLOBAL_MUTEX.lock().unwrap();
    ensure_global_state();

    let handle = usb_camera_create_instance();
    if handle.is_null() {
        return 0;
    }

    unsafe {
        let instance_id = NEXT_INSTANCE_ID;
        NEXT_INSTANCE_ID += 1;

        if let Some(instances) = &mut GLOBAL_INSTANCES {
            instances.insert(instance_id, handle);
        }

        info!("Created SmartScope instance {}", instance_id);
        instance_id
    }
}

/// Destroy SmartScope compatible instance
#[no_mangle]
pub extern "C" fn smartscope_destroy_instance(instance_id: u32) {
    let _lock = GLOBAL_MUTEX.lock().unwrap();

    unsafe {
        if let Some(instances) = &mut GLOBAL_INSTANCES {
            if let Some(handle) = instances.remove(&instance_id) {
                usb_camera_destroy_instance(handle);
                info!("Destroyed SmartScope instance {}", instance_id);
            } else {
                warn!("Attempted to destroy non-existent instance {}", instance_id);
            }
        }
    }
}

/// Initialize SmartScope compatible instance
#[no_mangle]
pub extern "C" fn smartscope_initialize(
    instance_id: u32,
    _stream_config: *const SmartScopeStreamConfig,
    _correction_config: *const SmartScopeCorrectionConfig,
) -> SmartScopeResult {
    let _lock = GLOBAL_MUTEX.lock().unwrap();

    unsafe {
        if let Some(instances) = &GLOBAL_INSTANCES {
            if instances.contains_key(&instance_id) {
                info!("Initialized SmartScope instance {}", instance_id);
                return SmartScopeResult::Success;
            }
        }
    }

    SmartScopeResult::ErrorInvalidHandle
}

/// Start SmartScope compatible instance
#[no_mangle]
pub extern "C" fn smartscope_start(instance_id: u32) -> SmartScopeResult {
    let _lock = GLOBAL_MUTEX.lock().unwrap();

    unsafe {
        if let Some(instances) = &GLOBAL_INSTANCES {
            if let Some(&handle) = instances.get(&instance_id) {
                match usb_camera_start(handle) {
                    UsbCameraError::Success => {
                        info!("Started SmartScope instance {}", instance_id);
                        SmartScopeResult::Success
                    }
                    UsbCameraError::StartFailed => SmartScopeResult::ErrorStartFailed,
                    _ => SmartScopeResult::ErrorStartFailed,
                }
            } else {
                SmartScopeResult::ErrorInvalidHandle
            }
        } else {
            SmartScopeResult::ErrorInvalidHandle
        }
    }
}

/// Stop SmartScope compatible instance
#[no_mangle]
pub extern "C" fn smartscope_stop(instance_id: u32) -> SmartScopeResult {
    let _lock = GLOBAL_MUTEX.lock().unwrap();

    unsafe {
        if let Some(instances) = &GLOBAL_INSTANCES {
            if let Some(&handle) = instances.get(&instance_id) {
                match usb_camera_stop(handle) {
                    UsbCameraError::Success => {
                        info!("Stopped SmartScope instance {}", instance_id);
                        SmartScopeResult::Success
                    }
                    UsbCameraError::StopFailed => SmartScopeResult::ErrorStopFailed,
                    _ => SmartScopeResult::ErrorStopFailed,
                }
            } else {
                SmartScopeResult::ErrorInvalidHandle
            }
        } else {
            SmartScopeResult::ErrorInvalidHandle
        }
    }
}

/// Get camera status - SmartScope compatible
#[no_mangle]
pub extern "C" fn smartscope_get_camera_status(
    instance_id: u32,
    status: *mut SmartScopeCameraStatus,
) -> SmartScopeResult {
    if status.is_null() {
        return SmartScopeResult::ErrorInvalidParam;
    }

    let _lock = GLOBAL_MUTEX.lock().unwrap();

    unsafe {
        if let Some(instances) = &GLOBAL_INSTANCES {
            if let Some(&handle) = instances.get(&instance_id) {
                let mut usb_status = UsbCameraStatus {
                    mode: UsbCameraMode::NoCamera,
                    camera_count: 0,
                    left_connected: false,
                    right_connected: false,
                    timestamp: 0,
                };

                match usb_camera_get_status(handle, &mut usb_status) {
                    UsbCameraError::Success => {
                        let smartscope_status = &mut *status;

                        smartscope_status.mode = match usb_status.mode {
                            UsbCameraMode::NoCamera => SmartScopeCameraMode::NoCamera,
                            UsbCameraMode::Single => SmartScopeCameraMode::Single,
                            UsbCameraMode::Stereo => SmartScopeCameraMode::Stereo,
                        };

                        smartscope_status.camera_count = usb_status.camera_count;
                        smartscope_status.left_connected = usb_status.left_connected;
                        smartscope_status.right_connected = usb_status.right_connected;
                        smartscope_status.timestamp = usb_status.timestamp;

                        SmartScopeResult::Success
                    }
                    _ => SmartScopeResult::ErrorInvalidHandle,
                }
            } else {
                SmartScopeResult::ErrorInvalidHandle
            }
        } else {
            SmartScopeResult::ErrorInvalidHandle
        }
    }
}

/// Get left frame - SmartScope compatible
#[no_mangle]
pub extern "C" fn smartscope_get_left_frame(
    instance_id: u32,
    frame: *mut SmartScopeFrameData,
) -> SmartScopeResult {
    if frame.is_null() {
        return SmartScopeResult::ErrorInvalidParam;
    }

    let _lock = GLOBAL_MUTEX.lock().unwrap();

    unsafe {
        if let Some(instances) = &GLOBAL_INSTANCES {
            if let Some(&handle) = instances.get(&instance_id) {
                let mut usb_frame = UsbCameraFrameData {
                    data: ptr::null(),
                    size: 0,
                    width: 0,
                    height: 0,
                    format: UsbCameraFormat::Rgb888,
                    frame_id: 0,
                    camera_type: UsbCameraType::Left,
                    timestamp: 0,
                };

                match usb_camera_get_left_frame(handle, &mut usb_frame) {
                    UsbCameraError::Success => {
                        let smartscope_frame = &mut *frame;

                        smartscope_frame.data = usb_frame.data;
                        smartscope_frame.size = usb_frame.size;
                        smartscope_frame.width = usb_frame.width;
                        smartscope_frame.height = usb_frame.height;
                        smartscope_frame.format = match usb_frame.format {
                            UsbCameraFormat::Rgb888 => SmartScopeImageFormat::Rgb888,
                            UsbCameraFormat::Bgr888 => SmartScopeImageFormat::Bgr888,
                            UsbCameraFormat::Mjpg => SmartScopeImageFormat::Mjpg,
                            UsbCameraFormat::Yuyv => SmartScopeImageFormat::Yuyv,
                        };
                        smartscope_frame.frame_id = usb_frame.frame_id;
                        smartscope_frame.timestamp = usb_frame.timestamp;

                        SmartScopeResult::Success
                    }
                    UsbCameraError::NoFrame => SmartScopeResult::ErrorNoFrame,
                    _ => SmartScopeResult::ErrorInvalidHandle,
                }
            } else {
                SmartScopeResult::ErrorInvalidHandle
            }
        } else {
            SmartScopeResult::ErrorInvalidHandle
        }
    }
}

/// Get right frame - SmartScope compatible
#[no_mangle]
pub extern "C" fn smartscope_get_right_frame(
    instance_id: u32,
    frame: *mut SmartScopeFrameData,
) -> SmartScopeResult {
    if frame.is_null() {
        return SmartScopeResult::ErrorInvalidParam;
    }

    let _lock = GLOBAL_MUTEX.lock().unwrap();

    unsafe {
        if let Some(instances) = &GLOBAL_INSTANCES {
            if let Some(&handle) = instances.get(&instance_id) {
                let mut usb_frame = UsbCameraFrameData {
                    data: ptr::null(),
                    size: 0,
                    width: 0,
                    height: 0,
                    format: UsbCameraFormat::Rgb888,
                    frame_id: 0,
                    camera_type: UsbCameraType::Right,
                    timestamp: 0,
                };

                match usb_camera_get_right_frame(handle, &mut usb_frame) {
                    UsbCameraError::Success => {
                        let smartscope_frame = &mut *frame;

                        smartscope_frame.data = usb_frame.data;
                        smartscope_frame.size = usb_frame.size;
                        smartscope_frame.width = usb_frame.width;
                        smartscope_frame.height = usb_frame.height;
                        smartscope_frame.format = match usb_frame.format {
                            UsbCameraFormat::Rgb888 => SmartScopeImageFormat::Rgb888,
                            UsbCameraFormat::Bgr888 => SmartScopeImageFormat::Bgr888,
                            UsbCameraFormat::Mjpg => SmartScopeImageFormat::Mjpg,
                            UsbCameraFormat::Yuyv => SmartScopeImageFormat::Yuyv,
                        };
                        smartscope_frame.frame_id = usb_frame.frame_id;
                        smartscope_frame.timestamp = usb_frame.timestamp;

                        SmartScopeResult::Success
                    }
                    UsbCameraError::NoFrame => SmartScopeResult::ErrorNoFrame,
                    _ => SmartScopeResult::ErrorInvalidHandle,
                }
            } else {
                SmartScopeResult::ErrorInvalidHandle
            }
        } else {
            SmartScopeResult::ErrorInvalidHandle
        }
    }
}
