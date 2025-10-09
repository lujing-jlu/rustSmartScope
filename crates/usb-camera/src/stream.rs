//! High-performance V4L2 video streaming module
//!
//! This module provides an optimized V4L2 implementation with:
//! - Persistent device connections using linux-video
//! - Asynchronous frame reading
//! - Memory-efficient buffering
//! - Error recovery mechanisms

use std::collections::VecDeque;
use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use crossbeam_channel::{Receiver, Sender};
use log::{debug, error, info, warn};
use tokio::sync::mpsc;

// Import linux-video for V4L2 operations
use linuxvideo::{format::PixFormat, stream::ReadStream, Device as V4L2Device};

use crate::config::CameraConfig;
use crate::error::{CameraError, CameraResult};
use crate::monitor::{CameraStatus, StatusMessage};

// V4L2 pixel format constant
pub const V4L2_PIX_FMT_MJPEG: u32 = 0x47504A4D; // "MJPG"

/// Video frame structure
#[derive(Debug, Clone)]
pub struct VideoFrame {
    /// Raw frame data
    pub data: Vec<u8>,
    /// Frame width
    pub width: u32,
    /// Frame height
    pub height: u32,
    /// Pixel format
    pub format: u32,
    /// Frame timestamp
    pub timestamp: std::time::SystemTime,
    /// Camera name
    pub camera_name: String,
    /// Frame ID (for debugging)
    pub frame_id: u64,
}

/// High-performance camera stream reader
pub struct CameraStreamReader {
    /// Camera name
    camera_name: String,
    /// Device path
    device_path: String,
    /// Camera configuration
    config: CameraConfig,
    /// Running state
    running: Arc<AtomicBool>,
    /// Frame buffer
    frame_buffer: Arc<Mutex<VecDeque<VideoFrame>>>,
    /// Reading thread handle
    read_thread: Option<thread::JoinHandle<()>>,
    /// Frame receiver
    frame_receiver: Option<mpsc::Receiver<VideoFrame>>,
}

impl CameraStreamReader {
    /// Create a new camera stream reader
    pub fn new(device_path: &str, camera_name: &str, config: CameraConfig) -> Self {
        Self {
            camera_name: camera_name.to_string(),
            device_path: device_path.to_string(),
            config,
            running: Arc::new(AtomicBool::new(false)),
            frame_buffer: Arc::new(Mutex::new(VecDeque::new())),
            read_thread: None,
            frame_receiver: None,
        }
    }

    /// Start the stream reader
    pub fn start(&mut self) -> CameraResult<()> {
        if self.running.load(Ordering::Relaxed) {
            return Ok(());
        }

        info!("Starting camera stream reader for {}", self.camera_name);

        // Create async channel for frame communication (small capacity to minimize latency)
        // 使用1帧缓冲，强制立即消费最新帧，丢弃旧帧
        let (tx, rx) = mpsc::channel(1);
        self.frame_receiver = Some(rx);

        // Clone data for the reading thread
        let device_path = self.device_path.clone();
        let camera_name = self.camera_name.clone();
        let config = self.config.clone();
        let running = self.running.clone();
        let frame_buffer = self.frame_buffer.clone();

        // Start reading thread
        let handle = thread::spawn(move || {
            Self::continuous_capture_thread(
                device_path,
                camera_name,
                config,
                running,
                frame_buffer,
                Some(tx),
            );
        });

        self.read_thread = Some(handle);
        self.running.store(true, Ordering::Relaxed);

        info!("Camera stream reader started for {}", self.camera_name);
        Ok(())
    }

    /// Stop the stream reader
    pub fn stop(&mut self) -> CameraResult<()> {
        if !self.running.load(Ordering::Relaxed) {
            return Ok(());
        }

        info!("Stopping camera stream reader for {}", self.camera_name);

        self.running.store(false, Ordering::Relaxed);

        // Wait for thread to finish
        if let Some(handle) = self.read_thread.take() {
            if let Err(e) = handle.join() {
                error!("Failed to join reading thread: {:?}", e);
            }
        }

        // Clear buffer
        if let Ok(mut buffer) = self.frame_buffer.lock() {
            buffer.clear();
        }

        info!("Camera stream reader stopped for {}", self.camera_name);
        Ok(())
    }

