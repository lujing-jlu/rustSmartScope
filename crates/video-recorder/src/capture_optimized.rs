//! Optimized X11 screen capture implementation

use crate::{RecorderError, Result, VideoFrame, VideoPixelFormat};
use x11rb::connection::Connection;
use x11rb::protocol::xproto::*;
use x11rb::rust_connection::RustConnection;

/// Optimized X11 screen capturer
pub struct OptimizedScreenCapturer {
    conn: RustConnection,
    screen_num: usize,
    root: Window,
    width: u32,
    height: u32,
    frame_number: u64,
    // Performance optimizations
    capture_region: (u16, u16, u16, u16),  // x, y, width, height
    use_shm: bool,  // Use X11 SHM for faster capture
}

impl OptimizedScreenCapturer {
    /// Create optimized capturer with performance settings
    pub fn new() -> Result<Self> {
        let (conn, screen_num) = RustConnection::connect(None)
            .map_err(|e| RecorderError::X11Connection(format!("{:?}", e)))?;

        let screen = &conn.setup().roots[screen_num];
        let root = screen.root;
        let width = screen.width_in_pixels as u32;
        let height = screen.height_in_pixels as u32;

        // Use smaller region for better performance (1920x1080 instead of full screen)
        let capture_width = width.min(1920);
        let capture_height = height.min(1080);

        log::info!("Optimized X11 capturer: full {}x{}, capture {}x{}",
                  width, height, capture_width, capture_height);

        Ok(Self {
            conn,
            screen_num,
            root,
            width,
            height,
            frame_number: 0,
            capture_region: (0, 0, capture_width as u16, capture_height as u16),
            use_shm: false,  // TODO: Implement SHM support
        })
    }

    /// Create capturer for high performance (smaller region)
    pub fn for_high_fps() -> Result<Self> {
        let mut capturer = Self::new()?;
        // Use even smaller region for 30+ FPS
        capturer.capture_region = (0, 0, 1280, 720);  // 720p
        log::info!("High-FPS mode: 1280x720 capture region");
        Ok(capturer)
    }

    pub fn dimensions(&self) -> (u32, u32) {
        (self.capture_region.2 as u32, self.capture_region.3 as u32)
    }

    /// Fast frame capture with optimizations
    pub fn capture_frame(&mut self) -> Result<VideoFrame> {
        let (x, y, width, height) = self.capture_region;

        // Capture smaller region for better performance
        let image = self.conn
            .get_image(
                ImageFormat::Z_PIXMAP,
                self.root,
                x as i16,
                y as i16,
                width,
                height,
                !0, // All planes
            )
            .map_err(|e| RecorderError::X11Capture(format!("{:?}", e)))?
            .reply()
            .map_err(|e| RecorderError::X11Capture(format!("{:?}", e)))?;

        let data = image.data;
        let depth = image.depth;

        // Fast path: avoid unnecessary logging in production
        if log::log_enabled!(log::Level::Debug) {
            log::debug!("Captured frame {}: depth={}, data_len={}",
                       self.frame_number, depth, data.len());
        }

        // Optimized color conversion
        let rgb_data = if depth == 24 || depth == 32 {
            self.fast_bgra_to_rgb(&data, width as u32, height as u32)
        } else {
            return Err(RecorderError::X11Capture(
                format!("Unsupported color depth: {}", depth)
            ));
        };

        let mut frame = VideoFrame::new(
            rgb_data,
            width as u32,
            height as u32,
            VideoPixelFormat::RGB888,
        );

        frame.frame_number = self.frame_number;
        self.frame_number += 1;

        Ok(frame)
    }

    /// Optimized BGRA to RGB conversion with minimal allocations
    fn fast_bgra_to_rgb(&self, bgra: &[u8], width: u32, height: u32) -> Vec<u8> {
        let pixel_count = (width * height) as usize;
        let mut rgb = Vec::with_capacity(pixel_count * 3);

        // Pre-allocate exact size to avoid reallocations
        rgb.resize(pixel_count * 3, 0);

        // Optimized conversion loop
        for i in 0..pixel_count {
            let src_offset = i * 4;
            let dst_offset = i * 3;

            if src_offset + 3 < bgra.len() {
                // Direct memory writes for speed
                unsafe {
                    let dst_ptr = rgb.as_mut_ptr().add(dst_offset);
                    *dst_ptr = bgra[src_offset + 2];      // R
                    *dst_ptr.add(1) = bgra[src_offset + 1]; // G
                    *dst_ptr.add(2) = bgra[src_offset];     // B
                }
            }
        }

        rgb
    }

    /// Benchmark capture performance
    pub fn benchmark(&mut self, frames: usize) -> Result<f64> {
        println!("Benchmarking {} frames...", frames);

        let start = std::time::Instant::now();
        for _ in 0..frames {
            self.capture_frame()?;
        }
        let elapsed = start.elapsed();

        let fps = frames as f64 / elapsed.as_secs_f64();
        println!("Captured {} frames in {:.2}s = {:.1} FPS",
                 frames, elapsed.as_secs_f64(), fps);

        Ok(fps)
    }
}

impl Drop for OptimizedScreenCapturer {
    fn drop(&mut self) {
        log::info!("Optimized X11 capturer shutting down (captured {} frames)", self.frame_number);
    }
}