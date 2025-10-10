use crate::bindings::*;

#[derive(Debug, Clone, Copy)]
pub struct RgaRect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

impl RgaRect {
    pub fn new(x: i32, y: i32, width: i32, height: i32) -> Self {
        RgaRect { x, y, width, height }
    }
    
    pub fn from_im_rect(rect: im_rect) -> Self {
        RgaRect {
            x: rect.x,
            y: rect.y,
            width: rect.width,
            height: rect.height,
        }
    }
    
    pub fn to_im_rect(&self) -> im_rect {
        im_rect {
            x: self.x,
            y: self.y,
            width: self.width,
            height: self.height,
        }
    }
}

pub struct RgaBuffer {
    buffer: rga_buffer_t,
}

impl RgaBuffer {
    pub fn new() -> Self {
        RgaBuffer {
            buffer: unsafe { std::mem::zeroed() }
        }
    }
    
    pub fn from_ptr(buffer: rga_buffer_t) -> Self {
        RgaBuffer { buffer }
    }
    
    pub fn as_ptr(&self) -> &rga_buffer_t {
        &self.buffer
    }
    
    pub fn as_mut_ptr(&mut self) -> &mut rga_buffer_t {
        &mut self.buffer
    }
    
    pub fn set_format(&mut self, format: i32) -> &mut Self {
        self.buffer.format = format;
        self
    }
    
    pub fn set_width(&mut self, width: i32) -> &mut Self {
        self.buffer.width = width;
        self
    }
    
    pub fn set_height(&mut self, height: i32) -> &mut Self {
        self.buffer.height = height;
        self
    }
    
    pub fn set_stride(&mut self, stride: i32) -> &mut Self {
        self.buffer.wstride = stride;
        self
    }
    
    pub fn set_height_stride(&mut self, height_stride: i32) -> &mut Self {
        self.buffer.hstride = height_stride;
        self
    }
    
    pub fn set_fd(&mut self, fd: i32) -> &mut Self {
        self.buffer.fd = fd;
        self
    }
    
    pub fn set_handle(&mut self, handle: rga_buffer_handle_t) -> &mut Self {
        self.buffer.handle = handle;
        self
    }
    
    pub fn set_vir_addr(&mut self, addr: *mut libc::c_void) -> &mut Self {
        self.buffer.vir_addr = addr;
        self
    }
    
    pub fn set_phy_addr(&mut self, addr: *mut libc::c_void) -> &mut Self {
        self.buffer.phy_addr = addr;
        self
    }
    
    pub fn get_format(&self) -> i32 {
        self.buffer.format
    }
    
    pub fn get_width(&self) -> i32 {
        self.buffer.width
    }
    
    pub fn get_height(&self) -> i32 {
        self.buffer.height
    }
    
    pub fn get_stride(&self) -> i32 {
        self.buffer.wstride
    }
    
    pub fn get_height_stride(&self) -> i32 {
        self.buffer.hstride
    }
    
    pub fn get_fd(&self) -> i32 {
        self.buffer.fd
    }
    
    pub fn get_handle(&self) -> rga_buffer_handle_t {
        self.buffer.handle
    }
    
    pub fn get_vir_addr(&self) -> *mut libc::c_void {
        self.buffer.vir_addr
    }
    
    pub fn get_phy_addr(&self) -> *mut libc::c_void {
        self.buffer.phy_addr
    }
}

// 重新导出格式常量
pub const RGA_FORMAT_RGBA_8888: i32 = _Rga_SURF_FORMAT_RK_FORMAT_RGBA_8888 as i32;
pub const RGA_FORMAT_RGBA_5551: i32 = _Rga_SURF_FORMAT_RK_FORMAT_RGBA_5551 as i32;
pub const RGA_FORMAT_RGBA_4444: i32 = _Rga_SURF_FORMAT_RK_FORMAT_RGBA_4444 as i32;
pub const RGA_FORMAT_RGB_888: i32 = _Rga_SURF_FORMAT_RK_FORMAT_RGB_888 as i32;

/// RGA图像格式枚举
#[derive(Debug, Clone, Copy)]
pub enum RgaFormat {
    Rgb888,
    Rgba8888,
    Rgba5551,
    Rgba4444,
}

impl RgaFormat {
    /// 转换为RGA格式常量
    pub fn to_rga_format(&self) -> u32 {
        match self {
            RgaFormat::Rgb888 => RGA_FORMAT_RGB_888 as u32,
            RgaFormat::Rgba8888 => RGA_FORMAT_RGBA_8888 as u32,
            RgaFormat::Rgba5551 => RGA_FORMAT_RGBA_5551 as u32,
            RgaFormat::Rgba4444 => RGA_FORMAT_RGBA_4444 as u32,
        }
    }
}
