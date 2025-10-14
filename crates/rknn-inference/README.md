# RKNN Inference

Rust wrapper for RKNN (Rockchip Neural Network) inference, providing YOLOv8 object detection on RK3588 hardware.

## Overview

This crate provides a safe Rust interface to the RKNN inference engine for running YOLOv8 object detection models on Rockchip NPU hardware. It wraps the C/C++ implementation with Rust FFI bindings and provides a high-level API for inference.

## Features

- YOLOv8 object detection inference
- Safe Rust API over RKNN C API
- Hardware-accelerated inference on RK3588 NPU
- Support for quantized models (INT8)
- Automatic image preprocessing (letterbox, resize)
- Post-processing with NMS (Non-Maximum Suppression)

## Project Structure

```
crates/rknn-inference/
├── src/
│   ├── lib.rs          # Main library interface
│   ├── ffi.rs          # C FFI bindings
│   ├── types.rs        # Rust types and data structures
│   └── error.rs        # Error handling
├── csrc/               # C/C++ source files
│   ├── yolov8.cc       # YOLOv8 model inference
│   ├── postprocess.cc  # Post-processing (NMS, etc.)
│   ├── image_utils.c   # Image preprocessing
│   └── file_utils.c    # File I/O utilities
├── include/            # C/C++ header files
│   ├── yolov8.h
│   ├── postprocess.h
│   ├── utils/          # Utility headers
│   └── rga/            # RGA (Rockchip Graphics Acceleration) headers
├── lib/                # Pre-built libraries
│   ├── librknnrt.so    # RKNN runtime library
│   └── librga.so       # RGA library
├── examples/           # Example applications
│   └── yolov8_detect.rs
├── build.rs            # Build script
└── Cargo.toml
```

## Usage

### Basic Example

```rust
use rknn_inference::{ImageBuffer, ImageFormat, Yolov8Detector};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 1. Initialize the detector with a model
    let mut detector = Yolov8Detector::new("path/to/model.rknn")?;

    // 2. Get model input size
    let (width, height) = detector.model_size();
    println!("Model input size: {}x{}", width, height);

    // 3. Prepare your image (RGB888 format)
    let image = ImageBuffer::from_rgb888(width as i32, height as i32, rgb_data);

    // 4. Run inference
    let detections = detector.detect(&image)?;

    // 5. Process results
    for det in detections {
        println!(
            "Class: {}, Confidence: {:.2}, BBox: [{},{},{},{}]",
            det.class_id,
            det.confidence,
            det.bbox.left,
            det.bbox.top,
            det.bbox.right,
            det.bbox.bottom
        );
    }

    Ok(())
}
```

### Running the Example

The crate includes a complete example that demonstrates:
- Loading a YOLOv8 RKNN model
- Loading and preprocessing images
- Running inference
- Processing and saving detection results

To run the example:

```bash
# Build the example
cargo build --example yolov8_detect --release

# Run with your model and image
cargo run --example yolov8_detect --release -- \
    /path/to/model.rknn \
    /path/to/image.jpg \
    output_results.txt
```

## Implementation Details

### Architecture

1. **Rust Layer** (`src/`):
   - High-level safe API
   - Memory safety guarantees
   - Error handling
   - Type conversions

2. **FFI Layer** (`src/ffi.rs`):
   - C bindings to native libraries
   - Unsafe pointer operations
   - Data structure marshalling

3. **C/C++ Layer** (`csrc/`):
   - RKNN API calls
   - Image preprocessing
   - YOLOv8 post-processing
   - NMS algorithm

### Key Components

#### YOLOv8 Inference Pipeline

1. **Model Initialization**: Load RKNN model and query input/output tensors
2. **Image Preprocessing**:
   - Letterbox padding to maintain aspect ratio
   - Resize to model input size
   - Format conversion (RGB888)
3. **Inference**: Run model on NPU hardware
4. **Post-processing**:
   - Parse output tensors
   - Apply confidence threshold
   - NMS to remove duplicate detections
   - Convert coordinates back to original image space

