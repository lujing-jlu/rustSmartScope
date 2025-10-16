//! Simple and reliable FFmpeg encoder

use crate::{RecorderConfig, RecorderError, Result, VideoFrame, VideoPixelFormat};
use ffmpeg_next as ffmpeg;
use std::path::Path;

pub struct SimpleEncoder {
    octx: Option<ffmpeg::format::context::Output>,
    encoder: Option<ffmpeg::codec::encoder::video::Video>,
    frame_count: u64,
    config: RecorderConfig,
}

impl SimpleEncoder {
    pub fn new(config: RecorderConfig) -> Result<Self> {
        Ok(Self {
            octx: None,
            encoder: None,
            frame_count: 0,
            config,
        })
    }

    pub fn start(&mut self) -> Result<()> {
        if self.octx.is_some() {
            return Err(RecorderError::AlreadyRunning);
        }

        // Initialize FFmpeg
        ffmpeg::init().map_err(RecorderError::FFmpeg)?;

        // Create output file
        let path = Path::new(&self.config.output_path);
        let mut octx = ffmpeg::format::output(&path)
            .map_err(RecorderError::FFmpeg)?;

        // Find libx264 encoder (most reliable)
        let codec = ffmpeg::encoder::find_by_name("libx264")
            .or_else(|| ffmpeg::encoder::find(ffmpeg::codec::Id::H264))
            .ok_or_else(|| RecorderError::FFmpeg(
                ffmpeg::Error::EncoderNotFound
            ))?;

        log::info!("Using encoder: {}", codec.name());

        // Add video stream
        let mut ost = octx.add_stream(codec)
            .map_err(RecorderError::FFmpeg)?;
        let stream_index = ost.index();

        // Configure encoder
        let mut encoder = ffmpeg::codec::context::Context::from_parameters(ost.parameters())
            .map_err(RecorderError::FFmpeg)?
            .encoder()
            .video()
            .map_err(RecorderError::FFmpeg)?;

        encoder.set_width(self.config.width);
        encoder.set_height(self.config.height);
        encoder.set_format(ffmpeg::format::Pixel::YUV420P);
        encoder.set_time_base((1, self.config.fps as i32));
        encoder.set_frame_rate(Some((self.config.fps as i32, 1)));
        encoder.set_bit_rate(self.config.bitrate as usize);

        // Open encoder with preset settings
        encoder.open()
            .map_err(RecorderError::FFmpeg)?;

        ost.set_parameters(&encoder);

        // Write file header
        octx.write_header()
            .map_err(RecorderError::FFmpeg)?;

        log::info!(
            "Simple encoder started: {}x{} @ {}fps, bitrate: {}",
            self.config.width,
            self.config.height,
            self.config.fps,
            self.config.bitrate
        );

        self.octx = Some(octx);
        // Convert context to encoder
        use ffmpeg_next::codec::encoder::video::Video;
        let video_encoder = Video::try_from(encoder).map_err(|e| RecorderError::FFmpeg(e))?;
        self.encoder = Some(video_encoder);
        self.frame_count = 0;

        Ok(())
    }

    pub fn encode_frame(&mut self, frame: &VideoFrame) -> Result<()> {
        let encoder = self.encoder.as_mut().ok_or(RecorderError::NotStarted)?;
        let octx = self.octx.as_mut().ok_or(RecorderError::NotStarted)?;

        // Convert RGB to YUV420P
        let yuv_frame = {
            let temp_self = self;
            temp_self.rgb_to_yuv420p(frame)?
        };

        // Send frame to encoder
        encoder.send_frame(&yuv_frame)
            .map_err(RecorderError::FFmpeg)?;

        // Receive and write packets
        let mut packet = ffmpeg::Packet::empty();
        loop {
            match encoder.receive_packet(&mut packet) {
                Ok(()) => {
                    packet.set_stream(0);
                    packet.write_interleaved(octx)
                        .map_err(RecorderError::FFmpeg)?;
                }
                Err(ffmpeg::Error::Other { errno: 11 }) => break, // EAGAIN - no more packets
                Err(ffmpeg::Error::Eof) => break,
                Err(e) => return Err(RecorderError::FFmpeg(e)),
            }
        }

        self.frame_count += 1;
        Ok(())
    }

    fn rgb_to_yuv420p(&self, frame: &VideoFrame) -> Result<ffmpeg::frame::Video> {
        // Create YUV420P frame
        let mut yuv_frame = ffmpeg::frame::Video::new(
            ffmpeg::format::Pixel::YUV420P,
            frame.width,
            frame.height,
        );

        let width = frame.width as usize;
        let height = frame.height as usize;

        if frame.format == VideoPixelFormat::RGB888 {
            // Simple RGB to YUV420P conversion
            let rgb_data = &frame.data;
            let y_size = width * height;
            let uv_size = y_size / 4;

            // Allocate YUV data
            let mut y_data = vec![0u8; y_size];
            let mut u_data = vec![0u8; uv_size];
            let mut v_data = vec![0u8; uv_size];

            // Convert RGB to YUV
            for y in 0..height {
                for x in 0..width {
                    let rgb_idx = (y * width + x) * 3;
                    if rgb_idx + 2 < rgb_data.len() {
                        let r = rgb_data[rgb_idx] as f32;
                        let g = rgb_data[rgb_idx + 1] as f32;
                        let b = rgb_data[rgb_idx + 2] as f32;

                        // RGB to YUV conversion
                        let y_val = (0.299 * r + 0.587 * g + 0.114 * b) as u8;
                        let u_val = ((-0.169 * r - 0.331 * g + 0.500 * b) + 128.0) as u8;
                        let v_val = ((0.500 * r - 0.419 * g - 0.081 * b) + 128.0) as u8;

                        y_data[y * width + x] = y_val;

                        // Subsample UV
                        if x % 2 == 0 && y % 2 == 0 {
                            let uv_idx = (y / 2) * (width / 2) + (x / 2);
                            if uv_idx < uv_size {
                                u_data[uv_idx] = u_val;
                                v_data[uv_idx] = v_val;
                            }
                        }
                    }
                }
            }

            // Copy to frame
            yuv_frame.data_mut(0)[..y_size].copy_from_slice(&y_data);
            yuv_frame.data_mut(1)[..uv_size].copy_from_slice(&u_data);
            yuv_frame.data_mut(2)[..uv_size].copy_from_slice(&v_data);
        } else {
            return Err(RecorderError::InvalidConfig(
                "Only RGB888 format supported in simple encoder".to_string()
            ));
        }

        yuv_frame.set_pts(Some(self.frame_count as i64));
        Ok(yuv_frame)
    }

    pub fn finish(&mut self) -> Result<()> {
        if let (Some(encoder), Some(octx)) = (&mut self.encoder, &mut self.octx) {
            // Send EOF to flush encoder
            let _ = encoder.send_eof();

            // Write remaining packets
            let mut packet = ffmpeg::Packet::empty();
            loop {
                match encoder.receive_packet(&mut packet) {
                    Ok(()) => {
                        packet.set_stream(0);
                        let _ = packet.write_interleaved(octx);
                    }
                    Err(ffmpeg::Error::Eof) => break,
                    Err(_) => break,
                }
            }

            // Write file trailer
            octx.write_trailer()
                .map_err(RecorderError::FFmpeg)?;
        }

        log::info!("Simple encoder finished: {} frames encoded", self.frame_count);

        self.octx = None;
        self.encoder = None;

        Ok(())
    }

    pub fn frame_count(&self) -> u64 {
        self.frame_count
    }
}

impl Drop for SimpleEncoder {
    fn drop(&mut self) {
        let _ = self.finish();
    }
}