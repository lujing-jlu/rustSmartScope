//! FFmpeg video encoder wrapper

use crate::{RecorderConfig, RecorderError, Result, VideoFrame, VideoPixelFormat, VideoCodec, HardwareAccelType};
use ffmpeg_next as ffmpeg;
use ffmpeg_next::format::{Pixel, context};
use ffmpeg_next::software::scaling::{context::Context as ScalerContext, flag::Flags};
use ffmpeg_next::codec;
use std::path::Path;

pub struct VideoEncoder {
    octx: Option<context::Output>,
    encoder: Option<codec::encoder::Video>,
    scaler: Option<ScalerContext>,
    video_stream_index: Option<usize>,
    frame_count: u64,
    config: RecorderConfig,
}

impl VideoEncoder {
    pub fn new(config: RecorderConfig) -> Result<Self> {
        Ok(Self {
            octx: None,
            encoder: None,
            scaler: None,
            video_stream_index: None,
            frame_count: 0,
            config,
        })
    }

    pub fn start(&mut self) -> Result<()> {
        if self.octx.is_some() {
            return Err(RecorderError::AlreadyRunning);
        }

        ffmpeg::init()?;

        // Create output context
        let path = Path::new(&self.config.output_path);
        let mut octx = ffmpeg::format::output(&path)?;

        // Find encoder
        let codec_id = match self.config.codec {
            VideoCodec::H264 => {
                if self.config.hardware_accel == HardwareAccelType::RkMpp {
                    // Try Rockchip MPP encoder first
                    codec::Id::H264
                } else {
                    codec::Id::H264
                }
            }
            VideoCodec::H265 => codec::Id::HEVC,
            VideoCodec::VP8 => codec::Id::VP8,
            VideoCodec::VP9 => codec::Id::VP9,
        };

        let encoder_name = match (self.config.codec, self.config.hardware_accel) {
            (VideoCodec::H264, HardwareAccelType::RkMpp) => "h264_rkmpp",
            (VideoCodec::H265, HardwareAccelType::RkMpp) => "hevc_rkmpp",
            (VideoCodec::H264, HardwareAccelType::VaApi) => "h264_vaapi",
            (VideoCodec::H265, HardwareAccelType::VaApi) => "hevc_vaapi",
            (VideoCodec::H264, HardwareAccelType::None) => "libx264",
            (VideoCodec::H265, HardwareAccelType::None) => "libx265",
            (VideoCodec::VP8, _) => "libvpx",
            (VideoCodec::VP9, _) => "libvpx-vp9",
        };

        // Try hardware encoder first, fall back to software if not available
        let codec = ffmpeg::encoder::find_by_name(encoder_name)
            .or_else(|| {
                // Fallback encoders
                match self.config.codec {
                    VideoCodec::H264 => {
                        log::warn!("Hardware encoder {} not available, falling back to libx264", encoder_name);
                        ffmpeg::encoder::find_by_name("libx264")
                    }
                    VideoCodec::H265 => {
                        log::warn!("Hardware encoder {} not available, falling back to libx265", encoder_name);
                        ffmpeg::encoder::find_by_name("libx265")
                    }
                    _ => None
                }
            })
            .or_else(|| ffmpeg::encoder::find(codec_id))
            .ok_or_else(|| RecorderError::FFmpeg(
                ffmpeg::Error::EncoderNotFound
            ))?;

        log::info!("Using encoder: {}", codec.name());

        // Add video stream
        let mut ost = octx.add_stream(codec)?;
        let video_stream_index = ost.index();

        // Configure encoder
        let mut encoder = codec::context::Context::from_parameters(ost.parameters())?
            .encoder()
            .video()?;

        encoder.set_width(self.config.width);
        encoder.set_height(self.config.height);
        encoder.set_format(Pixel::YUV420P);
        encoder.set_time_base((1, self.config.fps as i32));
        encoder.set_frame_rate(Some((self.config.fps as i32, 1)));
        encoder.set_bit_rate(self.config.bitrate as usize);

        // Codec-specific options
        if codec.name() == "libx264" {
            // Set libx264 preset and profile
            unsafe {
                (*encoder.as_mut_ptr()).gop_size = self.config.fps as i32 * 2;
                (*encoder.as_mut_ptr()).max_b_frames = 2;
            }

            // Open with options
            let mut dict = ffmpeg::Dictionary::new();
            dict.set("preset", "medium");
            dict.set("profile", "high");
            dict.set("tune", "zerolatency");
            let encoder = encoder.open_as_with(codec, dict)?;
            ost.set_parameters(&encoder);

            // Write header
            octx.write_header()?;

            log::info!(
                "Encoder started: {}x{} @ {}fps, bitrate: {}",
                self.config.width,
                self.config.height,
                self.config.fps,
                self.config.bitrate
            );

            self.octx = Some(octx);
            self.encoder = Some(encoder);
            self.video_stream_index = Some(video_stream_index);
            self.frame_count = 0;

            return Ok(());
        } else {
            // Other codecs use default opening
            unsafe {
                match self.config.codec {
                    VideoCodec::H264 | VideoCodec::H265 => {
                        (*encoder.as_mut_ptr()).gop_size = self.config.fps as i32 * 2;
                        (*encoder.as_mut_ptr()).max_b_frames = 2;
                    }
                    _ => {}
                }
            }
        }

        let encoder = encoder.open_as(codec)?;
        ost.set_parameters(&encoder);

        // Write header
        octx.write_header()?;

        log::info!(
            "Encoder started: {}x{} @ {}fps, bitrate: {}",
            self.config.width,
            self.config.height,
            self.config.fps,
            self.config.bitrate
        );

        self.octx = Some(octx);
        self.encoder = Some(encoder);
        self.video_stream_index = Some(video_stream_index);
        self.frame_count = 0;

        Ok(())
    }

