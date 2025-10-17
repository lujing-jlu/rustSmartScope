use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use crate::types::CDetection;

extern "C" {
    fn smartscope_ai_is_enabled() -> bool;
    fn smartscope_ai_try_get_latest_result(out: *mut CDetection, max_results: c_int) -> c_int;
}

type AiCb = extern "C" fn(*mut c_void, *const c_char);
type AiRawCb = extern "C" fn(*mut c_void, *const CDetection, c_int);

struct AiMonitor {
    stop: Arc<AtomicBool>,
    handle: Option<JoinHandle<()>>,
}

lazy_static::lazy_static! {
    static ref AI_CB: Mutex<Option<(usize, AiCb)>> = Mutex::new(None);
    static ref AI_RAW_CB: Mutex<Option<(usize, AiRawCb)>> = Mutex::new(None);
    static ref AI_MON: Mutex<Option<AiMonitor>> = Mutex::new(None);
}

fn start_monitor(max_fps: c_int) {
    let stop = Arc::new(AtomicBool::new(false));
    let stop_clone = stop.clone();
    let interval_ms = if max_fps > 0 { (1000 / max_fps.max(1)) as u64 } else { 1 };

    let handle = thread::spawn(move || {
        let mut last_tick = Instant::now();
        let mut last_empty = false;
        loop {
            if stop_clone.load(Ordering::Relaxed) { break; }

            let elapsed = last_tick.elapsed();
            if elapsed < Duration::from_millis(interval_ms) {
                thread::sleep(Duration::from_millis(interval_ms) - elapsed);
            }
            last_tick = Instant::now();

            if unsafe { !smartscope_ai_is_enabled() } { continue; }

            let mut buf: [CDetection; 128] = unsafe { std::mem::zeroed() };
            let n = unsafe { smartscope_ai_try_get_latest_result(buf.as_mut_ptr(), 128) };
            if n < 0 { continue; }

            if n == 0 && last_empty { continue; }

            if let Some((ctx_u, raw_fn)) = *AI_RAW_CB.lock().unwrap() {
                raw_fn(ctx_u as *mut c_void, buf.as_ptr(), n);
            } else if let Some((ctx_u, json_fn)) = *AI_CB.lock().unwrap() {
                let count = (n as usize).min(128);
                let mut s = String::with_capacity(64 + count * 64);
                s.push('[');
                for (i, d) in buf[..count].iter().enumerate() {
                    if i > 0 { s.push(','); }
                    s.push_str(&format!(
                        "{{\"left\":{},\"top\":{},\"right\":{},\"bottom\":{},\"confidence\":{},\"class_id\":{}}}",
                        d.left, d.top, d.right, d.bottom, d.confidence, d.class_id
                    ));
                }
                s.push(']');
                if let Ok(cstr) = CString::new(s) { json_fn(ctx_u as *mut c_void, cstr.as_ptr()); }
            }

            last_empty = n == 0;
        }
    });

    let mut mon = AI_MON.lock().unwrap();
    *mon = Some(AiMonitor { stop, handle: Some(handle) });
}

#[no_mangle]
pub extern "C" fn smartscope_ai_register_result_callback(ctx: *mut c_void, cb: Option<AiCb>, max_fps: c_int) {
    unsafe {
        smartscope_ai_unregister_result_callback(std::ptr::null_mut());
        smartscope_ai_unregister_result_callback_raw(std::ptr::null_mut());
    }
    if let Some(cb_fn) = cb {
        *AI_CB.lock().unwrap() = Some((ctx as usize, cb_fn));
        start_monitor(max_fps);
    }
}

#[no_mangle]
pub extern "C" fn smartscope_ai_unregister_result_callback(_ctx: *mut c_void) {
    if let Some(m) = AI_MON.lock().unwrap().take() {
        m.stop.store(true, Ordering::Relaxed);
        if let Some(h) = m.handle { let _ = h.join(); }
    }
    *AI_CB.lock().unwrap() = None;
}

#[no_mangle]
pub extern "C" fn smartscope_ai_register_result_callback_raw(ctx: *mut c_void, cb: Option<AiRawCb>, max_fps: c_int) {
    unsafe {
        smartscope_ai_unregister_result_callback(std::ptr::null_mut());
        smartscope_ai_unregister_result_callback_raw(std::ptr::null_mut());
    }
    if let Some(cb_fn) = cb {
        *AI_RAW_CB.lock().unwrap() = Some((ctx as usize, cb_fn));
        start_monitor(max_fps);
    }
}

#[no_mangle]
pub extern "C" fn smartscope_ai_unregister_result_callback_raw(_ctx: *mut c_void) {
    if let Some(m) = AI_MON.lock().unwrap().take() {
        m.stop.store(true, Ordering::Relaxed);
        if let Some(h) = m.handle { let _ = h.join(); }
    }
    *AI_RAW_CB.lock().unwrap() = None;
}
