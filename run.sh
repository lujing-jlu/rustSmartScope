#!/bin/bash

# RustSmartScope 运行脚本
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
    echo "║           🚀 RustSmartScope 启动器              ║"
    echo "║                                                ║"
    echo "║        基础桥接框架测试 - v0.1.0               ║"
    echo "╚════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# 检查构建状态
check_build() {
    print_step "检查构建状态..."

    # 检查可执行文件
    if [ ! -f "build/bin/rustsmartscope" ]; then
        print_error "应用程序未构建，请先运行: ./build.sh"
        print_info "或者运行: ./build_and_run.sh 进行一键构建运行"
        exit 1
    fi

    # 检查Rust库
    if [ ! -f "target/release/libsmartscope.a" ] && [ ! -f "target/debug/libsmartscope.a" ]; then
        print_warning "未找到Rust库文件，可能需要重新构建"
    fi

    # 检查文件大小
    local app_size=$(du -h "build/bin/rustsmartscope" | cut -f1)
    print_info "✓ 应用程序: $app_size"

    # 检查可执行权限
    if [ ! -x "build/bin/rustsmartscope" ]; then
        print_info "修复可执行权限..."
        chmod +x "build/bin/rustsmartscope"
    fi

    print_success "构建状态检查通过"
}

# 检查运行环境
check_runtime() {
    print_step "检查运行环境..."

    # 检查显示环境
    if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
        print_warning "未检测到图形显示环境 (DISPLAY或WAYLAND_DISPLAY)"
        print_info "如果您在SSH会话中，请使用X11转发: ssh -X username@hostname"

        read -p "是否继续运行？这可能导致图形界面无法显示 [y/N]: " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "已取消运行"
            exit 0
        fi
    else
        print_info "✓ 图形显示环境: ${DISPLAY:-$WAYLAND_DISPLAY}"
    fi

    # 检查Qt库
    if ! ldconfig -p | grep -q libQt5; then
        print_warning "系统中可能缺少Qt5运行时库"
        print_info "请安装: sudo apt install qt5-default"
    else
        print_info "✓ Qt5运行时库已安装"
    fi

    print_success "运行环境检查完成"
}

# 创建运行时文件
prepare_runtime() {
    print_step "准备运行时环境..."

    # 创建日志目录
    mkdir -p logs
    print_info "✓ 日志目录: logs/"

    # 确保使用 smartscope.toml（不再使用 simple_config.toml）
    if [ ! -f "smartscope.toml" ]; then
        cat > smartscope.toml << 'EOF'
[camera]
width = 1280
height = 720
fps = 30

[camera.left]
name_keywords = [
    "cameral",
    "left",
]
parameters_path = "camera_parameters/camera0_intrinsics.dat"
rot_trans_path = "camera_parameters/camera0_rot_trans.dat"

[camera.right]
name_keywords = [
    "camerar",
    "right",
]
parameters_path = "camera_parameters/camera1_intrinsics.dat"
rot_trans_path = "camera_parameters/camera1_rot_trans.dat"

[camera.control]
auto_exposure = true
exposure_time = 10000
gain = 1.0
auto_white_balance = true
brightness = 0
contrast = 0
saturation = 0

[storage]
location = "external"
external_device = "/dev/mmcblk1p1"
internal_base_path = "/home/eddysun/data"
external_relative_path = "EAI-520_data"
auto_recover = true
EOF
        print_info "✓ 创建默认配置文件: smartscope.toml"
    else
        print_info "✓ 使用配置文件: smartscope.toml"
    fi

    print_success "运行时环境准备完成"
}

# 运行应用程序
run_app() {
    print_step "启动RustSmartScope..."

    local exe_path="build/bin/rustsmartscope"
    local start_time=$(date +%s)

    print_info "执行路径: $exe_path"
    print_info "工作目录: $(pwd)"
    print_info "启动时间: $(date)"

    echo ""
    print_success "🚀 正在启动应用程序..."
    echo -e "${YELLOW}─────────────── 应用程序输出 ───────────────${NC}"

    # 设置环境变量
    export QT_QPA_PLATFORM_PLUGIN_PATH="/opt/qt5.15.15/plugins"
    # Add RKNN and RGA runtime libs bundled with the repo
    export LD_LIBRARY_PATH="/opt/qt5.15.15/lib:$(pwd)/crates/rknn-inference/lib:$LD_LIBRARY_PATH"

    # 运行应用程序
    cd "$(dirname "$0")"
    "$exe_path" "$@"
    local exit_code=$?

    echo -e "${YELLOW}─────────────── 应用程序结束 ───────────────${NC}"

    # 计算运行时间
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    if [ $exit_code -eq 0 ]; then
        print_success "应用程序正常结束 (运行时间: ${duration}秒)"
    else
        print_error "应用程序异常结束，退出码: $exit_code (运行时间: ${duration}秒)"

        # 提供故障排除建议
        echo ""
        print_info "故障排除建议:"
        echo -e "  1. 检查日志文件: ${GREEN}ls logs/${NC}"
        echo -e "  2. 重新构建: ${GREEN}./build.sh clean${NC}"
        echo -e "  3. 检查依赖: ${GREEN}ldd build/bin/rustsmartscope${NC}"
        echo -e "  4. 验证Qt环境: ${GREEN}qtdiag${NC} (如果可用)"
    fi

    return $exit_code
}

# 清理资源
cleanup() {
    print_step "清理临时资源..."

    # 清理可能的锁文件
    rm -f *.lock 2>/dev/null || true

    # 清理临时配置文件
    rm -f test_config.toml 2>/dev/null || true

    print_success "资源清理完成"
}

# 显示使用帮助
show_help() {
    echo -e "${CYAN}RustSmartScope 运行脚本${NC}"
    echo ""
    echo "用法:"
    echo "  $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help     显示此帮助信息"
    echo "  --no-check     跳过环境检查直接运行"
    echo "  --debug        启用调试输出"
    echo "  --version      显示版本信息"
    echo ""
    echo "示例:"
    echo "  $0              # 正常运行"
    echo "  $0 --debug      # 调试模式运行"
    echo "  $0 --no-check   # 跳过检查直接运行"
    echo ""
    echo "相关脚本:"
    echo "  ./build.sh           # 只构建应用"
    echo "  ./build_and_run.sh   # 构建并运行"
}

# 主函数
main() {
    # 解析参数
    local skip_check=false
    local debug_mode=false

    for arg in "$@"; do
        case $arg in
            -h|--help)
                show_help
                exit 0
                ;;
            --no-check)
                skip_check=true
                ;;
            --debug)
                debug_mode=true
                set -x  # 启用调试输出
                ;;
            --version)
                echo "RustSmartScope v0.1.0"
                exit 0
                ;;
            *)
                # 其他参数传递给应用程序
                ;;
        esac
    done

    show_banner

    # 检查构建状态
    check_build

    if [ "$skip_check" = false ]; then
        check_runtime
    fi

    prepare_runtime

    # 设置错误处理
    trap 'print_error "应用程序运行时发生错误"; cleanup' ERR
    trap 'print_info "接收到中断信号，正在退出..."; cleanup; exit 130' INT TERM

    # 运行应用程序
    run_app "$@"
    local exit_code=$?

    cleanup

    exit $exit_code
}

# 执行主函数
main "$@"
