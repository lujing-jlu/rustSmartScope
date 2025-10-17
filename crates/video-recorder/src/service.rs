//! 非阻塞录制服务：使用 crossbeam-channel 队列 + 工作线程

use crate::{RecorderConfig, RecorderError, RecorderStats, Result, VideoEncoder, VideoFrame};
use crossbeam_channel::{bounded, select, Receiver, Sender};
use std::sync::{atomic::{AtomicBool, AtomicU64, Ordering}, Arc};
use std::thread::{self, JoinHandle};

pub struct RecordingService {
    config: RecorderConfig,
    tx: Sender<VideoFrame>,
    // 克隆一个接收端用于丢弃最旧帧（非阻塞 try_recv）
    rx_for_drop: Receiver<VideoFrame>,
    // 工作线程使用的接收端在 start() 内部克隆
    is_running: Arc<AtomicBool>,
    worker_thread: Option<JoinHandle<()>>,
    total_encoded: Arc<AtomicU64>,
    total_dropped: Arc<AtomicU64>,
    // 关闭信号通道
    shutdown_tx: Option<Sender<()>>,
}

impl RecordingService {
    pub fn new(config: RecorderConfig) -> Self {
        let (tx, rx) = bounded::<VideoFrame>(config.max_queue_size);
        Self {
            config,
            tx,
            rx_for_drop: rx,
            is_running: Arc::new(AtomicBool::new(false)),
            worker_thread: None,
            total_encoded: Arc::new(AtomicU64::new(0)),
            total_dropped: Arc::new(AtomicU64::new(0)),
            shutdown_tx: None,
        }
    }

    pub fn start(&mut self) -> Result<()> {
        if self.is_running.load(Ordering::Relaxed) {
            return Err(RecorderError::AlreadyRunning);
        }

        self.is_running.store(true, Ordering::Relaxed);

        let config = self.config.clone();
        let rx = self.rx_for_drop.clone();
        let is_running = Arc::clone(&self.is_running);
        let total_encoded = Arc::clone(&self.total_encoded);

        // 关闭信号
        let (shutdown_tx, shutdown_rx) = bounded::<()>(1);
        self.shutdown_tx = Some(shutdown_tx);

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

            loop {
                select! {
                    recv(shutdown_rx) -> _ => {
                        break;
                    }
                    recv(rx) -> msg => {
                        match msg {
                            Ok(frame) => {
                                if let Err(e) = encoder.encode_frame(&frame) {
                                    log::error!("Failed to encode frame: {}", e);
                                } else {
                                    total_encoded.fetch_add(1, Ordering::Relaxed);
                                }
                            }
                            Err(_) => {
                                // Channel closed
                                break;
                            }
                        }
                    }
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

        // 尝试非阻塞发送；若队列满，则丢弃一个最旧帧再发送
        match self.tx.try_send(frame) {
            Ok(_) => {}
            Err(e) => {
                if e.is_full() {
                    // 丢弃最旧一帧
                    let _ = self.rx_for_drop.try_recv();
                    self.total_dropped.fetch_add(1, Ordering::Relaxed);
                    // 再尝试发送
                    if let Err(e2) = self.tx.try_send(e.into_inner()) {
                        return Err(RecorderError::InvalidConfig(format!("queue send failed: {}", e2)));
                    }
                } else {
                    return Err(RecorderError::InvalidConfig(format!("queue send failed: {}", e)));
                }
            }
        }

        Ok(())
    }

    pub fn stop(&mut self) -> Result<()> {
        if !self.is_running.load(Ordering::Relaxed) {
            return Ok(());
        }

        log::info!("Stopping recording service...");

        self.is_running.store(false, Ordering::Relaxed);
        if let Some(tx) = self.shutdown_tx.take() {
            let _ = tx.send(());
        }

        // Wait for worker thread to finish
        if let Some(handle) = self.worker_thread.take() {
            handle.join().map_err(|_| RecorderError::ThreadJoin)?;
        }

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
        self.rx_for_drop.len()
    }
}

impl Drop for RecordingService {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}
