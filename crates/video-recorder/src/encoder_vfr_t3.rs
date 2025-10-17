//! In-process VFR encoder using ffmpeg-the-third (software x264, 720p)
//! NOTE: This module is staged and not wired as default yet.

use crate::{Result, RecorderError, RecorderConfig, VideoFrame};
use ffmpeg_the_third as ffmpeg;

pub struct FfmpegVfrEncoderT3 {
    octx: ffmpeg::format::context::Output,
    stream_index: usize,
    enc: ffmpeg::encoder::video::Video,
    scaler: ffmpeg::software::scaling::Context,
    in_w: u32,
    in_h: u32,
    time_base_src: ffmpeg::Rational,
}

impl FfmpegVfrEncoderT3 {
    pub fn new(mut cfg: RecorderConfig) -> Result<Self> {
        // Force software 720p target
        cfg.width = 1280;
        cfg.height = 720;

        static INIT: std::sync::Once = std::sync::Once::new();
        INIT.call_once(|| {
            let _ = ffmpeg::init();
        });

        let mut octx = ffmpeg::format::output(&cfg.output_path)
            .map_err(|e| RecorderError::Io(std::io::Error::new(std::io::ErrorKind::Other, format!("{:?}", e))))?;

        // H.264 codec
        let codec = ffmpeg::encoder::find(ffmpeg::codec::Id::H264)
            .ok_or_else(|| RecorderError::InvalidConfig("H264 codec not found".into()))?;

        // Add a stream
        let mut stream = octx.add_stream(codec)?;
        let stream_index = stream.index();

        // Build encoder context
        let mut ctx = ffmpeg::codec::context::Context::new().encoder().video()?;
        ctx.set_width(cfg.width);
        ctx.set_height(cfg.height);
        let tb = ffmpeg::Rational::new(1, 1_000_000); // use microseconds
        ctx.set_time_base(tb);
        ctx.set_bit_rate(cfg.bitrate as usize);
        ctx.set_gop(30);
        ctx.set_format(ffmpeg::format::Pixel::YUV420P);

        let mut enc = ctx.open_as(codec)?;
        stream.set_parameters(&enc);

        // Header
        octx.write_header()?;

        // Prepare scaler (initially identity 720p, updated on first frame if needed)
        let scaler = ffmpeg::software::scaling::Context::get(
            ffmpeg::format::Pixel::RGB24,
            cfg.width,
            cfg.height,
            ffmpeg::format::Pixel::YUV420P,
            cfg.width,
            cfg.height,
            ffmpeg::software::scaling::Flags::BILINEAR,
        )?;

        Ok(Self { octx, stream_index, enc, scaler, in_w: cfg.width, in_h: cfg.height, time_base_src: tb })
    }

    pub fn start(&mut self) -> Result<()> { Ok(()) }

    pub fn encode(&mut self, frame: &VideoFrame) -> Result<()> {
        // Recreate scaler if input size changed
        if self.in_w != frame.width || self.in_h != frame.height {
            self.scaler = ffmpeg::software::scaling::Context::get(
                ffmpeg::format::Pixel::RGB24,
                frame.width,
                frame.height,
                ffmpeg::format::Pixel::YUV420P,
                self.enc.width(),
                self.enc.height(),
                ffmpeg::software::scaling::Flags::BILINEAR,
            )?;
            self.in_w = frame.width;
            self.in_h = frame.height;
        }

        // Build source RGB frame
        let mut src = ffmpeg::frame::Video::new(ffmpeg::format::Pixel::RGB24, frame.width, frame.height);
        let row = frame.width as usize * 3;
        let dst = src.data_mut(0);
        let rows = std::cmp::min(frame.height as usize, dst.len() / row);
        let copy_len = std::cmp::min(frame.data.len(), row * rows);
        dst[..copy_len].copy_from_slice(&frame.data[..copy_len]);

        // Convert to YUV420P @720p
        let mut yuv = ffmpeg::frame::Video::new(ffmpeg::format::Pixel::YUV420P, self.enc.width(), self.enc.height());
        self.scaler.run(&src, &mut yuv)?;

        // Set PTS in microseconds (VFR)
        yuv.set_pts(Some(frame.timestamp_us));

        // Encode
        self.enc.send_frame(&yuv)?;
        let mut pkt = ffmpeg::Packet::empty();
        while let Ok(()) = self.enc.receive_packet(&mut pkt) {
            pkt.set_stream(self.stream_index);
            let st_tb = self.octx.stream(self.stream_index).unwrap().time_base();
            pkt.rescale_ts(self.time_base_src, st_tb);
            pkt.write_interleaved(&mut self.octx)?;
        }
        Ok(())
    }

    pub fn finish(&mut self) -> Result<()> {
        self.enc.send_eof()?;
        let mut pkt = ffmpeg::Packet::empty();
        loop {
            match self.enc.receive_packet(&mut pkt) {
                Ok(()) => {
                    pkt.set_stream(self.stream_index);
                    let st_tb = self.octx.stream(self.stream_index).unwrap().time_base();
                    pkt.rescale_ts(self.time_base_src, st_tb);
                    pkt.write_interleaved(&mut self.octx)?;
                }
                Err(ffmpeg::Error::Eof) => break,
                Err(ffmpeg::Error::Other { errno }) if errno == libc::EAGAIN => break,
                Err(e) => return Err(RecorderError::Io(std::io::Error::new(std::io::ErrorKind::Other, format!("{:?}", e)))),
            }
        }
        self.octx.write_trailer()?;
        Ok(())
    }

    // Backward-compat helper
    pub fn encode_frame(&mut self, f: &VideoFrame) -> Result<()> { self.encode(f) }
}

