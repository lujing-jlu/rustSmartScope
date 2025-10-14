/// Image pixel format
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageFormat {
    Gray8 = 0,
    Rgb888 = 1,
    Rgba8888 = 2,
    Yuv420SpNv21 = 3,
    Yuv420SpNv12 = 4,
}

/// Image rectangle for bounding box
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct ImageRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl ImageRect {
    pub fn width(&self) -> i32 {
        self.right - self.left
    }

    pub fn height(&self) -> i32 {
        self.bottom - self.top
    }
}

/// Image buffer for input/output
#[derive(Debug, Clone)]
pub struct ImageBuffer {
    pub width: i32,
    pub height: i32,
    pub format: ImageFormat,
    pub data: Vec<u8>,
}

impl ImageBuffer {
    pub fn new(width: i32, height: i32, format: ImageFormat, data: Vec<u8>) -> Self {
        Self {
            width,
            height,
            format,
            data,
        }
    }

    /// Create an RGB888 image buffer
    pub fn from_rgb888(width: i32, height: i32, data: Vec<u8>) -> Self {
        Self::new(width, height, ImageFormat::Rgb888, data)
    }
}

/// Detection result from YOLOv8
#[derive(Debug, Clone, Copy)]
pub struct DetectionResult {
    pub bbox: ImageRect,
    pub confidence: f32,
    pub class_id: i32,
}

impl DetectionResult {
    pub fn new(bbox: ImageRect, confidence: f32, class_id: i32) -> Self {
        Self {
            bbox,
            confidence,
            class_id,
        }
    }
}