    /// Read a frame (non-blocking)
    /// 总是返回最新的帧，丢弃所有积压的旧帧以最小化延迟
    pub fn read_frame(&mut self) -> CameraResult<Option<VideoFrame>> {
        if !self.running.load(Ordering::Relaxed) {
            return Ok(None);
        }

        let mut latest_frame = None;

        // 从async receiver读取所有可用帧，只保留最新的
        if let Some(ref mut receiver) = self.frame_receiver {
            loop {
                match receiver.try_recv() {
                    Ok(frame) => {
                        latest_frame = Some(frame); // 持续更新为最新帧
                    }
                    Err(mpsc::error::TryRecvError::Empty) => break,
                    Err(mpsc::error::TryRecvError::Disconnected) => {
                        self.frame_receiver = None;
                        break;
                    }
                }
            }
        }

        // 如果从async receiver获取到帧，直接返回
        if latest_frame.is_some() {
            return Ok(latest_frame);
        }

        // Fallback to buffer (也只取最新的)
        if let Ok(mut buffer) = self.frame_buffer.lock() {
            Ok(buffer.pop_back()) // pop_back而不是pop_front，获取最新的帧
        } else {
            Ok(None)
        }
    }

    /// Check if reader is running
    pub fn is_running(&self) -> bool {
        self.running.load(Ordering::Relaxed)
    }

    /// Get current buffer size
    pub fn buffer_size(&self) -> usize {
        self.frame_buffer.lock().unwrap().len()
    }

    /// Create a persistent V4L2 stream for continuous frame capture
    fn create_persistent_stream(
        device_path: &str,
        config: &CameraConfig,
    ) -> CameraResult<ReadStream> {
        debug!("Creating persistent stream for {}", device_path);

        // Open the V4L2 device
        let device = V4L2Device::open(Path::new(device_path)).map_err(|e| {
            error!("Failed to open device {}: {}", device_path, e);
            CameraError::DeviceOperationFailed(format!("Failed to open device: {}", e))
        })?;

        // Set up the pixel format - try MJPG first, fallback to YUYV
        let pixel_format = match config.pixel_format.as_str() {
            "MJPG" | "MJPEG" => linuxvideo::format::PixelFormat::MJPG,
            "YUYV" => linuxvideo::format::PixelFormat::YUYV,
            _ => linuxvideo::format::PixelFormat::MJPG, // Default to MJPG
        };

        let format = PixFormat::new(config.width, config.height, pixel_format);

        // Configure the device for video capture
        let capture = device.video_capture(format).map_err(|e| {
            error!(
                "Failed to configure video capture for {}: {}",
                device_path, e
            );
            CameraError::DeviceOperationFailed(format!("Failed to configure video capture: {}", e))
        })?;

        info!("Negotiated format: {:?}", capture.format());

        // Set frame rate
        let target_fps = config.framerate;
        let frame_interval = linuxvideo::Fract::new(1, target_fps);

        match capture.set_frame_interval(frame_interval) {
            Ok(actual_interval) => {
                let actual_fps = 1.0 / actual_interval.as_f32() as f64;
                info!(
                    "Set frame rate for {}: target={} FPS, actual={:.1} FPS",
                    device_path, target_fps, actual_fps
                );
            }
            Err(e) => {
                warn!(
                    "Failed to set frame rate for {}: {}, using default",
                    device_path, e
                );
            }
        }

        // Create a stream for reading frames
        let stream = capture.into_stream().map_err(|e| {
            error!("Failed to create stream for {}: {}", device_path, e);
            CameraError::DeviceOperationFailed(format!("Failed to create stream: {}", e))
        })?;

        Ok(stream)
    }

