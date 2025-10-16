//! X11 screen capture implementation

use crate::{RecorderError, Result, VideoFrame, VideoPixelFormat};
use x11rb::connection::Connection;
use x11rb::protocol::xproto::*;
use x11rb::rust_connection::RustConnection;

/// X11 screen capturer
pub struct X11ScreenCapturer {
    conn: RustConnection,
    screen_num: usize,
    root: Window,
    width: u32,
    height: u32,
    frame_number: u64,
}

impl X11ScreenCapturer {
    /// Create a new X11 screen capturer
    pub fn new() -> Result<Self> {
        let (conn, screen_num) = RustConnection::connect(None)
            .map_err(|e| RecorderError::X11Connection(format!("{:?}", e)))?;

        let screen = &conn.setup().roots[screen_num];
        let root = screen.root;
        let width = screen.width_in_pixels as u32;
        let height = screen.height_in_pixels as u32;

        log::info!("X11 screen capturer initialized: {}x{}", width, height);

        Ok(Self {
            conn,
            screen_num,
            root,
            width,
            height,
            frame_number: 0,
        })
    }

    /// Create capturer for specific region
    pub fn with_region(x: i16, y: i16, width: u16, height: u16) -> Result<Self> {
        let (conn, screen_num) = RustConnection::connect(None)
            .map_err(|e| RecorderError::X11Connection(format!("{:?}", e)))?;

        let screen = &conn.setup().roots[screen_num];
        let root = screen.root;

        log::info!("X11 screen capturer initialized for region: {}x{} at ({}, {})",
                   width, height, x, y);

        Ok(Self {
            conn,
            screen_num,
            root,
            width: width as u32,
            height: height as u32,
            frame_number: 0,
        })
    }

    /// Get screen dimensions
    pub fn dimensions(&self) -> (u32, u32) {
        (self.width, self.height)
    }

    /// Capture a single frame from the screen
    pub fn capture_frame(&mut self) -> Result<VideoFrame> {
        // Get screen image
        let image = self.conn
            .get_image(
                ImageFormat::Z_PIXMAP,
                self.root,
                0,
                0,
                self.width as u16,
                self.height as u16,
                !0, // All planes
            )
            .map_err(|e| RecorderError::X11Capture(format!("{:?}", e)))?
            .reply()
            .map_err(|e| RecorderError::X11Capture(format!("{:?}", e)))?;

        // Convert image data to RGB888
        let data = image.data;
        let depth = image.depth;

        log::debug!("Captured frame {}: depth={}, data_len={}",
                   self.frame_number, depth, data.len());

        // Most X11 servers use 32-bit BGRA format
        let rgb_data = if depth == 24 || depth == 32 {
            self.convert_bgra_to_rgb(&data)
        } else {
            return Err(RecorderError::X11Capture(
                format!("Unsupported color depth: {}", depth)
            ));
        };

        let mut frame = VideoFrame::new(
            rgb_data,
            self.width,
            self.height,
            VideoPixelFormat::RGB888,
        );

        frame.frame_number = self.frame_number;
        self.frame_number += 1;

        Ok(frame)
    }

    /// Convert BGRA to RGB888
    fn convert_bgra_to_rgb(&self, bgra: &[u8]) -> Vec<u8> {
        let pixel_count = (self.width * self.height) as usize;
        let mut rgb = Vec::with_capacity(pixel_count * 3);

        // X11 typically returns BGRA (4 bytes per pixel)
        for i in 0..pixel_count {
            let offset = i * 4;
            if offset + 3 <= bgra.len() {
                rgb.push(bgra[offset + 2]); // R
                rgb.push(bgra[offset + 1]); // G
                rgb.push(bgra[offset]);     // B
            }
        }

        rgb
    }
}

impl Drop for X11ScreenCapturer {
    fn drop(&mut self) {
        log::info!("X11 screen capturer shutting down");
    }
}
