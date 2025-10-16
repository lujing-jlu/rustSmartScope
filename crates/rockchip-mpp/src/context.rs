use crate::bindings::*;
use crate::error::{MppError, MppResult};
use std::ptr;

#[derive(Debug)]
pub struct MppContext {
    ctx: MppCtx,
    api: *mut MppApi_t,
}

impl MppContext {
    pub fn new(ctx_type: MppCtxType, coding_type: MppCodingType) -> MppResult<Self> {
        let mut ctx = ptr::null_mut();
        let mut api = ptr::null_mut();

        unsafe {
            let ret = mpp_create(&mut ctx, &mut api);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }

            let ret = mpp_init(ctx, ctx_type, coding_type);
            if ret != 0 {
                mpp_destroy(ctx);
                return Err(MppError::MppError(ret));
            }
        }

        Ok(MppContext { ctx, api })
    }

    // --- Decoder path ---
    pub fn decode_put_packet(&self, packet: &crate::packet::MppPacket) -> MppResult<()> {
        unsafe {
            let decode_put = (*self.api).decode_put_packet;
            if decode_put.is_none() {
                return Err(MppError::NotSupported);
            }
            let ret = decode_put.unwrap()(self.ctx, packet.as_ptr());
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(())
        }
    }

    pub fn decode_get_frame(&self) -> MppResult<Option<crate::frame::MppFrame>> {
        unsafe {
            let decode_get = (*self.api).decode_get_frame;
            if decode_get.is_none() {
                return Err(MppError::NotSupported);
            }
            let mut frame = ptr::null_mut();
            let ret = decode_get.unwrap()(self.ctx, &mut frame);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            if frame.is_null() {
                return Ok(None);
            }
            Ok(Some(crate::frame::MppFrame::from_ptr(frame)))
        }
    }

    // --- Encoder path ---
    pub fn encode_put_frame(&self, frame: &crate::frame::MppFrame) -> MppResult<()> {
        unsafe {
            let encode_put = (*self.api).encode_put_frame;
            if encode_put.is_none() {
                return Err(MppError::NotSupported);
            }
            let ret = encode_put.unwrap()(self.ctx, frame.as_ptr());
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(())
        }
    }

    pub fn encode_get_packet(&self) -> MppResult<Option<crate::packet::MppPacket>> {
        unsafe {
            let encode_get = (*self.api).encode_get_packet;
            if encode_get.is_none() {
                return Err(MppError::NotSupported);
            }
            let mut packet = ptr::null_mut();
            let ret = encode_get.unwrap()(self.ctx, &mut packet);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            if packet.is_null() {
                return Ok(None);
            }
            Ok(Some(crate::packet::MppPacket::from_ptr(packet)))
        }
    }

    pub fn control(&self, cmd: MpiCmd, param: MppParam) -> MppResult<()> {
        unsafe {
            let control_fn = (*self.api).control;
            if control_fn.is_none() {
                return Err(MppError::NotSupported);
            }
            let ret = control_fn.unwrap()(self.ctx, cmd, param);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(())
        }
    }

    pub fn reset(&self) -> MppResult<()> {
        unsafe {
            let reset_fn = (*self.api).reset;
            if reset_fn.is_none() {
                return Err(MppError::NotSupported);
            }
            let ret = reset_fn.unwrap()(self.ctx);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(())
        }
    }
}

impl Drop for MppContext {
    fn drop(&mut self) {
        unsafe {
            mpp_destroy(self.ctx);
        }
    }
}

// 重新导出类型
pub use crate::bindings::{MpiCmd, MppApi_t, MppCodingType, MppCtx, MppCtxType, MppParam};
