use crate::bindings::*;
use crate::error::{MppError, MppResult};
use std::ptr;

#[derive(Debug)]
pub struct MppPacket {
    packet: crate::bindings::MppPacket, // This is *mut c_void
}

impl MppPacket {
    pub fn from_ptr(packet: crate::bindings::MppPacket) -> Self {
        MppPacket { packet }
    }

    pub fn as_ptr(&self) -> crate::bindings::MppPacket {
        self.packet
    }

    pub fn new() -> MppResult<Self> {
        let mut packet = ptr::null_mut();
        unsafe {
            let ret = mpp_packet_init(&mut packet, ptr::null_mut(), 0);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(MppPacket { packet })
        }
    }

    pub fn from_data(data: &[u8]) -> MppResult<Self> {
        let mut packet = ptr::null_mut();
        unsafe {
            let ret = mpp_packet_init(&mut packet, data.as_ptr() as *mut libc::c_void, data.len());
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(MppPacket { packet })
        }
    }

    pub fn get_data(&self) -> MppResult<&[u8]> {
        unsafe {
            let data = mpp_packet_get_data(self.packet);
            if data.is_null() {
                return Err(MppError::Unknown("Packet has no data".to_string()));
            }

            let size = mpp_packet_get_size(self.packet);
            if size == 0 {
                return Err(MppError::Unknown("Packet has no data".to_string()));
            }

            let slice = std::slice::from_raw_parts(data as *const u8, size);
            Ok(slice)
        }
    }

    pub fn get_size(&self) -> MppResult<usize> {
        unsafe {
            let size = mpp_packet_get_size(self.packet);
            Ok(size)
        }
    }

    pub fn get_timestamp(&self) -> MppResult<i64> {
        unsafe {
            let timestamp = mpp_packet_get_pts(self.packet);
            Ok(timestamp)
        }
    }

    pub fn set_timestamp(&mut self, timestamp: i64) -> MppResult<()> {
        unsafe {
            mpp_packet_set_pts(self.packet, timestamp);
            Ok(())
        }
    }

    pub fn is_eos(&self) -> MppResult<bool> {
        unsafe {
            let eos = mpp_packet_get_eos(self.packet);
            Ok(eos != 0)
        }
    }

    pub fn set_eos(&mut self) -> MppResult<()> {
        unsafe {
            let ret = mpp_packet_set_eos(self.packet);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(())
        }
    }
}

impl Drop for MppPacket {
    fn drop(&mut self) {
        unsafe {
            let _ = mpp_packet_deinit(&mut self.packet as *mut _);
        }
    }
}

// 重新导出类型
// pub use crate::bindings::MppPacketType; // This type doesn't exist
