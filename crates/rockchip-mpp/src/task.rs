use crate::bindings::*;
use crate::error::{MppError, MppResult};
use std::ptr;

#[derive(Debug)]
pub struct MppTask {
    task: MppTask,
}

impl MppTask {
    pub fn from_ptr(task: MppTask) -> Self {
        MppTask { task }
    }
    
    pub fn as_ptr(&self) -> MppTask {
        self.task
    }
    
    pub fn new() -> MppResult<Self> {
        let mut task = ptr::null_mut();
        unsafe {
            let ret = mpp_task_init(&mut task);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(MppTask { task })
        }
    }
    
    pub fn set_input(&mut self, packet: &crate::packet::MppPacket) -> MppResult<()> {
        unsafe {
            let ret = mpp_task_meta_set_packet(self.task, MPP_META_KEY_INPUT_PACKET, packet.as_ptr());
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(())
        }
    }
    
    pub fn set_output(&mut self, frame: &crate::frame::MppFrame) -> MppResult<()> {
        unsafe {
            let ret = mpp_task_meta_set_frame(self.task, MPP_META_KEY_OUTPUT_FRAME, frame.as_ptr());
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(())
        }
    }
    
    pub fn get_input(&self) -> MppResult<crate::packet::MppPacket> {
        unsafe {
            let mut packet = ptr::null_mut();
            let ret = mpp_task_meta_get_packet(self.task, MPP_META_KEY_INPUT_PACKET, &mut packet);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            if packet.is_null() {
                return Err(MppError::Unknown("Task has no input packet".to_string()));
            }
            Ok(crate::packet::MppPacket::from_ptr(packet))
        }
    }
    
    pub fn get_output(&self) -> MppResult<crate::frame::MppFrame> {
        unsafe {
            let mut frame = ptr::null_mut();
            let ret = mpp_task_meta_get_frame(self.task, MPP_META_KEY_OUTPUT_FRAME, &mut frame);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            if frame.is_null() {
                return Err(MppError::Unknown("Task has no output frame".to_string()));
            }
            Ok(crate::frame::MppFrame::from_ptr(frame))
        }
    }
    
    pub fn set_cmd(&mut self, cmd: u32, param: *mut libc::c_void) -> MppResult<()> {
        unsafe {
            let ret = mpp_task_meta_set_s32(self.task, MPP_META_KEY_COMMAND, cmd as i32);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(())
        }
    }
    
    pub fn get_cmd(&self) -> MppResult<u32> {
        unsafe {
            let mut cmd = 0i32;
            let ret = mpp_task_meta_get_s32(self.task, MPP_META_KEY_COMMAND, &mut cmd, 0);
            if ret != 0 {
                return Err(MppError::MppError(ret));
            }
            Ok(cmd as u32)
        }
    }
}

impl Drop for MppTask {
    fn drop(&mut self) {
        unsafe {
            mpp_task_deinit(&mut self.task);
        }
    }
}
