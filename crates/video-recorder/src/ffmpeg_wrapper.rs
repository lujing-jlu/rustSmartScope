//! Simple FFmpeg process wrapper for video recording

use crate::{RecorderError, Result, VideoFrame, RecorderConfig, VideoCodec, HardwareAccelType};
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

        log::info!(
            "Starting FFmpeg (software x264): {}x{} -> 1280x720",
            self.config.width, self.config.height
        );

        // 仅使用软件编码（libx264），并在管线内缩放到720p
        let args = build_ffmpeg_args_sw_720p(&self.config);
        let mut child = Command::new("ffmpeg")
            .args(args.iter().map(|s| s.as_str()))
            .stdin(Stdio::piped())
            .stderr(Stdio::inherit())
            .spawn()
            .map_err(RecorderError::Io)?;

        // 若 ffmpeg 很快退出（参数错误等），报错
        if let Ok(Some(status)) = child.try_wait() {
            if !status.success() {
                return Err(RecorderError::Io(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    format!("FFmpeg exited immediately: {:?}", status)
                )));
            }
        }

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

fn build_ffmpeg_args_sw_720p(cfg: &RecorderConfig) -> Vec<String> {
    let mut args: Vec<String> = Vec::new();
    // Input rawvideo over stdin (rgb24)
    args.push("-y".into());
    args.push("-f".into()); args.push("rawvideo".into());
    args.push("-vcodec".into()); args.push("rawvideo".into());
    args.push("-pix_fmt".into()); args.push("rgb24".into());
    args.push("-s".into()); args.push(format!("{}x{}", cfg.width, cfg.height));
    // 不强制固定输入帧率；为了兼容 rawvideo，需要给个合理的名义值，选 15
    args.push("-r".into()); args.push("15".into());
    args.push("-i".into()); args.push("-".into());

    // 缩放到720p + 转换到 yuv420p（保持通用）
    args.push("-vf".into()); args.push("scale=1280:720,format=yuv420p".into());

    // 软件编码 libx264
    args.push("-c:v".into()); args.push("libx264".into());
    args.push("-preset".into()); args.push("superfast".into());
    args.push("-tune".into()); args.push("zerolatency".into());
    args.push("-pix_fmt".into()); args.push("yuv420p".into());

    // 码率设置
    args.push("-b:v".into()); args.push(cfg.bitrate.to_string());

    args.push(cfg.output_path.clone());
    args
}

impl Drop for FFmpegRecorder {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}
