//! Multi-threaded Adaptive Camera Stream Example
//!
//! This example demonstrates automatic camera mode detection with independent thread-based streaming:
//! - Each camera runs in its own dedicated thread for maximum FPS
//! - Dynamically creates/destroys threads based on camera availability
//! - Handles mode changes during runtime with proper thread cleanup
//! - No camera: no threads
//! - Single camera: one thread
//! - Stereo camera: two independent threads

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use crossbeam_channel::{bounded, Receiver, Sender};
use usb_camera::{
    config::CameraConfig,
    device::V4L2Device,
    monitor::{CameraMode, CameraStatusMonitor},
    stream::{CameraStreamReader, VideoFrame},
};

/// Frame statistics for a camera thread
#[derive(Debug, Clone)]
struct FrameStats {
    camera_name: String,
    frame_count: usize,
    last_frame_time: Option<Instant>,
    thread_id: String,
    thread_start_time: Instant, // 添加线程启动时间
    thread_duration: Duration,  // 添加线程运行时长
}

/// Camera thread controller
struct CameraThread {
    camera_name: String,
    thread_handle: Option<JoinHandle<()>>,
    shutdown_signal: Arc<AtomicBool>,
    stats_receiver: Receiver<FrameStats>,
    frame_receiver: Receiver<VideoFrame>,
}

impl CameraThread {
    /// Create and start a new camera thread
    fn new(device: &V4L2Device, config: &CameraConfig) -> Result<Self, Box<dyn std::error::Error>> {
        let camera_name = device.name.clone();
        let device_path = device.path.to_string_lossy().to_string();
        let config = config.clone();

        // Create shutdown signal
        let shutdown_signal = Arc::new(AtomicBool::new(false));
        let shutdown_clone = shutdown_signal.clone();

        // Create channels for communication
        let (stats_tx, stats_rx) = bounded::<FrameStats>(10);
        let (frame_tx, frame_rx) = bounded::<VideoFrame>(5);

        let camera_name_clone = camera_name.clone();

        // Spawn the camera thread
        let handle = thread::spawn(move || {
            Self::camera_thread_loop(
                camera_name_clone,
                device_path,
                config,
                shutdown_clone,
                stats_tx,
                frame_tx,
            );
        });

        println!("  🚀 [{}] 相机线程已启动", camera_name);

        Ok(Self {
            camera_name,
            thread_handle: Some(handle),
            shutdown_signal,
            stats_receiver: stats_rx,
            frame_receiver: frame_rx,
        })
    }

    /// Camera thread main loop
    fn camera_thread_loop(
        camera_name: String,
        device_path: String,
        config: CameraConfig,
        shutdown_signal: Arc<AtomicBool>,
        stats_sender: Sender<FrameStats>,
        frame_sender: Sender<VideoFrame>,
    ) {
        let thread_id = format!("{:?}", thread::current().id());
        println!("  📡 [{}] 线程 {} 开始运行", camera_name, thread_id);

        // Create camera stream reader
        let mut reader = CameraStreamReader::new(&device_path, &camera_name, config);

        match reader.start() {
            Ok(_) => {
                println!("  ✅ [{}] 流启动成功", camera_name);
            }
            Err(e) => {
                println!("  ❌ [{}] 流启动失败: {}", camera_name, e);
                return;
            }
        }

        let start_time = Instant::now();
        let mut frame_count = 0;
        let mut last_stats_time = Instant::now();
        let stats_interval = Duration::from_secs(1);

        // Main reading loop
        while !shutdown_signal.load(Ordering::Relaxed) {
            match reader.read_frame() {
                Ok(Some(frame)) => {
                    frame_count += 1;

                    // Extract frame info before moving
                    let frame_width = frame.width;
                    let frame_height = frame.height;
                    let frame_size = frame.data.len();
                    let frame_id = frame.frame_id;

                    // Send frame to main thread (non-blocking)
                    if frame_sender.try_send(frame).is_err() {
                        // Frame buffer full, skip this frame
                    }

                    // Log every N frames (after moving frame)
                    if frame_count <= 5 || frame_count % 30 == 0 {
                        println!(
                            "  📦 [{}] 帧 {}: {}x{}, {} KB, ID: {} (线程: {})",
                            camera_name,
                            frame_count,
                            frame_width,
                            frame_height,
                            frame_size / 1024,
                            frame_id,
                            thread_id
                        );
                    }

                    // Send stats periodically
                    if last_stats_time.elapsed() >= stats_interval {
                        let thread_duration = start_time.elapsed();
                        let stats = FrameStats {
                            camera_name: camera_name.clone(),
                            frame_count,
                            last_frame_time: Some(Instant::now()),
                            thread_id: thread_id.clone(),
                            thread_start_time: start_time,
                            thread_duration,
                        };

                        if stats_sender.try_send(stats).is_err() {
                            // Stats buffer full, skip
                        }

                        last_stats_time = Instant::now();
                    }
                }
                Ok(None) => {
                    // No frame available, short wait
                    thread::sleep(Duration::from_millis(1));
                }
                Err(e) => {
                    println!("  ❌ [{}] 读取帧失败: {}", camera_name, e);
                    thread::sleep(Duration::from_millis(10));
                }
            }
        }

        // Cleanup
        let _ = reader.stop();
        println!("  🛑 [{}] 线程 {} 已停止", camera_name, thread_id);
    }

