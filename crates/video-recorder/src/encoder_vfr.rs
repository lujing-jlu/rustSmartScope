use crate::{Result, RecorderError, RecorderConfig, VideoFrame};
use ffmpeg_next as ffmpeg;

pub struct FfmpegVfrEncoder {
    octx: ffmpeg::format::context::Output,
    stream_index: usize,
    enc: ffmpeg::encoder::video::Video,
    scaler: ffmpeg::software::scaling::Context,
    in_w: u32,
    in_h: u32,
    time_base_src: ffmpeg::Rational, // 1/1_000_000 (microseconds)
    frame_count: u64,
}

impl FfmpegVfrEncoder {
    pub fn new(mut cfg: RecorderConfig) -> Result<Self> {
        // Force software path 720p
        cfg.width = 1280;
        cfg.height = 720;

        static INIT: std::sync::Once = std::sync::Once::new();
        INIT.call_once(|| {
            let _ = ffmpeg::init();
        });

        // Open output
        let mut octx = ffmpeg::format::output(&cfg.output_path)
            .map_err(|e| RecorderError::Io(std::io::Error::new(std::io::ErrorKind::Other, format!("{:?}", e))))?;

        // Find libx264
        let codec = ffmpeg::encoder::find(ffmpeg::codec::Id::H264)
            .ok_or_else(|| RecorderError::InvalidConfig("H264 codec not found".into()))?;

        // Add stream
        let mut stream = octx.add_stream(codec)?;
        let stream_index = stream.index();

        // Build encoder
        let mut ctx = ffmpeg::codec::context::Context::new().encoder().video()?;
        ctx.set_width(cfg.width);
        ctx.set_height(cfg.height);
        // VFR: use time_base = 1/1_000_000 (microseconds)
        let tb = ffmpeg::Rational::new(1, 1_000_000);
        ctx.set_time_base(tb);
        // Optional: nominal framerate not enforced
        // ctx.set_frame_rate(Some(ffmpeg::Rational::new(15, 1)));
        ctx.set_bit_rate(cfg.bitrate as usize);
        ctx.set_gop(2 * 15);
        ctx.set_format(ffmpeg::format::Pixel::YUV420P);

        let mut enc = ctx.open_as(codec)?;
        stream.set_parameters(&enc);

        // Write header
        octx.write_header()?;

        // Scaler RGB24 -> YUV420P 720p
        // Initial scaler will be created on first frame when we know source WxH
        let scaler = ffmpeg::software::scaling::Context::get(
            ffmpeg::format::Pixel::RGB24,
            cfg.width,
            cfg.height,
            ffmpeg::format::Pixel::YUV420P,
            cfg.width,
            cfg.height,
            ffmpeg::software::scaling::Flags::BILINEAR,
        )?;

        Ok(Self { octx, stream_index, enc, scaler, in_w: cfg.width, in_h: cfg.height, time_base_src: tb, frame_count: 0 })
    }

    pub fn start(&mut self) -> Result<()> { Ok(()) }

    pub fn encode(&mut self, frame: &VideoFrame) -> Result<()> {
        // (Re)create scaler if source size changed
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

        // Build src frame from RGB24 data with provided width/height
        let mut src = ffmpeg::frame::Video::new(ffmpeg::format::Pixel::RGB24, frame.width, frame.height);
        // Copy contiguous RGB data
        let line_size = frame.width as usize * 3;
        let dst = src.data_mut(0);
        let rows = std::cmp::min(frame.height as usize, dst.len() / line_size);
        let src_slice = &frame.data[..std::cmp::min(frame.data.len(), line_size * rows)];
        dst[..src_slice.len()].copy_from_slice(src_slice);

        // Convert to YUV420P with scaling to 720p preset
        let mut yuv = ffmpeg::frame::Video::new(ffmpeg::format::Pixel::YUV420P, self.enc.width(), self.enc.height());
        self.scaler.run(&src, &mut yuv)?;

        // VFR PTS in microseconds
        yuv.set_pts(Some(frame.timestamp_us));

        // send frame
        self.enc.send_frame(&yuv)?;

        // receive packets
        let mut pkt = ffmpeg::Packet::empty();
        while let Ok(()) = self.enc.receive_packet(&mut pkt) {
            pkt.set_stream(self.stream_index);
            // rescale from src tb to stream tb
            let st_tb = self.octx.stream(self.stream_index).unwrap().time_base();
            pkt.rescale_ts(self.time_base_src, st_tb);
            pkt.write_interleaved(&mut self.octx)?;
            self.frame_count += 1;
        }
        Ok(())
    }

    pub fn finish(&mut self) -> Result<()> {
        // flush encoder
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
                Err(ffmpeg::Error::Other { errno }) if errno == libc::EAGAIN => { break; }
                Err(ffmpeg::Error::Eof) => { break; }
                Err(e) => return Err(e.into()),
            }
        }
        self.octx.write_trailer()?;
        Ok(())
    }

    // Backward-compat alias
    pub fn encode_frame(&mut self, f: &VideoFrame) -> Result<()> { self.encode(f) }
}
