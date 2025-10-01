# RustSmartScope

模块化双目相机3D重建系统，采用Rust+C++混合架构，提供高性能的实时立体视觉处理能力。

## 🚀 项目特点

- **高性能**: Rust核心引擎提供零成本抽象和内存安全
- **模块化**: 清晰的分层架构，便于维护和扩展
- **现代UI**: QML响应式用户界面
- **跨平台**: 支持Linux/Windows/macOS
- **实时处理**: 异步数据流水线支持高帧率处理

## 🏗️ 架构设计

```
RustSmartScope/
├── 🦀 Rust Backend           # 核心引擎 (高性能/内存安全)
│   ├── smartscope-core/      # 状态管理和配置
│   ├── camera-pipeline/      # 相机数据流水线
│   ├── stereo-processing/    # 立体视觉算法
│   └── c-ffi/               # C FFI接口层
│
├── 🅲 C++ Bridge             # Qt集成层
│   ├── src/smartscope_core  # Rust接口封装
│   └── src/main.cpp         # 应用程序入口
│
└── 🎨 QML Frontend          # 现代化用户界面
    └── qml/main.qml         # 主界面
```

## 🛠️ 快速开始

### 构建项目
```bash
# 使用构建脚本（推荐）
./build.sh Release

# 或手动构建
cargo build --release -p smartscope-c-ffi
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### 运行程序
```bash
cd build/bin
./rustsmartscope
```

## 📋 功能特性

### ✅ 已实现
- [x] 基础项目架构
- [x] Rust核心引擎
- [x] C FFI接口层
- [x] 现代化QML用户界面
- [x] 配置管理系统
- [x] 错误处理框架
- [x] 响应式设计系统（支持HiDPI）
- [x] 状态栏和导航栏
- [x] 页面路由系统（主页、预览、报告、设置）
- [x] 工具栏组件系统
- [x] 独立3D测量窗口
- [x] 3D测量专用工具栏

### 🎨 UI组件
- [x] StatusIndicator - 状态指示器
- [x] CameraPreview - 相机预览组件
- [x] NavigationButton - 导航按钮
- [x] ToolBarButton - 工具栏按钮
- [x] Measurement3DWindow - 独立3D测量窗口

### 🚧 开发中
- [ ] USB相机驱动集成
- [ ] 实时图像采集
- [ ] 立体校正算法
- [ ] 立体匹配处理
- [ ] 3D点云生成
- [ ] 测量功能实现

## 📚 文档

详细的技术文档和指南请参考 `docs/` 目录：

- [**架构设计**](docs/ARCHITECTURE.md) - 系统架构和模块设计
- [**开发指南**](docs/CLAUDE.md) - Claude Code 开发环境指南
- [**项目状态**](docs/PROJECT_STATUS.md) - 当前开发进度和里程碑
- [**脚本说明**](docs/SCRIPTS_USAGE.md) - 构建和运行脚本使用说明