    /// Get latest stats (non-blocking)
    fn try_get_stats(&self) -> Option<FrameStats> {
        self.stats_receiver.try_recv().ok()
    }

    /// Get latest frame (non-blocking)
    fn try_get_frame(&self) -> Option<VideoFrame> {
        self.frame_receiver.try_recv().ok()
    }

    /// Stop the camera thread
    fn stop(&mut self) {
        if let Some(handle) = self.thread_handle.take() {
            println!("  🛑 [{}] 发送停止信号", self.camera_name);
            self.shutdown_signal.store(true, Ordering::Relaxed);

            // Wait for thread to finish
            if let Err(e) = handle.join() {
                println!("  ⚠️  [{}] 线程停止时出现问题: {:?}", self.camera_name, e);
            } else {
                println!("  ✅ [{}] 线程已安全停止", self.camera_name);
            }
        }
    }
}

impl Drop for CameraThread {
    fn drop(&mut self) {
        self.stop();
    }
}

/// Multi-threaded stream manager
struct ThreadedStreamManager {
    left_thread: Option<CameraThread>,
    right_thread: Option<CameraThread>,
    single_thread: Option<CameraThread>,
    config: CameraConfig,
    start_time: Instant,

    // Accumulated statistics
    left_stats: Option<FrameStats>,
    right_stats: Option<FrameStats>,
    single_stats: Option<FrameStats>,
}

impl ThreadedStreamManager {
    fn new() -> Self {
        let config = CameraConfig {
            width: 1280,
            height: 720,
            framerate: 30,
            pixel_format: "MJPG".to_string(),
            parameters: HashMap::new(),
        };

        Self {
            left_thread: None,
            right_thread: None,
            single_thread: None,
            config,
            start_time: Instant::now(),
            left_stats: None,
            right_stats: None,
            single_stats: None,
        }
    }

    fn update_mode(&mut self, mode: &CameraMode, cameras: &[V4L2Device]) {
        // Stop all current threads
        self.stop_all_threads();

        // Reset start time when mode changes
        self.start_time = Instant::now();

        match mode {
            CameraMode::NoCamera => {
                println!("📵 无相机模式 - 停止所有线程");
            }
            CameraMode::SingleCamera => {
                if let Some(camera) = cameras.first() {
                    println!("📷 单相机模式 - 启动线程: {}", camera.name);
                    match CameraThread::new(camera, &self.config) {
                        Ok(thread) => {
                            self.single_thread = Some(thread);
                            println!("  ✅ 单相机线程启动成功");
                        }
                        Err(e) => {
                            println!("  ❌ 单相机线程启动失败: {}", e);
                        }
                    }
                }
            }
            CameraMode::StereoCamera => {
                println!("📷📷 立体相机模式 - 启动双线程");

                // Try to identify left and right cameras
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

                // If no specific left/right naming, use first two cameras
                let (left_cam, right_cam) = if left_camera.is_none() && right_camera.is_none() {
                    (cameras.get(0), cameras.get(1))
                } else {
                    (left_camera, right_camera)
                };

                // Start left camera thread
                if let Some(left) = left_cam {
                    match CameraThread::new(left, &self.config) {
                        Ok(thread) => {
                            println!("  ✅ 左相机线程启动成功: {}", left.name);
                            self.left_thread = Some(thread);
                        }
                        Err(e) => {
                            println!("  ❌ 左相机线程启动失败: {}", e);
                        }
                    }
                }

                // Start right camera thread
                if let Some(right) = right_cam {
                    match CameraThread::new(right, &self.config) {
                        Ok(thread) => {
                            println!("  ✅ 右相机线程启动成功: {}", right.name);
                            self.right_thread = Some(thread);
                        }
                        Err(e) => {
                            println!("  ❌ 右相机线程启动失败: {}", e);
                        }
                    }
                }
            }
        }
    }

    fn stop_all_threads(&mut self) {
        if let Some(mut thread) = self.left_thread.take() {
            println!("  🛑 停止左相机线程");
            thread.stop();
        }
        if let Some(mut thread) = self.right_thread.take() {
            println!("  🛑 停止右相机线程");
            thread.stop();
        }
        if let Some(mut thread) = self.single_thread.take() {
            println!("  🛑 停止单相机线程");
            thread.stop();
        }

        // Clear stats
        self.left_stats = None;
        self.right_stats = None;
        self.single_stats = None;
    }

