//! Hardware-accelerated video decoder module
//!
//! This module provides MJPEG decoding using Rockchip MPP hardware acceleration
//! and format conversion using Rockchip RGA.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

use crossbeam_channel::{unbounded, Receiver, Sender};
use log::{debug, error, info, warn};
use serde::{Deserialize, Serialize};

use crate::error::{CameraError, CameraResult};
use crate::stream::VideoFrame;
use crate::stream::V4L2_PIX_FMT_MJPEG;
use crate::turbojpeg::{TurboJpegDecompressor, TJPF_BGRA};

// TODO: Re-export MPP and RGA types when they are properly implemented
// pub use rockchip_mpp::{MppBuffer, MppCodingType, MppContext, MppCtxType, MppFrame, MppPacket};
// pub use rockchip_rga::{RgaBuffer, RgaContext, RgaRect};

/// Decoded frame format options
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DecodedFormat {
    /// RGB888 format (24-bit)
    RGB888,
    /// BGR888 format (24-bit)
    BGR888,
    /// RGBA8888 format (32-bit with alpha)
    RGBA8888,
    /// BGRA8888 format (32-bit with alpha)
    BGRA8888,
    /// YUV420P format (planar)
    YUV420P,
    /// NV12 format (semi-planar)
    NV12,
}

/// Decoder priority levels
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DecodePriority {
    /// Low priority - may skip frames under load
    Low,
    /// Normal priority - balanced performance
    Normal,
    /// High priority - decode all frames
    High,
    /// Real-time priority - minimal latency
    RealTime,
}

/// Decoder configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DecoderConfig {
    /// Enable/disable decoding
    pub enabled: bool,
    /// Maximum input queue size
    pub max_queue_size: usize,
    /// Target output format
    pub target_format: DecodedFormat,
    /// Maximum resolution (width, height)
    pub max_resolution: (u32, u32),
    /// Enable frame caching
    pub enable_caching: bool,
    /// Cache size (number of frames)
    pub cache_size: usize,
    /// Default decode priority
    pub priority: DecodePriority,
}

impl Default for DecoderConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            max_queue_size: 50,
            target_format: DecodedFormat::RGB888,
            max_resolution: (1920, 1080),
            enable_caching: true,
            cache_size: 5,
            priority: DecodePriority::Normal,
        }
    }
}

/// Decoded video frame
#[derive(Debug, Clone)]
pub struct DecodedFrame {
    /// Decoded image data
    pub data: Vec<u8>,
    /// Frame width
    pub width: u32,
    /// Frame height
    pub height: u32,
    /// Pixel format
    pub format: DecodedFormat,
    /// Original frame timestamp
    pub timestamp: std::time::SystemTime,
    /// Camera name
    pub camera_name: String,
    /// Original frame ID
    pub frame_id: u64,
    /// Decode duration
    pub decode_duration: Duration,
}

/// Decode request
#[derive(Debug)]
pub struct DecodeRequest {
    /// Original video frame
    pub frame: VideoFrame,
    /// Decode priority
    pub priority: DecodePriority,
    /// Request ID for tracking
    pub request_id: u64,
    /// Request timestamp
    pub timestamp: Instant,
}

/// Decode result
#[derive(Debug)]
pub enum DecodeResult {
    /// Successfully decoded frame
    Success(DecodedFrame),
    /// Decode error with message
    Error(String),
    /// Frame was skipped (e.g., due to load)
    Skipped,
}

/// Hardware-accelerated decoder manager
pub struct DecoderManager {
    /// Configuration
    config: DecoderConfig,
    /// Request sender
    request_sender: Sender<DecodeRequest>,
    /// Request receiver (for worker thread)
    request_receiver: Option<Receiver<DecodeRequest>>,
    /// Result sender (for worker thread)
    result_sender: Option<Sender<DecodeResult>>,
    /// Result receiver
    result_receiver: Receiver<DecodeResult>,
    /// Worker thread handle
    worker_thread: Option<thread::JoinHandle<()>>,
    /// Running flag
    running: Arc<AtomicBool>,
    /// Request ID counter
    request_id_counter: Arc<Mutex<u64>>,
}

