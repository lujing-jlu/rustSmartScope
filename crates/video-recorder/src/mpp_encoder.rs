//! Rockchip MPP hardware encoder wrapper

use crate::{RecorderConfig, RecorderError, Result, VideoFrame};
use log;

pub struct MppEncoder {
    config: RecorderConfig,
    frame_count: u64,
    initialized: bool,
}

impl MppEncoder {
    pub fn new(config: RecorderConfig) -> Result<Self> {
        Ok(Self {
            config,
            frame_count: 0,
            initialized: false,
        })
    }

    pub fn start(&mut self) -> Result<()> {
        if self.initialized {
            return Err(RecorderError::AlreadyRunning);
        }

        // For now, we'll simulate MPP initialization
        // In a real implementation, you would:
        // 1. Create MPP context with MPP_CTX_ENC
        // 2. Set encoding parameters (resolution, bitrate, etc.)
        // 3. Configure hardware buffers

        log::info!("MPP encoder started: {}x{} @ {}fps, codec: {:?}",
                  self.config.width, self.config.height, self.config.fps, self.config.codec);

        self.initialized = true;
        Ok(())
    }

    pub fn encode_frame(&mut self, frame: &VideoFrame) -> Result<()> {
        if !self.initialized {
            return Err(RecorderError::NotStarted);
        }

        // Simulate encoding process
        // In reality, this would:
        // 1. Convert frame to MPP-compatible format (NV12/NV15 for Rockchip)
        // 2. Submit frame to MPP encoder
        // 3. Retrieve encoded packets
        // 4. Write to output file

        log::debug!("Encoding frame {}: {}x{} {:?}",
                   self.frame_count, frame.width, frame.height, frame.format);

        self.frame_count += 1;
        Ok(())
    }

    pub fn finish(&mut self) -> Result<()> {
        if !self.initialized {
            return Ok(());
        }

        log::info!("MPP encoder finished: {} frames encoded", self.frame_count);
        self.initialized = false;
        Ok(())
    }

    pub fn frame_count(&self) -> u64 {
        self.frame_count
    }

    /// Check if MPP hardware is available
    pub fn is_available() -> bool {
        // Check if MPP libraries and hardware are available
        std::path::Path::new("/dev/rga").exists() ||
        std::path::Path::new("/dev/mpp_service").exists()
    }
}

impl Drop for MppEncoder {
    fn drop(&mut self) {
        let _ = self.finish();
    }
}