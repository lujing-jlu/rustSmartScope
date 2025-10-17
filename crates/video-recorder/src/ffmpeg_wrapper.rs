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
            "Starting FFmpeg: {}x{} @ {}fps, codec={:?}, hw={:?}",
            self.config.width, self.config.height, self.config.fps, self.config.codec, self.config.hardware_accel
        );

        // 尝试按配置硬件编码启动，失败则自动回退到软件编码
        let try_spawn = |codec: VideoCodec, hw: HardwareAccelType| -> std::io::Result<Child> {
            let args = build_ffmpeg_args(&self.config, codec, hw);
            Command::new("ffmpeg")
                .args(args.iter().map(|s| s.as_str()))
                .stdin(Stdio::piped())
                .stderr(Stdio::inherit())
                .spawn()
        };

        // 1) 优先使用配置的硬件编码
        let mut child = match try_spawn(self.config.codec, self.config.hardware_accel) {
            Ok(c) => c,
            Err(e) => {
                log::warn!("FFmpeg spawn failed with hw={:?}: {}. Falling back to software encoder.", self.config.hardware_accel, e);
                // 2) 回退到软件编码
                try_spawn(self.config.codec, HardwareAccelType::None)
                    .map_err(RecorderError::Io)?
            }
        };

        // 若 ffmpeg 很快退出（编码器不可用等），也做回退检测
        if let Ok(Some(status)) = child.try_wait() {
            if !status.success() {
                log::warn!("FFmpeg exited immediately (hw={:?}, status={:?}). Falling back to software encoder.", self.config.hardware_accel, status);
                // 再尝试一次软件编码
                child = try_spawn(self.config.codec, HardwareAccelType::None)
                    .map_err(RecorderError::Io)?;
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

fn build_ffmpeg_args(cfg: &RecorderConfig, codec: VideoCodec, hw: HardwareAccelType) -> Vec<String> {
    let mut args: Vec<String> = Vec::new();
    // Input rawvideo over stdin
    args.push("-y".into());
    args.push("-f".into()); args.push("rawvideo".into());
    args.push("-vcodec".into()); args.push("rawvideo".into());
    args.push("-pix_fmt".into()); args.push("rgb24".into());
    args.push("-s".into()); args.push(format!("{}x{}", cfg.width, cfg.height));
    args.push("-r".into()); args.push(cfg.fps.to_string());
    args.push("-i".into()); args.push("-".into());

    match hw {
        HardwareAccelType::VaApi => {
            args.push("-vaapi_device".into());
            args.push("/dev/dri/renderD128".into());
            args.push("-vf".into());
            args.push("format=nv12,hwupload".into());
        }
        HardwareAccelType::RkMpp => { /* no-op */ }
        HardwareAccelType::None => {}
    }

    let (codec_name, add_sw_opts) = match (codec, hw) {
        (VideoCodec::H264, HardwareAccelType::RkMpp) => ("h264_rkmpp", false),
        (VideoCodec::H265, HardwareAccelType::RkMpp) => ("hevc_rkmpp", false),
        (VideoCodec::H264, HardwareAccelType::VaApi) => ("h264_vaapi", false),
        (VideoCodec::H265, HardwareAccelType::VaApi) => ("hevc_vaapi", false),
        (VideoCodec::H264, HardwareAccelType::None) => ("libx264", true),
        (VideoCodec::H265, HardwareAccelType::None) => ("libx265", true),
        (VideoCodec::VP8,  _) => ("libvpx", true),
        (VideoCodec::VP9,  _) => ("libvpx-vp9", true),
    };

    args.push("-c:v".into()); args.push(codec_name.into());

    if add_sw_opts {
        args.push("-preset".into()); args.push("superfast".into());
        args.push("-tune".into()); args.push("zerolatency".into());
        args.push("-pix_fmt".into()); args.push("yuv420p".into());
    }

    args.push("-b:v".into()); args.push(cfg.bitrate.to_string());
    args.push(cfg.output_path.clone());
    args
}

impl Drop for FFmpegRecorder {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}