impl DecoderManager {
    /// Create a new decoder manager
    pub fn new(config: DecoderConfig) -> Self {
        let (request_sender, request_receiver) = unbounded();
        let (result_sender, result_receiver) = unbounded();
        let running = Arc::new(AtomicBool::new(false));

        Self {
            config,
            request_sender,
            request_receiver: Some(request_receiver),
            result_sender: Some(result_sender),
            result_receiver,
            worker_thread: None,
            running,
            request_id_counter: Arc::new(Mutex::new(0)),
        }
    }

    /// Start the decoder
    pub fn start(&mut self) -> CameraResult<()> {
        if self.running.load(Ordering::Relaxed) {
            return Err(CameraError::DeviceOperationFailed(
                "Decoder already running".to_string(),
            ));
        }

        self.running.store(true, Ordering::Relaxed);

        let config = self.config.clone();
        let request_receiver = self.request_receiver.take().ok_or_else(|| {
            CameraError::DeviceOperationFailed("Request receiver already taken".to_string())
        })?;
        let result_sender = self.result_sender.take().ok_or_else(|| {
            CameraError::DeviceOperationFailed("Result sender already taken".to_string())
        })?;
        let running = Arc::clone(&self.running);

        let worker_thread = thread::spawn(move || {
            if let Err(e) = Self::worker_thread(config, request_receiver, result_sender, running) {
                error!("Decoder worker thread failed: {}", e);
            }
        });

        self.worker_thread = Some(worker_thread);
        info!("Decoder manager started");
        Ok(())
    }

    /// Stop the decoder
    pub fn stop(&mut self) -> CameraResult<()> {
        if !self.running.load(Ordering::Relaxed) {
            return Ok(());
        }

        self.running.store(false, Ordering::Relaxed);

        if let Some(handle) = self.worker_thread.take() {
            if let Err(e) = handle.join() {
                warn!("Failed to join decoder worker thread: {:?}", e);
            }
        }

        info!("Decoder manager stopped");
        Ok(())
    }

    /// Submit a frame for decoding
    pub fn submit_frame(&self, frame: VideoFrame) -> Option<u64> {
        if !self.config.enabled || !self.running.load(Ordering::Relaxed) {
            return None;
        }

        let request_id = {
            let mut counter = self.request_id_counter.lock().ok()?;
            *counter += 1;
            *counter
        };

        let request = DecodeRequest {
            frame,
            priority: self.config.priority,
            request_id,
            timestamp: Instant::now(),
        };

        if self.request_sender.try_send(request).is_ok() {
            Some(request_id)
        } else {
            debug!("Decoder queue full, dropping frame");
            None
        }
    }

    /// Try to get a decode result (non-blocking)
    pub fn try_get_result(&self) -> Option<DecodeResult> {
        self.result_receiver.try_recv().ok()
    }

    /// Get decoder statistics
    pub fn get_stats(&self) -> DecoderStats {
        DecoderStats {
            queue_size: self.request_sender.len(),
            result_queue_size: self.result_receiver.len(),
            running: self.running.load(Ordering::Relaxed),
        }
    }
}

/// Decoder statistics
#[derive(Debug, Clone)]
pub struct DecoderStats {
    /// Current request queue size
    pub queue_size: usize,
    /// Current result queue size
    pub result_queue_size: usize,
    /// Whether decoder is running
    pub running: bool,
}

