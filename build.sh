#!/bin/bash

# RustSmartScope 编译脚本
# 作者: EDDYSUN
# 版本: 0.1.0

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() { echo -e "${CYAN}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }
print_step() { echo -e "${PURPLE}[STEP]${NC} $1"; }

# 显示横幅
show_banner() {
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════════════╗"
    echo "║           🦀 RustSmartScope 构建器              ║"
    echo "║                                                ║"
    echo "║           Rust + C++ + Qt 混合架构             ║"
    echo "║                v0.1.0                         ║"
    echo "╚════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# 检查依赖
check_dependencies() {
    print_step "检查构建依赖..."

    local missing_deps=()

    # 检查Rust工具链
    if ! command -v cargo &> /dev/null; then
        missing_deps+=("cargo (Rust工具链)")
    else
        print_info "✓ Cargo: $(cargo --version | head -n1)"
    fi

    # 检查CMake
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    else
        print_info "✓ CMake: $(cmake --version | head -n1)"
    fi

    # 检查Make
    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    else
        print_info "✓ Make: $(make --version | head -n1)"
    fi

    # 检查C++编译器
    if ! command -v g++ &> /dev/null; then
        missing_deps+=("g++ (C++编译器)")
    else
        print_info "✓ G++: $(g++ --version | head -n1)"
    fi

    # 检查Qt5
    if ! pkg-config --exists Qt5Core; then
        missing_deps+=("Qt5开发库")
    else
        print_info "✓ Qt5: $(pkg-config --modversion Qt5Core)"
    fi

    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "缺少以下依赖:"
        for dep in "${missing_deps[@]}"; do
            echo -e "${RED}  - ${dep}${NC}"
        done
        echo ""
        print_info "在Ubuntu/Debian上安装依赖:"
        echo "  sudo apt update"
        echo "  sudo apt install build-essential cmake qt5-default qtdeclarative5-dev"
        echo "  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
        exit 1
    fi

    print_success "所有依赖检查通过!"
}

# 清理构建目录
clean_build() {
    print_step "清理构建目录..."

    if [ -d "build" ]; then
        rm -rf build
        print_info "删除旧的build目录"
    fi

    if [ -d "target" ] && [ "$1" = "clean" ]; then
        rm -rf target
        print_info "删除Rust target目录"
    fi

    mkdir -p build
    print_success "构建目录已准备"
}

# 构建Rust库
build_rust() {
    print_step "构建Rust核心库..."

    local build_mode=${1:-release}

    if [ "$build_mode" = "debug" ]; then
        print_info "使用Debug模式构建Rust库..."
        cargo build -p smartscope-c-ffi
        RUST_LIB_PATH="target/debug/libsmartscope.a"
    else
        print_info "使用Release模式构建Rust库..."
        cargo build --release -p smartscope-c-ffi
        RUST_LIB_PATH="target/release/libsmartscope.a"
    fi

    if [ ! -f "$RUST_LIB_PATH" ]; then
        print_error "Rust库构建失败: $RUST_LIB_PATH 不存在"
        exit 1
    fi

    local lib_size=$(du -h "$RUST_LIB_PATH" | cut -f1)
    print_success "Rust库构建完成 ($lib_size)"
}

# 构建C++应用
build_cpp() {
    print_step "构建C++应用..."

    cd build

    local build_type=${1:-Release}

    print_info "配置CMake (构建类型: $build_type)..."
    cmake -DCMAKE_BUILD_TYPE="$build_type" ..

    print_info "编译C++应用..."
    local cpu_count=$(nproc)
    make -j"$cpu_count"

    cd ..

    if [ ! -f "build/bin/rustsmartscope" ]; then
        print_error "C++应用构建失败"
        exit 1
    fi

    local app_size=$(du -h "build/bin/rustsmartscope" | cut -f1)
    print_success "C++应用构建完成 ($app_size)"
}

# 验证构建结果
verify_build() {
    print_step "验证构建结果..."

    # 检查可执行文件
    if [ ! -f "build/bin/rustsmartscope" ]; then
        print_error "可执行文件未找到"
        exit 1
    fi

    # 检查文件权限
    if [ ! -x "build/bin/rustsmartscope" ]; then
        chmod +x "build/bin/rustsmartscope"
        print_info "修复可执行权限"
    fi

    # 检查依赖库
    print_info "检查动态库依赖..."
    if command -v ldd &> /dev/null; then
        local missing_libs=$(ldd build/bin/rustsmartscope 2>/dev/null | grep "not found" || true)
        if [ -n "$missing_libs" ]; then
            print_warning "发现缺失的动态库:"
            echo "$missing_libs"
        else
            print_success "所有动态库依赖满足"
        fi
    fi

    print_success "构建验证通过!"
}

# 显示构建摘要
show_summary() {
    print_step "构建摘要"

    echo ""
    echo -e "${CYAN}📁 项目结构:${NC}"
    echo -e "  ├── 🦀 Rust核心库: $(ls -lh target/release/libsmartscope.a 2>/dev/null | awk '{print $5}' || echo 'N/A')"
    echo -e "  ├── 🅲 C++应用: $(ls -lh build/bin/rustsmartscope 2>/dev/null | awk '{print $5}' || echo 'N/A')"
    echo -e "  └── 🎨 QML界面: qml/main.qml"

    echo ""
    echo -e "${CYAN}🚀 运行方式:${NC}"
    echo -e "  直接运行:    ${GREEN}./run.sh${NC}"
    echo -e "  手动运行:    ${GREEN}cd build/bin && ./rustsmartscope${NC}"
    echo -e "  构建+运行:   ${GREEN}./build_and_run.sh${NC}"

    echo ""
    echo -e "${CYAN}🛠 其他命令:${NC}"
    echo -e "  重新构建:    ${GREEN}./build.sh clean${NC}"
    echo -e "  调试构建:    ${GREEN}./build.sh debug${NC}"
    echo -e "  清理全部:    ${GREEN}./build.sh clean debug${NC}"
}

# 主函数
main() {
    show_banner

    # 解析参数
    local build_mode="release"
    local build_type="Release"
    local clean_first=false

    for arg in "$@"; do
        case $arg in
            clean)
                clean_first=true
                ;;
            debug)
                build_mode="debug"
                build_type="Debug"
                ;;
            release)
                build_mode="release"
                build_type="Release"
                ;;
            *)
                print_warning "未知参数: $arg"
                ;;
        esac
    done

    # 记录开始时间
    local start_time=$(date +%s)

    # 执行构建步骤
    check_dependencies

    if [ "$clean_first" = true ]; then
        clean_build clean
    else
        clean_build
    fi

    build_rust "$build_mode"
    build_cpp "$build_type"
    verify_build

    # 计算耗时
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    show_summary

    echo ""
    print_success "🎉 构建完成! 耗时: ${duration}秒"

    # 提示下一步
    echo ""
    print_info "下一步: 运行 ${GREEN}./run.sh${NC} 启动应用"
}

# 错误处理
trap 'print_error "构建过程中发生错误，退出码: $?"' ERR

# 执行主函数
main "$@"