    /// Continuous frame capture thread using persistent stream
    fn continuous_capture_thread(
        device_path: String,
        camera_name: String,
        config: CameraConfig,
        running: Arc<AtomicBool>,
        frame_buffer: Arc<Mutex<VecDeque<VideoFrame>>>,
        frame_sender: Option<mpsc::Sender<VideoFrame>>,
    ) {
        info!("Starting continuous capture thread for {}", camera_name);

        let mut frame_id = 0u64;
        let mut stream_opt = None;
        let mut last_error_time = Instant::now();
        const ERROR_RETRY_INTERVAL: Duration = Duration::from_secs(1);

        while running.load(Ordering::Relaxed) {
            // Create or recreate stream if needed
            if stream_opt.is_none() {
                match Self::create_persistent_stream(&device_path, &config) {
                    Ok(stream) => {
                        info!("Persistent stream created for {}", camera_name);
                        stream_opt = Some(stream);
                    }
                    Err(e) => {
                        if last_error_time.elapsed() >= ERROR_RETRY_INTERVAL {
                            error!("Failed to create stream for {}: {}", camera_name, e);
                            last_error_time = Instant::now();
                        }
                        thread::sleep(Duration::from_millis(100));
                        continue;
                    }
                }
            }

            // Capture frame from persistent stream
            if let Some(ref mut stream) = stream_opt {
                // Reuse allocation to avoid per-frame reallocations; rough capacity heuristic for MJPEG
                static mut PREALLOC_CAP: usize = 0;
                let mut frame_data = Vec::with_capacity(unsafe {
                    if PREALLOC_CAP == 0 {
                        let est = (config.width as usize * config.height as usize) / 2;
                        PREALLOC_CAP = est.max(512 * 1024);
                    }
                    PREALLOC_CAP
                });

                // 在帧捕获开始时立即记录时间戳，而不是处理完成后
                let frame_timestamp = std::time::SystemTime::now();

                let capture_result = stream.dequeue(|buf| {
                    if buf.is_error() {
                        warn!("Buffer error flag set for {}", camera_name);
                    }
                    frame_data.extend_from_slice(&*buf);
                    Ok(())
                });

                match capture_result {
                    Ok(()) => {
                        if !frame_data.is_empty() {
                            frame_id += 1;
                            let frame = VideoFrame {
                                data: frame_data,
                                width: config.width,
                                height: config.height,
                                format: V4L2_PIX_FMT_MJPEG,
                                timestamp: frame_timestamp, // 使用捕获开始时的时间戳
                                camera_name: camera_name.clone(),
                                frame_id,
                            };

                            // Try to send to async channel without cloning; fallback to ring buffer
                            // 为了最小化延迟，如果通道满了就丢弃旧帧，只保留最新帧
                            if let Some(ref sender) = frame_sender {
                                match sender.try_send(frame) {
                                    Ok(()) => {}
                                    Err(mpsc::error::TrySendError::Full(_old_frame)) => {
                                        // 通道满了，丢弃旧帧（已经在通道里的），这个新帧也暂时丢弃
                                        // 这样可以确保下一帧能进入通道，保持最新画面
                                        debug!("Async channel full for {}, dropping old frame", camera_name);
                                    }
                                    Err(mpsc::error::TrySendError::Closed(_frame_back)) => {
                                        // Drop if receiver is closed
                                    }
                                }
                            } else {
                                // 后备缓冲区也只保留1帧
                                if let Ok(mut buffer) = frame_buffer.lock() {
                                    buffer.clear(); // 清空旧帧
                                    buffer.push_back(frame); // 只保留最新帧
                                }
                            }
                        }
                    }
                    Err(e) => {
                        if last_error_time.elapsed() >= ERROR_RETRY_INTERVAL {
                            error!("Frame capture failed for {}: {}", camera_name, e);
                            last_error_time = Instant::now();
                        }
                        // Reset stream on error
                        stream_opt = None;
                        thread::sleep(Duration::from_millis(10));
                    }
                }
            }
        }

        info!("Continuous capture thread stopped for {}", camera_name);
    }
}

impl Drop for CameraStreamReader {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}

/// Camera stream manager
pub struct CameraStreamManager {
    /// Status receiver
    status_receiver: Receiver<StatusMessage>,
    /// Stream sender
    stream_sender: Sender<StreamMessage>,
    /// Stream receiver
    stream_receiver: Receiver<StreamMessage>,
    /// Active stream readers
    stream_readers: Arc<Mutex<std::collections::HashMap<String, CameraStreamReader>>>,
    /// Manager thread
    thread_handle: Option<thread::JoinHandle<()>>,
    /// Running flag
    running: Arc<AtomicBool>,
    /// Stream configuration
    config: StreamConfig,
}

/// Stream configuration
#[derive(Debug, Clone)]
pub struct StreamConfig {
    /// Frame width
    pub width: u32,
    /// Frame height
    pub height: u32,
    /// Pixel format
    pub format: u32,
    /// Read interval in milliseconds
    pub read_interval_ms: u32,
}

/// Stream message types
#[derive(Debug)]
pub enum StreamMessage {
    /// New frame received
    Frame(VideoFrame),
    /// Status update
    StatusUpdate(CameraStatus),
    /// Error occurred
    Error(String),
}

impl CameraStreamManager {
    /// Create a new stream manager
    pub fn new(status_receiver: Receiver<StatusMessage>, config: StreamConfig) -> Self {
        let (stream_sender, stream_receiver) = crossbeam_channel::unbounded();

        Self {
            status_receiver,
            stream_sender,
            stream_receiver,
            stream_readers: Arc::new(Mutex::new(std::collections::HashMap::new())),
            thread_handle: None,
            running: Arc::new(AtomicBool::new(false)),
            config,
        }
    }

