use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use crate::types::CDetection;

// Call into our own exported symbols
extern "C" {
    fn smartscope_ai_is_enabled() -> bool;
    fn smartscope_ai_try_get_latest_result(out: *mut CDetection, max_results: c_int) -> c_int;
}

type AiCb = extern "C" fn(*mut c_void, *const c_char);

struct AiMonitor {
    stop: Arc<AtomicBool>,
    handle: Option<JoinHandle<()>>,
}

lazy_static::lazy_static! {
    static ref AI_CB: Mutex<Option<(usize, AiCb)>> = Mutex::new(None);
    static ref AI_MON: Mutex<Option<AiMonitor>> = Mutex::new(None);
}

#[no_mangle]
pub extern "C" fn smartscope_ai_register_result_callback(ctx: *mut c_void, cb: Option<AiCb>, max_fps: c_int) {
    // Stop any existing monitor
    unsafe { smartscope_ai_unregister_result_callback(std::ptr::null_mut()); }

    if cb.is_none() { return; }
    let cb = cb.unwrap();
    {
        let mut guard = AI_CB.lock().unwrap();
        *guard = Some((ctx as usize, cb));
    }

    let stop = Arc::new(AtomicBool::new(false));
    let stop_clone = stop.clone();
    // max_fps <= 0 means "as fast as possible"; use 1ms minimum to avoid busy loop
    let interval_ms = if max_fps > 0 { (1000 / max_fps.max(1)) as u64 } else { 1 };

    let handle = thread::spawn(move || {
        let mut last_tick = Instant::now();
        // We push per available result; empty result clears overlay
        loop {
            if stop_clone.load(Ordering::Relaxed) { break; }

            // throttle
            let elapsed = last_tick.elapsed();
            if elapsed < Duration::from_millis(interval_ms) {
                thread::sleep(Duration::from_millis(interval_ms) - elapsed);
            }
            last_tick = Instant::now();

            let enabled = unsafe { smartscope_ai_is_enabled() };
            if !enabled { continue; }

            let mut buf: [CDetection; 128] = unsafe { std::mem::zeroed() };
            let n = unsafe { smartscope_ai_try_get_latest_result(buf.as_mut_ptr(), 128) };
            if n < 0 { continue; }

            // Compose JSON (empty array means "clear overlay")
            let count = (n as usize).min(128);
            let mut s = String::with_capacity(64 + count * 64);
            s.push('[');
            for (i, d) in buf[..count].iter().enumerate() {
                if i > 0 { s.push(','); }
                // Bare minimum payload; labels rendered at UI if needed
                s.push_str(&format!(
                    "{{\"left\":{},\"top\":{},\"right\":{},\"bottom\":{},\"confidence\":{},\"class_id\":{}}}",
                    d.left, d.top, d.right, d.bottom, d.confidence, d.class_id
                ));
            }
            s.push(']');

            if let Some((ctx_u, func)) = *AI_CB.lock().unwrap() {
                if let Ok(cstr) = CString::new(s) {
                    func(ctx_u as *mut c_void, cstr.as_ptr());
                }
            }
        }
    });

    let mut mon = AI_MON.lock().unwrap();
    *mon = Some(AiMonitor { stop, handle: Some(handle) });
}

#[no_mangle]
pub extern "C" fn smartscope_ai_unregister_result_callback(_ctx: *mut c_void) {
    if let Some(m) = AI_MON.lock().unwrap().take() {
        m.stop.store(true, Ordering::Relaxed);
        if let Some(h) = m.handle { let _ = h.join(); }
    }
    *AI_CB.lock().unwrap() = None;
}
