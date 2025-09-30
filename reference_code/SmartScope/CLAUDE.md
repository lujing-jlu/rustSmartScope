# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SmartScope is a modular stereo vision system for 3D reconstruction using binocular cameras. Built with Qt5 and OpenCV, it features real-time disparity mapping, point cloud visualization, and AI-powered object detection optimized for RK3588 hardware.

## Build Commands

```bash
# Recommended: Auto build and run
./auto_build_and_run.sh

# Build with options (clean build, parallel jobs)
./scripts/root_scripts/auto_build_run.sh --clean --jobs=4

# Quick test startup
./scripts/快速测试启动.sh
```

The build system uses CMake with out-of-source builds in the `/build` directory. All builds generate the main `SmartScopeQt` executable.

## Architecture

### 4-Layer Design
1. **Infrastructure** (`/src/infrastructure/`): Configuration (TOML), logging, exceptions
2. **Core** (`/src/core/`): Camera management, RGA acceleration, stereo processing
3. **Application** (`/src/app/`): UI pages, measurements, YOLOv8 detection
4. **Specialized**: Stereo depth (`/src/stereo_depth/`), AI inference (`/src/inference/`)

### Key Components
- **Page-based UI**: Navigation between Home, Measurement, Adjust, Debug, Settings, Preview, Report pages
- **Dual Camera System**: Left/right stereo setup with hot-plug detection and calibration
- **Stereo Pipeline**: BM/SGBM/Neural algorithms with real-time depth estimation
- **Measurement Tools**: Line, polyline, rectangle, circle, angle measurements
- **AI Integration**: YOLOv8 object detection with RKNN hardware acceleration

## Configuration System

Uses TOML format (`config.toml`) with hierarchical settings:
- `[camera]`: Dual camera setup, calibration paths
- `[stereo]`: Matching parameters (BM/SGBM/Neural)
- `[measurement]`: Units, precision, auto-save settings
- `[ui]`: Theme, language, window dimensions
- `[logging]`: Levels, file paths, rotation

Configuration supports runtime reloading and robust defaults.

## Hardware Integration

- **RK3588 Platform**: ARM processor with RKNN neural network acceleration
- **RGA**: Rockchip Graphics Acceleration for image processing
- **USB Cameras**: V4L2 interface for stereo camera pairs
- **STM32 HID**: External control integration

## Development Patterns

- **Singleton**: ConfigManager, Logger, CameraManager for system-wide access
- **Factory**: Camera creation, stereo matcher selection
- **Observer**: Configuration change notifications
- **Strategy**: Multiple stereo matching algorithms
- **Worker Threads**: Asynchronous camera capture and processing

## Tech Stack

- **C++17** with Qt5 (Core, Widgets, OpenGL, Charts)
- **OpenCV 4.x** for computer vision
- **PCL** for 3D point cloud processing
- **RKNN Runtime** for AI inference
- **Google glog** for logging
- **TurboJPEG** for image compression

## Testing

Test executables include camera sync tests and multi-camera validation. Use the quick test script for rapid iteration during development.