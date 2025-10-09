# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Rules
Always use context7 when I need code generation, setup or configuration steps, or
library/API documentation. This means you should automatically use the Context7 MCP
tools to resolve library id and get library docs without me having to explicitly ask.

## Testing and Quality Assurance

### Testing Commands
- `cargo test` - Run Rust unit tests for all workspace crates
- `cargo test -p smartscope-core` - Run tests for specific crate
- `cargo clippy` - Run Rust linter for code quality
- `cargo fmt` - Format Rust code according to style guidelines

### Quality Checks
- No dedicated test runner scripts currently configured
- When adding tests, place them in `tests/` directory for integration tests
- Unit tests should be co-located with source code using `#[cfg(test)]` modules

## Project Overview

RustSmartScope is a modular stereo camera 3D reconstruction system built with a Rust+C++/Qt hybrid architecture. It demonstrates modern cross-language integration patterns with Rust providing the high-performance core engine and Qt/QML providing the user interface.

## Build System and Commands

### Primary Build Commands
- `./build.sh` - Standard release build
- `./build.sh debug` - Debug build with debugging symbols
- `./build.sh clean` - Clean rebuild (removes build directory)
- `./build.sh clean debug` - Clean debug rebuild

### Run Commands
- `./run.sh` - Run the built application with environment checks
- `./run.sh --debug` - Run with debug output
- `./run.sh --no-check` - Skip environment validation

### Integrated Commands
- `./build_and_run.sh` - One-command build and run (recommended for development)
- `./build_and_run.sh clean` - Clean build and run
- `./build_and_run.sh debug` - Debug build and run

### Additional Build Scripts
- `./build_incremental.sh` - Incremental build for faster development iteration
- `./build_optimized.sh` - Optimized build with additional performance flags

### Cleanup Commands
- `./clean.sh` - Clean build artifacts only
- `./clean.sh all` - Clean everything (build + Rust target + logs)
- `./clean.sh rust` - Clean only Rust target directory

### Manual Build Process (if needed)
```bash
# Build Rust core library
cargo build --release -p smartscope-c-ffi

# Build C++ application
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
cd ..

# Run application
cd build/bin && ./rustsmartscope
```

## Architecture and Structure

### Multi-Language Architecture
The project uses a three-layer architecture:

1. **Rust Backend (`crates/`)**:
   - `smartscope-core/` - Core state management, configuration, and error handling
   - `c-ffi/` - C Foreign Function Interface layer for Qt integration
   - `usb-camera/` - Camera interface and video frame handling
   - `linux-video/` - Low-level Linux V4L2 video support
   - Built as static library (`libsmartscope.a`)

2. **C++ Bridge Layer (`src/`, `CMakeLists.txt`)**:
   - Qt5 application framework
   - Integrates Rust static library via C FFI
   - Manages Qt/QML lifecycle
   - Includes `camera_manager.cpp/h` for camera integration

3. **QML Frontend (`qml/`)**:
   - Declarative user interface
   - Modern responsive design
   - Cross-platform UI layer

### Key Architectural Patterns
- **C FFI Interface**: The `c-ffi` crate provides a safe C-compatible API that bridges Rust and C++
- **State Management**: Centralized application state in `smartscope-core::AppState`
- **Configuration System**: TOML-based configuration with runtime loading/saving
- **Error Handling**: Typed error system with C-compatible error codes
- **Logging**: Unified logging system accessible from Rust, C++, and QML

### Workspace Structure
This is a Cargo workspace with multiple crates:
- Main workspace defined in root `Cargo.toml`
- Individual crates have their own `Cargo.toml` files
- Shared dependencies defined at workspace level
- Active crates: `smartscope-core`, `c-ffi`, `usb-camera`, `linux-video`
- Reference code in `reference_code/` (excluded from workspace)

## Development Workflow

### Dependencies Required
- Rust toolchain (cargo, rustc)
- CMake 3.16+
- Qt5 development libraries (Core, Gui, Widgets, Quick, Qml, Svg)
- G++ or compatible C++ compiler
- Make
- libturbojpeg (for JPEG encoding/decoding)

### When Making Changes

1. **Rust Code Changes**:
   - Modify code in `crates/smartscope-core/` or `crates/c-ffi/`
   - Run `cargo build --release -p smartscope-c-ffi` to rebuild Rust library
   - Or use `./build.sh` for full rebuild

2. **C++ Code Changes**:
   - Modify `src/main.cpp` or other C++ files
   - Use `./build.sh` to rebuild both Rust and C++ components

3. **QML Interface Changes**:
   - Modify `qml/main.qml` or component files
   - No rebuild needed, changes reflected on next run

4. **C API Changes**:
   - If modifying C FFI exports in `crates/c-ffi/src/lib.rs`
   - Regenerate headers with `cbindgen` (handled by build scripts)
   - Full rebuild required

### Testing the Application
- Use `./build_and_run.sh` for rapid development iteration
- Check build artifacts in `build/bin/rustsmartscope`
- Rust library built to `target/release/libsmartscope.a`

### Configuration Management
- Application configuration stored in TOML format
- Default config: `config.toml`
- Runtime loading/saving via C FFI interface
- Configuration structure defined in `smartscope-core::config`

