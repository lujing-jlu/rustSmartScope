//! Simple FFmpeg process wrapper for video recording

use crate::{RecorderError, Result, VideoFrame, RecorderConfig};
use std::process::{Command, Child, Stdio};
use std::io::Write;

/// FFmpeg process wrapper for recording video
pub struct FFmpegRecorder {
    child: Option<Child>,
    config: RecorderConfig,
    is_recording: bool,
    frames_sent: u64,
}

impl FFmpegRecorder {
    /// Create new FFmpeg recorder
    pub fn new(config: RecorderConfig) -> Result<Self> {
        Ok(Self {
            child: None,
            config,
            is_recording: false,
            frames_sent: 0,
        })
    }

    /// Start FFmpeg process
    pub fn start(&mut self) -> Result<()> {
        if self.is_recording {
            return Err(RecorderError::AlreadyRunning);
        }

        log::info!("Starting FFmpeg for recording: {}x{} @ {}fps",
                  self.config.width, self.config.height, self.config.fps);

        let child = Command::new("ffmpeg")
            .args(&[
                "-y",  // Overwrite output
                "-f", "rawvideo",
                "-vcodec", "rawvideo",
                "-pix_fmt", "rgb24",
                "-s", &format!("{}x{}", self.config.width, self.config.height),
                "-r", &self.config.fps.to_string(),
                "-i", "-",  // Read from stdin
                "-c:v", "libx264",
                "-preset", "superfast",
                "-tune", "zerolatency",
                "-pix_fmt", "yuv420p",
                "-b:v", &self.config.bitrate.to_string(),
                &self.config.output_path,
            ])
            .stdin(Stdio::piped())
            .stderr(Stdio::inherit())
            .spawn()
            .map_err(|e| RecorderError::Io(e))?;

        self.child = Some(child);
        self.is_recording = true;
        self.frames_sent = 0;

        log::info!("FFmpeg started, output: {}", self.config.output_path);
        Ok(())
    }

    /// Backward-compat: alias to `start()` used by older service code
    #[inline]
    pub fn initialize(&mut self) -> Result<()> { self.start() }

    /// Send frame to FFmpeg
    pub fn send_frame(&mut self, frame: &VideoFrame) -> Result<()> {
        if !self.is_recording {
            return Err(RecorderError::NotStarted);
        }

        if let Some(ref mut child) = self.child {
            if let Some(ref mut stdin) = child.stdin {
                stdin.write_all(&frame.data)
                    .map_err(RecorderError::Io)?;
                self.frames_sent += 1;
            }
        }

        Ok(())
    }

    /// Backward-compat: alias to `send_frame()` used by older service code
    #[inline]
    pub fn encode_frame(&mut self, frame: &VideoFrame) -> Result<()> { self.send_frame(frame) }

    /// Stop recording and wait for FFmpeg to finish
    pub fn stop(&mut self) -> Result<()> {
        if !self.is_recording {
            return Ok(());
        }

        log::info!("Stopping FFmpeg recording...");

        if let Some(mut child) = self.child.take() {
            // Close stdin to signal EOF
            if let Some(stdin) = child.stdin.take() {
                drop(stdin);
            }

            // Wait for FFmpeg to finish
            match child.wait() {
                Ok(status) => {
                    if status.success() {
                        log::info!("FFmpeg completed successfully, {} frames sent", self.frames_sent);
                    } else {
                        log::error!("FFmpeg exited with status: {:?}", status);
                        return Err(RecorderError::Io(std::io::Error::new(
                            std::io::ErrorKind::Other,
                            format!("FFmpeg failed: {:?}", status)
                        )));
                    }
                }
                Err(e) => {
                    log::error!("Failed to wait for FFmpeg: {}", e);
                    return Err(RecorderError::Io(e));
                }
            }
        }

        self.is_recording = false;

        // Check if output file was created
        if std::path::Path::new(&self.config.output_path).exists() {
            if let Ok(metadata) = std::fs::metadata(&self.config.output_path) {
                log::info!("Output file size: {} bytes", metadata.len());
            }
        } else {
            log::error!("Output file not created: {}", self.config.output_path);
            return Err(RecorderError::Io(std::io::Error::new(
                std::io::ErrorKind::NotFound,
                "Output file not created"
            )));
        }

        Ok(())
    }

    /// Backward-compat: alias to `stop()` used by older service code
    #[inline]
    pub fn finish(&mut self) -> Result<()> { self.stop() }

    /// Check if recording is active
    pub fn is_recording(&self) -> bool {
        self.is_recording
    }

    /// Get number of frames sent
    pub fn frames_sent(&self) -> u64 {
        self.frames_sent
    }
}

impl Drop for FFmpegRecorder {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}