    /// Start the stream manager
    pub fn start(&mut self) -> CameraResult<()> {
        if self.running.load(Ordering::Relaxed) {
            return Ok(());
        }

        info!("Starting camera stream manager");

        let status_receiver = self.status_receiver.clone();
        let stream_sender = self.stream_sender.clone();
        let stream_readers = self.stream_readers.clone();
        let config = self.config.clone();
        let running = self.running.clone();

        let handle = thread::spawn(move || {
            if let Err(e) = Self::manager_loop(
                status_receiver,
                stream_sender,
                stream_readers,
                config,
                running,
            ) {
                error!("Stream manager loop failed: {}", e);
            }
        });

        self.thread_handle = Some(handle);
        self.running.store(true, Ordering::Relaxed);

        info!("Camera stream manager started");
        Ok(())
    }

    /// Stop the stream manager
    pub fn stop(&mut self) -> CameraResult<()> {
        if !self.running.load(Ordering::Relaxed) {
            return Ok(());
        }

        info!("Stopping camera stream manager");

        self.running.store(false, Ordering::Relaxed);

        // Stop all readers
        if let Ok(mut readers) = self.stream_readers.lock() {
            for (name, mut reader) in readers.drain() {
                if let Err(e) = reader.stop() {
                    warn!("Failed to stop reader {}: {}", name, e);
                }
            }
        }

        // Wait for manager thread
        if let Some(handle) = self.thread_handle.take() {
            if let Err(e) = handle.join() {
                error!("Failed to join manager thread: {:?}", e);
            }
        }

        info!("Camera stream manager stopped");
        Ok(())
    }

    /// Try to get a message
    pub fn try_get_message(&self) -> Option<StreamMessage> {
        self.stream_receiver.try_recv().ok()
    }

    /// Manager main loop
    fn manager_loop(
        status_receiver: Receiver<StatusMessage>,
        stream_sender: Sender<StreamMessage>,
        stream_readers: Arc<Mutex<std::collections::HashMap<String, CameraStreamReader>>>,
        config: StreamConfig,
        running: Arc<AtomicBool>,
    ) -> CameraResult<()> {
        let _last_status_check = Instant::now();

        while running.load(Ordering::Relaxed) {
            // Check for status updates
            if let Ok(StatusMessage::StatusUpdate(status)) = status_receiver.try_recv() {
                Self::handle_status_update(&status, &stream_readers, &stream_sender, &config)?;
            }

            // Forward frames from readers
            if let Ok(mut readers) = stream_readers.lock() {
                for (_camera_name, reader) in readers.iter_mut() {
                    if let Ok(Some(frame)) = reader.read_frame() {
                        let _ = stream_sender.send(StreamMessage::Frame(frame));
                    }
                }
            }

            // Small delay
            thread::sleep(Duration::from_millis(1));
        }

        Ok(())
    }

    /// Handle camera status updates
    fn handle_status_update(
        status: &CameraStatus,
        stream_readers: &Arc<Mutex<std::collections::HashMap<String, CameraStreamReader>>>,
        _stream_sender: &Sender<StreamMessage>,
        config: &StreamConfig,
    ) -> CameraResult<()> {
        let mut readers = stream_readers.lock().map_err(|_| {
            CameraError::DeviceOperationFailed("Failed to lock readers".to_string())
        })?;

        // Start readers for new cameras
        for camera in &status.cameras {
            if !readers.contains_key(&camera.name) {
                let mut reader = CameraStreamReader::new(
                    &camera.path.to_string_lossy(),
                    &camera.name,
                    CameraConfig {
                        width: config.width,
                        height: config.height,
                        framerate: 30,
                        pixel_format: "MJPG".to_string(),
                        parameters: std::collections::HashMap::new(),
                    },
                );

                if let Err(e) = reader.start() {
                    warn!("Failed to start reader for {}: {}", camera.name, e);
                } else {
                    readers.insert(camera.name.clone(), reader);
                    info!("Started reader for camera {}", camera.name);
                }
            }
        }

        // Stop readers for removed cameras
        let current_names: std::collections::HashSet<String> =
            status.cameras.iter().map(|c| c.name.clone()).collect();

        let to_remove: Vec<String> = readers
            .keys()
            .filter(|name| !current_names.contains(*name))
            .cloned()
            .collect();

        for name in to_remove {
            if let Some(mut reader) = readers.remove(&name) {
                let _ = reader.stop();
                info!("Stopped reader for camera {}", name);
            }
        }

        Ok(())
    }
}

impl Drop for CameraStreamManager {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}