## Code Organization Patterns

### Rust Code Patterns
- Follow standard Rust conventions (snake_case, modules, etc.)
- Error handling uses `thiserror` for structured errors
- Configuration uses `serde` for TOML serialization
- C FFI functions use `#[no_mangle]` and `extern "C"`
- Thread-safe state management with `RwLock`

### C++ Integration Patterns
- All Rust functionality accessed through C FFI
- Qt application lifecycle managed in `src/main.cpp`
- QML engine integration follows Qt5 patterns
- Resource management follows RAII patterns

### Build System Patterns
- CMake handles C++ build and Rust library linking
- Cargo workspace manages Rust dependencies and builds
- Shell scripts provide developer convenience and environment validation
- CBinded generates C headers automatically

## Common Tasks

### Adding New Rust Functionality
1. Add to appropriate crate (`smartscope-core` for core logic)
2. If exposing to C++, add C FFI wrapper in `c-ffi` crate
3. Update C header generation if needed (cbindgen)
4. Full rebuild with `./build.sh`

### Adding New UI Features
1. Modify `qml/main.qml` for main interface changes
2. Create new QML components in `qml/components/` directory
3. Create new page components in `qml/pages/` directory
4. Add new windows as separate QML files (e.g., `Measurement3DWindow.qml`)
5. Update `qml/qml.qrc` resource file to include new components
6. For icons, add SVG files to `resources/icons/` and reference in qrc
7. No rebuild needed for QML changes, just run application
8. Add C++ integration in `src/main.cpp` if needed
9. Expose new Rust functionality via C FFI if required

### Configuration Changes
1. Update `AppConfig` structure in `smartscope-core::config`
2. Regenerate C FFI bindings if config API changes
3. Update default `config.toml` if needed

### Debugging
- Use `./build.sh debug` for debug builds with symbols
- Run with `./run.sh --debug` for verbose output
- Check application logs in `logs/` directory
- Use `ldd build/bin/rustsmartscope` to check library dependencies

## Important Files and Locations

### Key Configuration Files
- `config.toml` - Main application configuration
- `simple_config.toml` - Minimal configuration for testing
- `cbindgen.toml` - C header generation configuration

### Build Outputs
- `build/bin/rustsmartscope` - Main executable
- `target/release/libsmartscope.a` - Rust static library
- `include/` - Generated C headers for FFI

### Resource Files
- `qml/qml.qrc` - QML resource file
- `qml/fonts.qrc` - Font resources
- `resources/icons/` - SVG icon files
- `camera_parameters/` - Camera calibration data

## Current UI Architecture

### Main Window Structure
- **ApplicationWindow**: Full-screen, frameless window
- **Status Bar**: Top bar with time, date, and system info
- **Navigation Bar**: Bottom bar with main navigation buttons
- **Content Area**: Central area for page content
- **Right Toolbar**: Vertical toolbar with tool buttons

### Window System
- **Main Window** (`main.qml`): Primary application interface
- **3D Measurement Window** (`Measurement3DWindow.qml`): Independent measurement window
  - Opened via "3D测量" button in main navigation
  - Has its own specialized toolbar for measurement tools
  - Supports minimize/close operations

### Component System
```
qml/components/
├── StatusIndicator.qml           # Status display component
├── CameraPreview.qml             # Camera feed display
├── NavigationButton.qml          # Legacy navigation bar buttons
├── UnifiedNavigationButton.qml   # New unified navigation component
└── ToolBarButton.qml             # Toolbar action buttons
```

### Page System
```
qml/pages/
├── HomePage.qml             # Welcome/dashboard page
├── PreviewPage.qml          # Camera preview page
├── ReportPage.qml           # Report generation page
├── SettingsPage.qml         # Application settings
├── MeasurementPage.qml      # Legacy measurement page
└── DebugPage.qml            # Debug and development page
```

### Navigation System
- **UnifiedNavigationButton**: New unified navigation component with enhanced features
  - Support for both icon-only and text+icon modes
  - Square button mode for special buttons (home, exit)
  - Enhanced hover effects and animations
  - Consistent styling across all navigation elements
- **Navigation Layout**: Fixed single-row layout with 6 buttons
  - Home, Preview, Report, Settings, 3D Measurement, Exit
  - Dynamic button width calculation for optimal text display
  - Responsive design for different screen sizes

### Toolbar Configurations
- **Main Window Toolbar**: General camera controls (config, capture, LED, AI)
- **3D Measurement Toolbar**: Specialized measurement tools
  - 3D校准 (Calibration)
  - 点云生成 (Point Cloud Generation)
  - 深度测量 (Depth Measurement)
  - 长度测量 (Length Measurement)
  - 面积测量 (Area Measurement)
  - 数据导出 (Data Export)

### Resource Management
- Icons stored in `resources/icons/` with SVG format
- QML resources managed via `qml/qml.qrc`
- Fonts loaded via `qml/fonts.qrc`
- All resources embedded in binary for distribution
- New unified navigation component added to resource system

### Design System
- Modern dark theme with professional blue accents
- HiDPI responsive scaling system
- Consistent spacing and typography
- Hover and click animations for better UX
- Enhanced button styling with unified component system
