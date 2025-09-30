//! 简单的linux-video测试程序
//! 演示基本的相机列表、格式查询和图像捕获功能

use std::{
    env,
    io::{self, Write},
    path::Path,
    time::{Duration, Instant},
};

use anyhow::{anyhow, Result};
use linuxvideo::{
    format::{PixFormat, PixelFormat},
    BufType, Device,
};

fn main() -> Result<()> {
    env_logger::init();
    
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 2 {
        println!("用法: {} <命令> [参数]", args[0]);
        println!("命令:");
        println!("  list                    - 列出所有视频设备");
        println!("  info <设备路径>         - 显示设备信息");
        println!("  capture <设备路径> <秒数> - 捕获视频流指定秒数");
        println!("  save <设备路径> <文件名> - 保存一张图片");
        return Ok(());
    }
    
    match args[1].as_str() {
        "list" => list_devices()?,
        "info" => {
            if args.len() < 3 {
                return Err(anyhow!("需要设备路径参数"));
            }
            show_device_info(&args[2])?;
        }
        "capture" => {
            if args.len() < 4 {
                return Err(anyhow!("需要设备路径和时间参数"));
            }
            let seconds: u64 = args[3].parse()?;
            capture_stream(&args[2], seconds)?;
        }
        "save" => {
            if args.len() < 4 {
                return Err(anyhow!("需要设备路径和文件名参数"));
            }
            save_image(&args[2], &args[3])?;
        }
        _ => {
            return Err(anyhow!("未知命令: {}", args[1]));
        }
    }
    
    Ok(())
}

/// 列出所有视频设备
fn list_devices() -> Result<()> {
    println!("=== 扫描视频设备 ===");
    
    for (index, res) in linuxvideo::list()?.enumerate() {
        match res {
            Ok(device) => {
                let caps = device.capabilities()?;
                let path = device.path().unwrap_or_else(|_| format!("设备{}", index).into());
                
                println!("{}. {}", index + 1, path.display());
                println!("   名称: {}", caps.card());
                println!("   驱动: {}", caps.driver());
                println!("   总线: {}", caps.bus_info());
                println!("   功能: {:?}", caps.device_capabilities());
                println!();
            }
            Err(e) => {
                eprintln!("跳过设备 {} (错误: {})", index + 1, e);
            }
        }
    }
    
    Ok(())
}

/// 显示设备详细信息
fn show_device_info(device_path: &str) -> Result<()> {
    println!("=== 设备信息: {} ===", device_path);
    
    let device = Device::open(Path::new(device_path))?;
    let caps = device.capabilities()?;
    
    println!("基本信息:");
    println!("  名称: {}", caps.card());
    println!("  驱动: {}", caps.driver());
    println!("  总线: {}", caps.bus_info());
    println!("  功能: {:?}", caps.device_capabilities());
    
    // 显示支持的格式
    println!("\n支持的像素格式:");
    for (index, res) in device.formats(BufType::VIDEO_CAPTURE).enumerate() {
        match res {
            Ok(format_desc) => {
                println!("  {}. {:?} - {}", 
                    index + 1, 
                    format_desc.pixel_format(), 
                    format_desc.description()
                );
            }
            Err(e) => {
                eprintln!("  格式错误: {}", e);
                break;
            }
        }
    }
    
    // 尝试获取当前格式
    if let Ok(current_format) = device.format(BufType::VIDEO_CAPTURE) {
        println!("\n当前格式: {:?}", current_format);
    }
    
    Ok(())
}

/// 捕获视频流
fn capture_stream(device_path: &str, seconds: u64) -> Result<()> {
    println!("=== 捕获视频流: {} ({}秒) ===", device_path, seconds);
    
    let device = Device::open(Path::new(device_path))?;
    
    // 尝试设置MJPG格式
    let format = PixFormat::new(1280, 720, PixelFormat::MJPG);
    let capture = device.video_capture(format)?;
    
    println!("协商格式: {:?}", capture.format());
    
    let mut stream = capture.into_stream()?;
    
    println!("开始捕获...");
    let start_time = Instant::now();
    let mut frame_count = 0;
    let mut last_fps_time = Instant::now();
    let mut last_frame_count = 0;
    
    while start_time.elapsed() < Duration::from_secs(seconds) {
        match stream.dequeue(|_buf| Ok(())) {
            Ok(()) => {
                frame_count += 1;
                print!(".");
                io::stdout().flush().ok();
                
                // 每秒显示一次FPS
                if last_fps_time.elapsed() >= Duration::from_secs(1) {
                    let fps = frame_count - last_frame_count;
                    println!(" {} FPS", fps);
                    last_fps_time = Instant::now();
                    last_frame_count = frame_count;
                }
            }
            Err(e) => {
                eprintln!("\n捕获错误: {}", e);
                break;
            }
        }
    }
    
    println!("\n捕获完成! 总帧数: {}", frame_count);
    let avg_fps = frame_count as f64 / start_time.elapsed().as_secs_f64();
    println!("平均FPS: {:.2}", avg_fps);
    
    Ok(())
}

/// 保存单张图像
fn save_image(device_path: &str, filename: &str) -> Result<()> {
    println!("=== 保存图像: {} -> {} ===", device_path, filename);
    
    let device = Device::open(Path::new(device_path))?;
    
    // 检查是否支持MJPG或JPEG
    let formats = device
        .formats(BufType::VIDEO_CAPTURE)
        .map(|res| res.map(|f| f.pixel_format()))
        .collect::<io::Result<Vec<_>>>()?;
    
    let format = if formats.contains(&PixelFormat::MJPG) {
        PixelFormat::MJPG
    } else if formats.contains(&PixelFormat::JPEG) {
        PixelFormat::JPEG
    } else {
        return Err(anyhow!("设备不支持JPEG格式 (支持的格式: {:?})", formats));
    };
    
    let capture = device.video_capture(PixFormat::new(u32::MAX, u32::MAX, format))?;
    println!("协商格式: {:?}", capture.format());
    
    let mut stream = capture.into_stream()?;
    println!("等待图像数据...");
    
    let mut file = std::fs::File::create(filename)?;
    stream.dequeue(|buf| {
        if buf.is_error() {
            eprintln!("警告: 缓冲区错误标志已设置");
        }
        file.write_all(&*buf)?;
        println!("写入 {} 字节到 {} (原始缓冲区大小: {} 字节)",
            buf.len(), filename, buf.raw_buffer().len());
        Ok(())
    })?;
    
    println!("图像保存成功!");
    Ok(())
}
