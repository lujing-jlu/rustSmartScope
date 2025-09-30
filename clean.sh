#!/bin/bash

# RustSmartScope 清理脚本
# 作者: EDDYSUN
# 版本: 0.1.0

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

print_info() { echo -e "${CYAN}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 显示横幅
show_banner() {
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════════════╗"
    echo "║           🧹 RustSmartScope 清理工具            ║"
    echo "║                                                ║"
    echo "║        清理构建产物和临时文件                   ║"
    echo "╚════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# 显示帮助
show_help() {
    echo -e "${CYAN}RustSmartScope 清理脚本${NC}"
    echo ""
    echo "用法:"
    echo "  $0 [选项]"
    echo ""
    echo "选项:"
    echo "  all      清理所有文件 (构建产物 + Rust缓存 + 日志)"
    echo "  build    只清理构建产物 (build目录)"
    echo "  rust     只清理Rust缓存 (target目录)"
    echo "  logs     只清理日志文件"
    echo "  temp     只清理临时文件"
    echo "  -h       显示此帮助"
    echo ""
    echo "示例:"
    echo "  $0           # 清理构建产物"
    echo "  $0 all       # 清理所有文件"
    echo "  $0 rust      # 只清理Rust缓存"
}

# 清理构建产物
clean_build() {
    print_info "清理构建产物..."

    local cleaned=0

    if [ -d "build" ]; then
        local size=$(du -sh build 2>/dev/null | cut -f1 || echo "未知")
        rm -rf build
        print_success "删除 build/ 目录 (${size})"
        ((cleaned++))
    fi

    # 清理CMake缓存文件
    for file in CMakeCache.txt CMakeFiles cmake_install.cmake Makefile; do
        if [ -e "$file" ]; then
            rm -rf "$file"
            print_success "删除 $file"
            ((cleaned++))
        fi
    done

    if [ $cleaned -eq 0 ]; then
        print_info "没有构建产物需要清理"
    fi
}

# 清理Rust缓存
clean_rust() {
    print_info "清理Rust缓存..."

    if [ -d "target" ]; then
        local size=$(du -sh target 2>/dev/null | cut -f1 || echo "未知")
        rm -rf target
        print_success "删除 target/ 目录 (${size})"
    else
        print_info "没有Rust缓存需要清理"
    fi
}

# 清理日志文件
clean_logs() {
    print_info "清理日志文件..."

    local cleaned=0

    if [ -d "logs" ]; then
        local size=$(du -sh logs 2>/dev/null | cut -f1 || echo "未知")
        rm -rf logs/*
        print_success "清理 logs/ 目录内容 (${size})"
        ((cleaned++))
    fi

    # 清理根目录下的日志文件
    for logfile in *.log; do
        if [ -f "$logfile" ]; then
            rm -f "$logfile"
            print_success "删除 $logfile"
            ((cleaned++))
        fi
    done

    if [ $cleaned -eq 0 ]; then
        print_info "没有日志文件需要清理"
    fi
}

# 清理临时文件
clean_temp() {
    print_info "清理临时文件..."

    local cleaned=0
    local temp_patterns=("*.tmp" "*.lock" "*~" ".#*" "core" "test_config.toml" "*.orig" "*.rej")

    for pattern in "${temp_patterns[@]}"; do
        for file in $pattern; do
            if [ -f "$file" ]; then
                rm -f "$file"
                print_success "删除 $file"
                ((cleaned++))
            fi
        done
    done

    # 清理编辑器临时文件
    find . -name ".*.swp" -o -name ".*.swo" -o -name "*~" 2>/dev/null | while read -r file; do
        if [ -f "$file" ]; then
            rm -f "$file"
            print_success "删除 $file"
            ((cleaned++))
        fi
    done

    if [ $cleaned -eq 0 ]; then
        print_info "没有临时文件需要清理"
    fi
}

# 显示清理前的大小统计
show_size_stats() {
    print_info "清理前大小统计:"

    local total_size=0

    if [ -d "build" ]; then
        local build_size=$(du -sm build 2>/dev/null | cut -f1 || echo "0")
        echo -e "  构建产物: ${build_size}MB"
        ((total_size += build_size))
    fi

    if [ -d "target" ]; then
        local target_size=$(du -sm target 2>/dev/null | cut -f1 || echo "0")
        echo -e "  Rust缓存: ${target_size}MB"
        ((total_size += target_size))
    fi

    if [ -d "logs" ]; then
        local logs_size=$(du -sm logs 2>/dev/null | cut -f1 || echo "0")
        echo -e "  日志文件: ${logs_size}MB"
        ((total_size += logs_size))
    fi

    echo -e "  总计: ${total_size}MB"

    if [ $total_size -gt 100 ]; then
        print_warning "检测到大量文件 (${total_size}MB)，清理可释放大量磁盘空间"
    fi
}

# 主函数
main() {
    show_banner

    case "${1:-build}" in
        all)
            show_size_stats
            echo ""
            read -p "确定要清理所有文件吗? [y/N]: " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                clean_build
                clean_rust
                clean_logs
                clean_temp
                print_success "🎉 全部清理完成!"
            else
                print_info "已取消清理操作"
            fi
            ;;
        build)
            clean_build
            ;;
        rust)
            clean_rust
            ;;
        logs)
            clean_logs
            ;;
        temp)
            clean_temp
            ;;
        -h|--help)
            show_help
            ;;
        *)
            print_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"