#### Memory Management

- Rust owns all data buffers
- FFI layer uses raw pointers for C interop
- Automatic cleanup via `Drop` trait
- No memory leaks with proper RAII patterns

## Building

### Prerequisites

- Rust toolchain (1.70+)
- C/C++ compiler (gcc/g++)
- CMake (optional, for building from source)
- RKNN runtime libraries (included in `lib/`)
- RGA libraries (included in `lib/`)

### Build Steps

```bash
# Build the library
cargo build --release -p rknn-inference

# Build with examples
cargo build --release --examples -p rknn-inference

# Run tests
cargo test -p rknn-inference
```

The build script (`build.rs`) automatically:
1. Compiles C/C++ source files
2. Links RKNN and RGA libraries
3. Sets up include paths
4. Generates static library `libyolov8_wrapper.a`

## API Reference

### `Yolov8Detector`

Main detector struct for running inference.

```rust
impl Yolov8Detector {
    /// Create a new detector from a model file
    pub fn new<P: AsRef<Path>>(model_path: P) -> Result<Self>;

    /// Run inference on an image
    pub fn detect(&mut self, image: &ImageBuffer) -> Result<Vec<DetectionResult>>;

    /// Get model input dimensions
    pub fn model_size(&self) -> (u32, u32);
}
```

### `ImageBuffer`

Image data container.

```rust
pub struct ImageBuffer {
    pub width: i32,
    pub height: i32,
    pub format: ImageFormat,
    pub data: Vec<u8>,
}

impl ImageBuffer {
    pub fn new(width: i32, height: i32, format: ImageFormat, data: Vec<u8>) -> Self;
    pub fn from_rgb888(width: i32, height: i32, data: Vec<u8>) -> Self;
}
```

### `DetectionResult`

Object detection result.

```rust
pub struct DetectionResult {
    pub bbox: ImageRect,      // Bounding box coordinates
    pub confidence: f32,       // Detection confidence (0.0-1.0)
    pub class_id: i32,         // COCO class ID
}
```

### `ImageRect`

Bounding box representation.

```rust
pub struct ImageRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl ImageRect {
    pub fn width(&self) -> i32;
    pub fn height(&self) -> i32;
}
```

## Image Preprocessing

The example code shows how to implement image preprocessing. For production use, consider:

1. **Image Loading**: Use the `image` crate for loading various formats:
   ```rust
   use image::io::Reader as ImageReader;

   let img = ImageReader::open("image.jpg")?.decode()?;
   let rgb = img.to_rgb8();
   ```

2. **Resizing**: Use `imageproc` or `image` crate for resizing
3. **Letterbox Padding**: Maintain aspect ratio with padding (implemented in C++ layer)
4. **Color Space**: Ensure RGB888 format for input

## Performance Considerations

- The RKNN NPU provides hardware acceleration
- INT8 quantized models are recommended for best performance
- Batch inference is not currently supported
- Image preprocessing (resize, letterbox) is done on CPU

## Limitations

- Only YOLOv8 detection is currently supported
- No support for YOLOv8 segmentation or pose estimation
- Single image inference only (no batching)
- Thread safety: detector is `Send` but not `Sync`

## Future Enhancements

- [ ] Support for YOLOv8 segmentation
- [ ] Support for YOLOv8 pose estimation
- [ ] Batch inference support
- [ ] Zero-copy image input via DMA buffers
- [ ] Additional model formats (YOLOv5, YOLOv7, etc.)
- [ ] Async inference API

## License

This project uses code from Rockchip's RKNN examples, licensed under Apache 2.0.

## References

- [RKNN Toolkit](https://github.com/rockchip-linux/rknn-toolkit2)
- [YOLOv8](https://github.com/ultralytics/ultralytics)
- [RK3588 Documentation](https://www.rock-chips.com/a/en/products/RK35_Series/2022/0926/1660.html)