impl DecoderManager {
    /// Worker thread implementation
    fn worker_thread(
        config: DecoderConfig,
        request_receiver: Receiver<DecodeRequest>,
        result_sender: Sender<DecodeResult>,
        running: Arc<AtomicBool>,
    ) -> CameraResult<()> {
        info!("Starting decoder worker thread");

        // TODO: Initialize MPP decoder when properly implemented
        // let mut mpp_ctx = MppContext::new(1, 8)?;

        // TODO: Initialize RGA context when properly implemented
        // let rga_ctx = RgaContext::new()?;

        // Initialize TurboJPEG per-thread handle
        let tj = match TurboJpegDecompressor::new() {
            Ok(t) => t,
            Err(e) => {
                return Err(CameraError::DeviceOperationFailed(format!(
                    "Failed to init turbojpeg: {}",
                    e
                )));
            }
        };
        info!("TurboJPEG initialized");

        let mut stats = WorkerStats::new();
        let mut last_stats_report = Instant::now();

        while running.load(Ordering::Relaxed) {
            // Process decode requests
            match request_receiver.recv_timeout(Duration::from_millis(100)) {
                Ok(request) => {
                    let decode_start = Instant::now();

                    match Self::decode_frame(&config, request, &tj) {
                        Ok(decoded_frame) => {
                            stats.successful_decodes += 1;
                            stats.total_decode_time += decode_start.elapsed();

                            if result_sender
                                .try_send(DecodeResult::Success(decoded_frame))
                                .is_err()
                            {
                                debug!("Result queue full, dropping decoded frame");
                                stats.dropped_results += 1;
                            }
                        }
                        Err(e) => {
                            stats.failed_decodes += 1;
                            warn!("Decode failed: {}", e);

                            let _ = result_sender.try_send(DecodeResult::Error(e.to_string()));
                        }
                    }
                }
                Err(crossbeam_channel::RecvTimeoutError::Timeout) => {
                    // No requests, continue
                }
                Err(crossbeam_channel::RecvTimeoutError::Disconnected) => {
                    info!("Request channel disconnected, stopping worker");
                    break;
                }
            }

            // Report statistics periodically
            if last_stats_report.elapsed() > Duration::from_secs(10) {
                info!("Decoder stats: {:?}", stats);
                last_stats_report = Instant::now();
            }
        }

        info!("Decoder worker thread stopped");
        Ok(())
    }

    /// Decode a single frame (placeholder implementation)
    fn decode_frame(
        config: &DecoderConfig,
        request: DecodeRequest,
        tj: &TurboJpegDecompressor,
    ) -> CameraResult<DecodedFrame> {
        let decode_start = Instant::now();
        if request.frame.format != V4L2_PIX_FMT_MJPEG {
            return Err(CameraError::DeviceOperationFailed(
                "Only MJPEG frames supported by turbojpeg decoder".to_string(),
            ));
        }

        // Choose output pixel format: default to BGRA8888 for performance and Qt friendliness
        let (pf, bpp, out_format) = match config.target_format {
            DecodedFormat::BGRA8888 => (TJPF_BGRA, 4usize, DecodedFormat::BGRA8888),
            _ => (TJPF_BGRA, 4usize, DecodedFormat::BGRA8888),
        };

        // Allocate output buffer once per call (caller could add a cache if needed)
        // Use requested width/height as upper bound; tjDecompressHeader3 will return real size
        let mut out_buf =
            vec![0u8; request.frame.width as usize * request.frame.height as usize * bpp];
        let (w, h, used) = tj
            .decompress_to(&request.frame.data, &mut out_buf, pf)
            .map_err(|e| CameraError::DeviceOperationFailed(format!("turbojpeg: {}", e)))?;
        out_buf.truncate(used);

        let decode_duration = decode_start.elapsed();

        Ok(DecodedFrame {
            data: out_buf,
            width: w as u32,
            height: h as u32,
            format: out_format,
            timestamp: request.frame.timestamp,
            camera_name: request.frame.camera_name,
            frame_id: request.frame.frame_id,
            decode_duration,
        })
    }
}

/// Worker thread statistics
#[derive(Debug, Clone)]
struct WorkerStats {
    successful_decodes: u64,
    failed_decodes: u64,
    dropped_results: u64,
    total_decode_time: Duration,
}

impl WorkerStats {
    fn new() -> Self {
        Self {
            successful_decodes: 0,
            failed_decodes: 0,
            dropped_results: 0,
            total_decode_time: Duration::ZERO,
        }
    }
}

impl Drop for DecoderManager {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}
