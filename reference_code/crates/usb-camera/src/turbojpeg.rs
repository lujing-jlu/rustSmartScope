use libc::{c_char, c_int, c_ulong, c_void};

// Minimal FFI for libturbojpeg we need

#[allow(non_camel_case_types)]
type tjhandle = *mut c_void;

// Pixel formats
pub const TJPF_RGB: c_int = 0;
pub const TJPF_BGR: c_int = 1;
pub const TJPF_RGBA: c_int = 2;
pub const TJPF_BGRA: c_int = 3;

// Flags
pub const TJFLAG_FASTDCT: c_int = 2048;
pub const TJFLAG_FASTUPSAMPLE: c_int = 256;

#[link(name = "turbojpeg")]
extern "C" {
    fn tjInitDecompress() -> tjhandle;
    fn tjDestroy(handle: tjhandle) -> c_int;

    fn tjDecompressHeader3(
        handle: tjhandle,
        jpegBuf: *const u8,
        jpegSize: c_ulong,
        width: *mut c_int,
        height: *mut c_int,
        jpegSubsamp: *mut c_int,
        colorspace: *mut c_int,
    ) -> c_int;

    fn tjDecompress2(
        handle: tjhandle,
        jpegBuf: *const u8,
        jpegSize: c_ulong,
        dstBuf: *mut u8,
        width: c_int,
        pitch: c_int,
        height: c_int,
        pixelFormat: c_int,
        flags: c_int,
    ) -> c_int;

    fn tjGetErrorStr2(handle: tjhandle) -> *const c_char;
}

pub struct TurboJpegDecompressor {
    handle: tjhandle,
}

impl TurboJpegDecompressor {
    pub fn new() -> Result<Self, String> {
        let handle = unsafe { tjInitDecompress() };
        if handle.is_null() {
            return Err("tjInitDecompress failed".to_string());
        }
        Ok(Self { handle })
    }

    pub fn decompress_to(
        &self,
        jpeg: &[u8],
        dst: &mut [u8],
        desired_pf: c_int,
    ) -> Result<(i32, i32, usize), String> {
        let mut w: c_int = 0;
        let mut h: c_int = 0;
        let mut subsamp: c_int = 0;
        let mut _cs: c_int = 0;
        let rc = unsafe {
            tjDecompressHeader3(
                self.handle,
                jpeg.as_ptr(),
                jpeg.len() as c_ulong,
                &mut w,
                &mut h,
                &mut subsamp,
                &mut _cs,
            )
        };
        if rc != 0 {
            return Err(self.last_error());
        }

        let bpp: usize = match desired_pf {
            TJPF_RGB | TJPF_BGR => 3,
            TJPF_RGBA | TJPF_BGRA => 4,
            _ => 3,
        };
        let pitch = (w as usize * bpp) as c_int;
        let need = w as usize * h as usize * bpp;
        if dst.len() < need {
            return Err(format!("dst too small: need {} have {}", need, dst.len()));
        }
        let flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;
        let rc = unsafe {
            tjDecompress2(
                self.handle,
                jpeg.as_ptr(),
                jpeg.len() as c_ulong,
                dst.as_mut_ptr(),
                w,
                pitch,
                h,
                desired_pf,
                flags,
            )
        };
        if rc != 0 {
            return Err(self.last_error());
        }
        Ok((w as i32, h as i32, need))
    }

    fn last_error(&self) -> String {
        unsafe {
            let c = tjGetErrorStr2(self.handle);
            if c.is_null() {
                return "turbojpeg error".to_string();
            }
            let s = std::ffi::CStr::from_ptr(c);
            s.to_string_lossy().into_owned()
        }
    }
}

impl Drop for TurboJpegDecompressor {
    fn drop(&mut self) {
        unsafe {
            let _ = tjDestroy(self.handle);
        }
    }
}