    fn update_stats(&mut self) {
        // Update stats from all active threads
        if let Some(ref thread) = self.left_thread {
            if let Some(stats) = thread.try_get_stats() {
                self.left_stats = Some(stats);
            }
        }

        if let Some(ref thread) = self.right_thread {
            if let Some(stats) = thread.try_get_stats() {
                self.right_stats = Some(stats);
            }
        }

        if let Some(ref thread) = self.single_thread {
            if let Some(stats) = thread.try_get_stats() {
                self.single_stats = Some(stats);
            }
        }
    }

    fn print_stats(&self) {
        let overall_elapsed = self.start_time.elapsed();
        println!(
            "\n📊 线程统计 (总运行时长: {:.1}s):",
            overall_elapsed.as_secs_f64()
        );

        if let Some(ref stats) = self.left_stats {
            let fps = if stats.thread_duration.as_secs_f64() > 0.0 {
                stats.frame_count as f64 / stats.thread_duration.as_secs_f64()
            } else {
                0.0
            };
            println!(
                "  📷 左相机: {} 帧 ({:.1} FPS, 线程运行: {:.1}s, 线程: {})",
                stats.frame_count,
                fps,
                stats.thread_duration.as_secs_f64(),
                stats.thread_id
            );
        }

        if let Some(ref stats) = self.right_stats {
            let fps = if stats.thread_duration.as_secs_f64() > 0.0 {
                stats.frame_count as f64 / stats.thread_duration.as_secs_f64()
            } else {
                0.0
            };
            println!(
                "  📷 右相机: {} 帧 ({:.1} FPS, 线程运行: {:.1}s, 线程: {})",
                stats.frame_count,
                fps,
                stats.thread_duration.as_secs_f64(),
                stats.thread_id
            );
        }

        if let Some(ref stats) = self.single_stats {
            let fps = if stats.thread_duration.as_secs_f64() > 0.0 {
                stats.frame_count as f64 / stats.thread_duration.as_secs_f64()
            } else {
                0.0
            };
            println!(
                "  📷 单相机: {} 帧 ({:.1} FPS, 线程运行: {:.1}s, 线程: {})",
                stats.frame_count,
                fps,
                stats.thread_duration.as_secs_f64(),
                stats.thread_id
            );
        }

        if self.left_stats.is_none() && self.right_stats.is_none() && self.single_stats.is_none() {
            println!("  📵 当前无活跃线程");
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize logging
    env_logger::init();

    println!("🎥 Multi-threaded Adaptive Camera Stream Example");
    println!("{}", "=".repeat(60));
    println!("💡 这个例子使用独立线程实现高性能相机流处理:");
    println!("   - 无相机: 不创建线程");
    println!("   - 单相机: 创建一个独立线程");
    println!("   - 立体相机: 创建两个独立线程并行处理");
    println!("🧵 每个相机都在自己的线程中运行，最大化FPS性能");
    println!("📡 监控相机状态变化并动态管理线程...");
    println!("🛑 按 Ctrl+C 停止\n");

    // Set up shutdown signal
    let running = Arc::new(AtomicBool::new(true));
    let running_clone = running.clone();

    // Setup Ctrl+C handler
    ctrlc::set_handler(move || {
        println!("\n🛑 收到 Ctrl+C，正在优雅关闭所有线程...");
        running_clone.store(false, Ordering::Relaxed);
    })
    .expect("Error setting Ctrl+C handler");

    // Create camera status monitor
    let mut monitor = CameraStatusMonitor::new(1000); // Check every second
    monitor.start()?;
    println!("✅ 相机状态监控已启动");

    // Create threaded stream manager
    let mut stream_manager = ThreadedStreamManager::new();

    // Get initial camera status
    let initial_status = CameraStatusMonitor::get_current_status()?;
    let mut current_mode = initial_status.mode.clone();

    println!("📋 初始状态: {:?}", current_mode);
    stream_manager.update_mode(&current_mode, &initial_status.cameras);

    let mut last_stats_time = Instant::now();
    let stats_interval = Duration::from_secs(5);

    // Main loop
    while running.load(Ordering::Relaxed) {
        // Check for status updates
        if let Some(status) = monitor.try_get_status() {
            if status.mode != current_mode {
                println!("\n🔄 相机模式变化: {:?} -> {:?}", current_mode, status.mode);
                current_mode = status.mode.clone();
                stream_manager.update_mode(&current_mode, &status.cameras);
            }
        }

        // Update stats from threads
        stream_manager.update_stats();

        // Print stats periodically
        if last_stats_time.elapsed() >= stats_interval {
            stream_manager.print_stats();
            last_stats_time = Instant::now();
        }

        // Short sleep to prevent busy waiting
        thread::sleep(Duration::from_millis(100));
    }

    // Cleanup
    println!("\n🧹 正在清理所有线程...");
    stream_manager.stop_all_threads();
    monitor.stop()?;

    // Final stats
    stream_manager.print_stats();

    println!("\n🎉 多线程例子完成！");
    Ok(())
}
