//! SmartScope Core V4L2分辨率检测
//!
//! 使用V4L2 API查询相机支持的所有MJPEG分辨率
//!
//! 运行方式：
//! ```bash
//! cargo run --example detect_resolutions
//! ```

use std::fs::File;
use std::os::unix::io::AsRawFd;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("=== V4L2相机分辨率检测 ===");

    // 检测左相机
    println!("\n检测左相机 (/dev/video3):");
    detect_camera_formats("/dev/video3")?;

    // 检测右相机
    println!("\n检测右相机 (/dev/video1):");
    detect_camera_formats("/dev/video1")?;

    Ok(())
}

fn detect_camera_formats(device_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    let file = File::open(device_path)?;
    let fd = file.as_raw_fd();

    println!("设备: {}", device_path);

    // 获取设备信息
    let mut cap = v4l2_capability::default();
    unsafe {
        if v4l2_ioctl(fd, VIDIOC_QUERYCAP, &mut cap as *mut _ as *mut _) < 0 {
            return Err("无法查询设备能力".into());
        }
    }

    let driver = std::str::from_utf8(&cap.driver).unwrap_or("unknown").trim_end_matches('\0');
    let card = std::str::from_utf8(&cap.card).unwrap_or("unknown").trim_end_matches('\0');
    println!("驱动: {}, 设备: {}", driver, card);

    // 枚举支持的格式
    println!("\n支持的格式:");
    let mut fmt_desc = v4l2_fmtdesc::default();
    fmt_desc.type_ = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt_desc.index = 0;

    loop {
        unsafe {
            if v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &mut fmt_desc as *mut _ as *mut _) < 0 {
                break;
            }
        }

        let description = std::str::from_utf8(&fmt_desc.description).unwrap_or("unknown").trim_end_matches('\0');
        let pixelformat = fourcc_to_string(fmt_desc.pixelformat);

        println!("  格式 {}: {} ({})", fmt_desc.index, description, pixelformat);

        // 如果是MJPEG格式，枚举支持的分辨率
        if fmt_desc.pixelformat == v4l2_fourcc(b'M', b'J', b'P', b'G') {
            println!("    MJPEG支持的分辨率:");
            enumerate_frame_sizes(fd, fmt_desc.pixelformat)?;
        }

        fmt_desc.index += 1;
    }

    Ok(())
}

fn enumerate_frame_sizes(fd: i32, pixelformat: u32) -> Result<(), Box<dyn std::error::Error>> {
    let mut frame_size = v4l2_frmsizeenum::default();
    frame_size.pixel_format = pixelformat;
    frame_size.index = 0;

    let mut resolutions = Vec::new();

    loop {
        unsafe {
            if v4l2_ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &mut frame_size as *mut _ as *mut _) < 0 {
                break;
            }
        }

        match frame_size.type_ {
            V4L2_FRMSIZE_TYPE_DISCRETE => {
                let width = frame_size.discrete.width;
                let height = frame_size.discrete.height;
                let pixels = width * height;
                resolutions.push((width, height, pixels));
                println!("      {}×{} ({} 像素)", width, height, pixels);
            }
            V4L2_FRMSIZE_TYPE_STEPWISE => {
                println!("      步进模式: {}×{} - {}×{}, 步长: {}×{}",
                    frame_size.stepwise.min_width, frame_size.stepwise.min_height,
                    frame_size.stepwise.max_width, frame_size.stepwise.max_height,
                    frame_size.stepwise.step_width, frame_size.stepwise.step_height);
            }
            V4L2_FRMSIZE_TYPE_CONTINUOUS => {
                println!("      连续模式: {}×{} - {}×{}",
                    frame_size.stepwise.min_width, frame_size.stepwise.min_height,
                    frame_size.stepwise.max_width, frame_size.stepwise.max_height);
            }
            _ => {}
        }

        frame_size.index += 1;
    }

    // 找到最高分辨率
    if !resolutions.is_empty() {
        resolutions.sort_by_key(|(_, _, pixels)| *pixels);
        let (max_width, max_height, max_pixels) = resolutions.last().unwrap();
        println!("    ✓ 最高分辨率: {}×{} ({} 像素)", max_width, max_height, max_pixels);
    }

    Ok(())
}

fn fourcc_to_string(fourcc: u32) -> String {
    let bytes = fourcc.to_le_bytes();
    String::from_utf8_lossy(&bytes).to_string()
}

fn v4l2_fourcc(a: u8, b: u8, c: u8, d: u8) -> u32 {
    (a as u32) | ((b as u32) << 8) | ((c as u32) << 16) | ((d as u32) << 24)
}

// V4L2 常量和结构体定义
const VIDIOC_QUERYCAP: u64 = 0x80685600;
const VIDIOC_ENUM_FMT: u64 = 0xc0405602;
const VIDIOC_ENUM_FRAMESIZES: u64 = 0xc02c561a;

const V4L2_BUF_TYPE_VIDEO_CAPTURE: u32 = 1;
const V4L2_FRMSIZE_TYPE_DISCRETE: u32 = 1;
const V4L2_FRMSIZE_TYPE_CONTINUOUS: u32 = 2;
const V4L2_FRMSIZE_TYPE_STEPWISE: u32 = 3;

#[repr(C)]
#[derive(Default)]
struct v4l2_capability {
    driver: [u8; 16],
    card: [u8; 32],
    bus_info: [u8; 32],
    version: u32,
    capabilities: u32,
    device_caps: u32,
    reserved: [u32; 3],
}

#[repr(C)]
#[derive(Default)]
struct v4l2_fmtdesc {
    index: u32,
    type_: u32,
    flags: u32,
    description: [u8; 32],
    pixelformat: u32,
    reserved: [u32; 4],
}

#[repr(C)]
#[derive(Default)]
struct v4l2_frmsizeenum {
    index: u32,
    pixel_format: u32,
    type_: u32,
    discrete: v4l2_frmsize_discrete,
    stepwise: v4l2_frmsize_stepwise,
    reserved: [u32; 2],
}

#[repr(C)]
#[derive(Default)]
struct v4l2_frmsize_discrete {
    width: u32,
    height: u32,
}

#[repr(C)]
#[derive(Default)]
struct v4l2_frmsize_stepwise {
    min_width: u32,
    max_width: u32,
    step_width: u32,
    min_height: u32,
    max_height: u32,
    step_height: u32,
}

extern "C" {
    fn ioctl(fd: i32, request: u64, ...) -> i32;
}

unsafe fn v4l2_ioctl(fd: i32, request: u64, arg: *mut std::os::raw::c_void) -> i32 {
    ioctl(fd, request, arg)
}