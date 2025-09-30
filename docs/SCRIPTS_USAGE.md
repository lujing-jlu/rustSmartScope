# 🛠️ RustSmartScope 脚本使用指南

本项目提供了完整的自动化脚本工具链，让编译和运行变得简单。

## 📋 脚本列表

| 脚本 | 功能 | 使用场景 |
|------|------|----------|
| `build.sh` | 🔨 编译项目 | 需要重新构建时 |
| `run.sh` | 🚀 运行应用 | 构建完成后运行 |
| `build_and_run.sh` | ⚡ 一键编译运行 | 开发测试时 |
| `clean.sh` | 🧹 清理文件 | 清理构建产物 |

## 🚀 快速开始

### 1. 首次运行
```bash
# 一键编译并运行 (推荐)
./build_and_run.sh

# 或者分步执行
./build.sh          # 先构建
./run.sh            # 后运行
```

### 2. 日常开发
```bash
# 快速重新构建运行
./build_and_run.sh clean

# 调试模式
./build_and_run.sh debug

# 只构建不运行
./build.sh

# 只运行不构建
./run.sh
```

### 3. 清理维护
```bash
# 清理构建产物
./clean.sh

# 清理所有文件
./clean.sh all

# 只清理Rust缓存
./clean.sh rust
```

## 📖 详细说明

### `build.sh` - 构建脚本

**功能：** 完整的项目构建流程

**用法：**
```bash
./build.sh [选项]
```

**选项：**
- `release` - 发布模式构建 (默认)
- `debug` - 调试模式构建
- `clean` - 清理后重新构建

**示例：**
```bash
./build.sh              # 默认发布模式
./build.sh debug         # 调试模式
./build.sh clean         # 清理重建
./build.sh clean debug   # 清理后调试构建
```

**构建步骤：**
1. ✅ 检查构建依赖 (Rust, CMake, Qt5, G++)
2. 🧹 清理构建目录
3. 🦀 构建Rust核心库
4. 🅲 构建C++应用
5. ✔️ 验证构建结果

### `run.sh` - 运行脚本

**功能：** 启动已构建的应用程序

**用法：**
```bash
./run.sh [选项]
```

**选项：**
- `--help` - 显示帮助信息
- `--no-check` - 跳过环境检查直接运行
- `--debug` - 启用调试输出
- `--version` - 显示版本信息

**示例：**
```bash
./run.sh              # 正常运行
./run.sh --debug      # 调试模式运行
./run.sh --no-check   # 跳过检查直接运行
```

**运行步骤：**
1. ✅ 检查构建状态
2. 🖥️ 检查运行环境 (图形界面、Qt库)
3. 📁 准备运行时环境
4. 🚀 启动应用程序
5. 🧹 清理资源

### `build_and_run.sh` - 一键编译运行脚本

**功能：** 自动化构建并运行整个流程

**用法：**
```bash
./build_and_run.sh [构建选项] [-- 运行选项]
```

**示例：**
```bash
./build_and_run.sh                    # 默认构建运行
./build_and_run.sh clean              # 清理构建运行
./build_and_run.sh debug              # 调试模式构建运行
./build_and_run.sh -- --debug         # 默认构建，调试运行
./build_and_run.sh clean -- --no-check # 清理构建，跳过检查运行
```

### `clean.sh` - 清理脚本

**功能：** 清理各种构建产物和临时文件

**用法：**
```bash
./clean.sh [选项]
```

**选项：**
- `all` - 清理所有文件 (构建产物 + Rust缓存 + 日志)
- `build` - 只清理构建产物 (build目录)
- `rust` - 只清理Rust缓存 (target目录)
- `logs` - 只清理日志文件
- `temp` - 只清理临时文件

**示例：**
```bash
./clean.sh           # 清理构建产物
./clean.sh all       # 清理所有文件
./clean.sh rust      # 只清理Rust缓存
```

## 🔧 故障排除

### 常见问题

**1. 构建失败**
```bash
# 解决方案
./clean.sh all       # 完全清理
./build.sh clean     # 重新构建
```

**2. 运行时错误**
```bash
# 调试步骤
./run.sh --debug     # 调试运行
ldd build/bin/rustsmartscope  # 检查依赖
qtdiag               # 检查Qt环境
```

**3. 图形界面无法显示**
```bash
# 检查显示环境
echo $DISPLAY        # X11显示
echo $WAYLAND_DISPLAY # Wayland显示

# SSH X11转发
ssh -X username@hostname
```

**4. Qt库缺失**
```bash
# Ubuntu/Debian
sudo apt install qt5-default qtdeclarative5-dev

# 检查安装
pkg-config --modversion Qt5Core
```

### 日志和调试

**构建日志：**
- 脚本提供实时彩色输出
- 错误信息会高亮显示

**运行日志：**
- 应用运行日志保存在 `logs/` 目录
- 使用 `--debug` 参数获取详细输出

**性能监控：**
- 脚本会显示构建耗时
- 文件大小统计
- 依赖库检查结果

## 💡 最佳实践

1. **开发时：** 使用 `./build_and_run.sh debug` 快速测试
2. **测试时：** 使用 `./build_and_run.sh clean` 确保干净环境
3. **发布时：** 使用 `./build.sh` 生成优化版本
4. **维护时：** 定期使用 `./clean.sh all` 清理空间

## 📊 脚本特性

- ✅ **彩色输出** - 清晰的状态指示
- ✅ **错误处理** - 详细的错误信息和建议
- ✅ **进度显示** - 实时构建进度
- ✅ **依赖检查** - 自动验证构建环境
- ✅ **平台适配** - 自动适配不同Linux发行版
- ✅ **性能统计** - 构建时间和文件大小统计
- ✅ **故障排除** - 提供解决方案建议

让编译和运行变得简单！🎉