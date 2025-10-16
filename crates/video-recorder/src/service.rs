//! Non-blocking recording service with worker thread

use crate::{RecorderConfig, RecorderError, RecorderStats, Result, VideoEncoder, VideoFrame};
use std::collections::VecDeque;
use std::sync::{Arc, Mutex, atomic::{AtomicBool, AtomicU64, Ordering}};
use std::thread::{self, JoinHandle};
use std::time::Duration;

pub struct RecordingService {
    config: RecorderConfig,
    frame_queue: Arc<Mutex<VecDeque<VideoFrame>>>,
    is_running: Arc<AtomicBool>,
    worker_thread: Option<JoinHandle<()>>,
    total_encoded: Arc<AtomicU64>,
    total_dropped: Arc<AtomicU64>,
}

impl RecordingService {
    pub fn new(config: RecorderConfig) -> Self {
        Self {
            config,
            frame_queue: Arc::new(Mutex::new(VecDeque::new())),
            is_running: Arc::new(AtomicBool::new(false)),
            worker_thread: None,
            total_encoded: Arc::new(AtomicU64::new(0)),
            total_dropped: Arc::new(AtomicU64::new(0)),
        }
    }

    pub fn start(&mut self) -> Result<()> {
        if self.is_running.load(Ordering::Relaxed) {
            return Err(RecorderError::AlreadyRunning);
        }

        self.is_running.store(true, Ordering::Relaxed);

        let config = self.config.clone();
        let frame_queue = Arc::clone(&self.frame_queue);
        let is_running = Arc::clone(&self.is_running);
        let total_encoded = Arc::clone(&self.total_encoded);

        // Spawn worker thread
        let handle = thread::spawn(move || {
            log::info!("Recording worker thread started");

            let mut encoder = match VideoEncoder::new(config) {
                Ok(e) => e,
                Err(e) => {
                    log::error!("Failed to create encoder: {}", e);
                    is_running.store(false, Ordering::Relaxed);
                    return;
                }
            };

            if let Err(e) = encoder.start() {
                log::error!("Failed to start encoder: {}", e);
                is_running.store(false, Ordering::Relaxed);
                return;
            }

            while is_running.load(Ordering::Relaxed) {
                // Get frame from queue
                let frame = {
                    let mut queue = frame_queue.lock().unwrap();
                    queue.pop_front()
                };

                if let Some(frame) = frame {
                    match encoder.encode_frame(&frame) {
                        Ok(_) => {
                            total_encoded.fetch_add(1, Ordering::Relaxed);
                        }
                        Err(e) => {
                            log::error!("Failed to encode frame: {}", e);
                        }
                    }
                } else {
                    // No frames available, sleep briefly
                    thread::sleep(Duration::from_millis(10));
                }
            }

            // Finish encoding
            if let Err(e) = encoder.finish() {
                log::error!("Failed to finish encoding: {}", e);
            }

            log::info!("Recording worker thread stopped");
        });

        self.worker_thread = Some(handle);

        log::info!("Recording service started: {}", self.config.output_path);

        Ok(())
    }

    pub fn submit_frame(&self, frame: VideoFrame) -> Result<()> {
        if !self.is_running.load(Ordering::Relaxed) {
            return Err(RecorderError::NotStarted);
        }

        let mut queue = self.frame_queue.lock().unwrap();

        // Drop oldest frame if queue is full
        if queue.len() >= self.config.max_queue_size {
            queue.pop_front();
            self.total_dropped.fetch_add(1, Ordering::Relaxed);
            log::warn!("Frame queue full, dropping oldest frame");
        }

        queue.push_back(frame);

        Ok(())
    }

    pub fn stop(&mut self) -> Result<()> {
        if !self.is_running.load(Ordering::Relaxed) {
            return Ok(());
        }

        log::info!("Stopping recording service...");

        self.is_running.store(false, Ordering::Relaxed);

        // Wait for worker thread to finish
        if let Some(handle) = self.worker_thread.take() {
            handle.join().map_err(|_| RecorderError::ThreadJoin)?;
        }

        // Clear queue
        self.frame_queue.lock().unwrap().clear();

        log::info!("Recording service stopped");

        Ok(())
    }

    pub fn stats(&self) -> RecorderStats {
        RecorderStats {
            frames_encoded: self.total_encoded.load(Ordering::Relaxed),
            frames_dropped: self.total_dropped.load(Ordering::Relaxed),
            bytes_written: 0, // TODO: track this
            duration_ms: 0,    // TODO: track this
            is_recording: self.is_running.load(Ordering::Relaxed),
        }
    }

    pub fn is_running(&self) -> bool {
        self.is_running.load(Ordering::Relaxed)
    }

    pub fn queue_size(&self) -> usize {
        self.frame_queue.lock().unwrap().len()
    }
}

impl Drop for RecordingService {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}
