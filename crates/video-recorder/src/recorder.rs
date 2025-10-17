//! External process screen recorder implementation

use crate::{RecorderError, Result};
use std::process::{Command, Child};
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};
use std::cell::UnsafeCell;
use chrono::Local;

/// Recording backend type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RecorderBackend {
    /// wf-recorder for Wayland
    WfRecorder,
    /// ffmpeg x11grab for X11
    FFmpegX11,
}

impl RecorderBackend {
    /// Detect available backend
    pub fn detect() -> Option<Self> {
        // Check for wf-recorder first (Wayland)
        if which::which("wf-recorder").is_ok() {
            log::info!("Detected Wayland backend: wf-recorder");
            return Some(RecorderBackend::WfRecorder);
        }

        // Check for ffmpeg (X11)
        if which::which("ffmpeg").is_ok() {
            log::info!("Detected X11 backend: ffmpeg");
            return Some(RecorderBackend::FFmpegX11);
        }

        log::error!("No recording backend available (wf-recorder or ffmpeg)");
        None
    }

    /// Get backend name
    pub fn name(&self) -> &'static str {
        match self {
            RecorderBackend::WfRecorder => "wf-recorder",
            RecorderBackend::FFmpegX11 => "ffmpeg",
        }
    }
}

/// Screen recorder using external processes
pub struct ScreenRecorder {
    backend: RecorderBackend,
    process: UnsafeCell<Option<Child>>,
    output_path: PathBuf,
    start_time: Option<Instant>,
    width: u32,
    height: u32,
    fps: u32,
}

impl ScreenRecorder {
    /// Create new recorder with auto-detected backend
    pub fn new() -> Result<Self> {
        let backend = RecorderBackend::detect()
            .ok_or_else(|| RecorderError::BackendNotAvailable(
                "No wf-recorder or ffmpeg found".to_string()
            ))?;

        Ok(Self {
            backend,
            process: UnsafeCell::new(None),
            output_path: PathBuf::new(),
            start_time: None,
            width: 1920,
            height: 1200,
            fps: 30,
        })
    }

    /// Set screen dimensions
    pub fn set_dimensions(&mut self, width: u32, height: u32) {
        self.width = width;
        self.height = height;
    }

    /// Set frame rate
    pub fn set_fps(&mut self, fps: u32) {
        self.fps = fps;
    }

    /// Generate output path in Videos directory
    pub fn generate_output_path(&self, root_dir: &str) -> PathBuf {
        let videos_dir = Path::new(root_dir).join("Videos");

        // Create Videos directory if not exists
        if !videos_dir.exists() {
            let _ = std::fs::create_dir_all(&videos_dir);
        }

        // Generate filename with timestamp
        let timestamp = Local::now().format("%Y%m%d_%H%M%S");
        let filename = format!("record_{}.mp4", timestamp);

        videos_dir.join(filename)
    }

    /// Start recording
    pub fn start(&mut self, output_path: PathBuf) -> Result<()> {
        if self.is_recording() {
            return Err(RecorderError::AlreadyRunning);
        }

        self.output_path = output_path;

        log::info!("Starting screen recording: backend={}, output={}, {}x{} @ {}fps",
                  self.backend.name(), self.output_path.display(),
                  self.width, self.height, self.fps);

        let child = match self.backend {
            RecorderBackend::WfRecorder => self.start_wf_recorder()?,
            RecorderBackend::FFmpegX11 => self.start_ffmpeg_x11()?,
        };

        unsafe {
            *self.process.get() = Some(child);
        }

        // Wait longer for ffmpeg to initialize and start capturing
        // ffmpeg needs time to probe the display and start encoding
        log::info!("Waiting for recording process to initialize...");
        std::thread::sleep(Duration::from_millis(3000));

        // Set start time after initialization
        self.start_time = Some(Instant::now());

        log::info!("Recording started successfully");
        Ok(())
    }

    /// Start wf-recorder process
    fn start_wf_recorder(&self) -> Result<Child> {
        Command::new("wf-recorder")
            .args(&[
                "-f", self.output_path.to_str().unwrap(),
                "-r", &self.fps.to_string(),
            ])
            .spawn()
            .map_err(RecorderError::Io)
    }

    /// Start ffmpeg x11grab process
    fn start_ffmpeg_x11(&self) -> Result<Child> {
        let display = std::env::var("DISPLAY").unwrap_or_else(|_| ":0".to_string());
        let video_size = format!("{}x{}", self.width, self.height);
        let fps_str = self.fps.to_string();

        Command::new("ffmpeg")
            .args(&[
                "-y",  // Overwrite
                "-f", "x11grab",
                "-video_size", &video_size,
                "-framerate", &fps_str,  // Input framerate (before -i)
                "-probesize", "10M",  // Larger probe size for faster startup
                "-i", &display,
                "-vf", &format!("fps={}", self.fps),  // Output framerate filter
                "-c:v", "libx264",
                "-preset", "ultrafast",
                "-pix_fmt", "yuv420p",
                "-crf", "23",
                self.output_path.to_str().unwrap(),
            ])
            .spawn()
            .map_err(RecorderError::Io)
    }

    /// Stop recording
    pub fn stop(&mut self) -> Result<()> {
        if !self.is_recording() {
            return Ok(());
        }

        log::info!("Stopping screen recording...");

        let process_opt = unsafe { (*self.process.get()).take() };
        if let Some(mut child) = process_opt {
            let output_path = self.output_path.clone();

            // Spawn background thread to handle graceful shutdown
            std::thread::spawn(move || {
                #[cfg(unix)]
                {
                    use nix::sys::signal::{self, Signal};
                    use nix::unistd::Pid;

                    let pid = Pid::from_raw(child.id() as i32);
                    log::info!("Sending SIGTERM to recording process (pid={})...", pid);
                    if let Err(e) = signal::kill(pid, Signal::SIGTERM) {
                        log::error!("Failed to send SIGTERM: {}", e);
                    }
                }

                #[cfg(not(unix))]
                {
                    let _ = child.kill();
                }

                // Wait for process to finish (no timeout, let it complete naturally)
                match child.wait() {
                    Ok(status) => {
                        log::info!("Recording process finished: {:?}", status);

                        // Check final file size
                        if let Ok(metadata) = std::fs::metadata(&output_path) {
                            log::info!("Final recording: {} ({} bytes)",
                                      output_path.display(), metadata.len());
                        }
                    }
                    Err(e) => {
                        log::error!("Failed to wait for recording process: {}", e);
                    }
                }
            });
        }

        self.start_time = None;

        // Give thread a moment to send the signal
        std::thread::sleep(Duration::from_millis(100));

        log::info!("Stop signal sent, recording will finish in background");

        Ok(())
    }

    /// Check if recording is active
    pub fn is_recording(&self) -> bool {
        unsafe {
            if let Some(ref mut child) = &mut *self.process.get() {
                // Check if process is still running
                match child.try_wait() {
                    Ok(Some(_)) => false,  // Process has exited
                    Ok(None) => true,      // Still running
                    Err(_) => false,
                }
            } else {
                false
            }
        }
    }

    /// Get elapsed recording time
    pub fn elapsed_seconds(&self) -> u64 {
        self.start_time
            .map(|t| t.elapsed().as_secs())
            .unwrap_or(0)
    }

    /// Get output path
    pub fn output_path(&self) -> &Path {
        &self.output_path
    }

    /// Get backend type
    pub fn backend(&self) -> RecorderBackend {
        self.backend
    }
}

impl Drop for ScreenRecorder {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}