    pub fn encode_frame(&mut self, frame: &VideoFrame) -> Result<()> {
        if self.encoder.is_none() {
            return Err(RecorderError::NotStarted);
        }

        // Convert frame to YUV420P
        let yuv_frame = self.convert_to_yuv420p(frame)?;

        // Send frame to encoder
        {
            let encoder = self.encoder.as_mut().ok_or(RecorderError::NotStarted)?;
            encoder.send_frame(&yuv_frame)?;
        }

        // Receive packets
        let stream_index = self.video_stream_index.ok_or(RecorderError::NotStarted)?;
        self.receive_packets_impl(stream_index)?;

        self.frame_count += 1;

        Ok(())
    }

    fn convert_to_yuv420p(&mut self, frame: &VideoFrame) -> Result<ffmpeg::frame::Video> {
        let src_format = match frame.format {
            VideoPixelFormat::RGB888 => Pixel::RGB24,
            VideoPixelFormat::BGR888 => Pixel::BGR24,
            VideoPixelFormat::RGBA8888 => Pixel::RGBA,
            VideoPixelFormat::BGRA8888 => Pixel::BGRA,
            VideoPixelFormat::YUV420P => Pixel::YUV420P,
            VideoPixelFormat::NV12 => Pixel::NV12,
        };

        // If already YUV420P, create frame directly
        if src_format == Pixel::YUV420P {
            let mut yuv_frame = ffmpeg::frame::Video::new(
                Pixel::YUV420P,
                frame.width,
                frame.height,
            );
            // Copy data
            let data = &frame.data;
            let y_size = (frame.width * frame.height) as usize;
            let uv_size = y_size / 4;

            if data.len() >= y_size + uv_size * 2 {
                yuv_frame.data_mut(0)[..y_size].copy_from_slice(&data[..y_size]);
                yuv_frame.data_mut(1)[..uv_size].copy_from_slice(&data[y_size..y_size + uv_size]);
                yuv_frame.data_mut(2)[..uv_size].copy_from_slice(&data[y_size + uv_size..y_size + uv_size * 2]);
            }

            yuv_frame.set_pts(Some(self.frame_count as i64));
            return Ok(yuv_frame);
        }

        // Initialize scaler if needed
        if self.scaler.is_none() {
            let scaler = ScalerContext::get(
                src_format,
                frame.width,
                frame.height,
                Pixel::YUV420P,
                self.config.width,
                self.config.height,
                Flags::BILINEAR,
            )?;
            self.scaler = Some(scaler);
        }

        // Create source frame
        let mut src_frame = ffmpeg::frame::Video::new(
            src_format,
            frame.width,
            frame.height,
        );

        // Copy data to source frame
        let bytes_per_pixel = match frame.format {
            VideoPixelFormat::RGB888 | VideoPixelFormat::BGR888 => 3,
            VideoPixelFormat::RGBA8888 | VideoPixelFormat::BGRA8888 => 4,
            _ => 3,
        };

        let src_stride = frame.width as usize * bytes_per_pixel;
        let frame_data = &frame.data;

        for y in 0..frame.height as usize {
            let src_offset = y * src_stride;
            let dst_offset = y * src_frame.stride(0);
            let len = src_stride.min(src_frame.stride(0));

            if src_offset + len <= frame_data.len() {
                src_frame.data_mut(0)[dst_offset..dst_offset + len]
                    .copy_from_slice(&frame_data[src_offset..src_offset + len]);
            }
        }

        // Create destination frame
        let mut yuv_frame = ffmpeg::frame::Video::new(
            Pixel::YUV420P,
            self.config.width,
            self.config.height,
        );

        // Scale and convert
        {
            let scaler = self.scaler.as_mut().ok_or(RecorderError::NotStarted)?;
            scaler.run(&src_frame, &mut yuv_frame)?;
        }

        yuv_frame.set_pts(Some(self.frame_count as i64));

        Ok(yuv_frame)
    }

    fn receive_packets_impl(&mut self, stream_index: usize) -> Result<()> {
        let mut encoded = ffmpeg::Packet::empty();

        loop {
            let result = {
                let encoder = self.encoder.as_mut().ok_or(RecorderError::NotStarted)?;
                encoder.receive_packet(&mut encoded)
            };

            match result {
                Ok(_) => {
                    encoded.set_stream(stream_index);
                    let octx = self.octx.as_mut().ok_or(RecorderError::NotStarted)?;
                    encoded.write_interleaved(octx)?;
                }
                Err(ffmpeg::Error::Other { errno: 11 }) => break, // EAGAIN
                Err(ffmpeg::Error::Eof) => break,
                Err(e) => return Err(e.into()),
            }
        }

        Ok(())
    }

    pub fn finish(&mut self) -> Result<()> {
        if let Some(encoder) = &mut self.encoder {
            // Flush encoder
            let _ = encoder.send_eof();

            if let Some(stream_index) = self.video_stream_index {
                let _ = self.receive_packets_impl(stream_index);
            }
        }

        if let Some(octx) = &mut self.octx {
            octx.write_trailer()?;
        }

        log::info!("Encoder finished: {} frames encoded", self.frame_count);

        self.octx = None;
        self.encoder = None;
        self.scaler = None;

        Ok(())
    }

    pub fn frame_count(&self) -> u64 {
        self.frame_count
    }
}

impl Drop for VideoEncoder {
    fn drop(&mut self) {
        let _ = self.finish();
    }
}
