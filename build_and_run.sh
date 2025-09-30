#!/bin/bash

# RustSmartScope 一键编译运行脚本
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
    echo "║       🛠️ RustSmartScope 一键构建运行           ║"
    echo "║                                                ║"
    echo "║     构建 → 验证 → 运行 全流程自动化             ║"
    echo "║                v0.1.0                         ║"
    echo "╚════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# 检查脚本存在
check_scripts() {
    print_step "检查必要脚本..."

    local missing_scripts=()

    if [ ! -f "build.sh" ]; then
        missing_scripts+=("build.sh")
    fi

    if [ ! -f "run.sh" ]; then
        missing_scripts+=("run.sh")
    fi

    if [ ${#missing_scripts[@]} -ne 0 ]; then
        print_error "缺少以下脚本文件:"
        for script in "${missing_scripts[@]}"; do
            echo -e "${RED}  - ${script}${NC}"
        done
        exit 1
    fi

    # 检查脚本可执行权限
    if [ ! -x "build.sh" ]; then
        chmod +x "build.sh"
        print_info "修复build.sh执行权限"
    fi

    if [ ! -x "run.sh" ]; then
        chmod +x "run.sh"
        print_info "修复run.sh执行权限"
    fi

    print_success "脚本检查通过"
}

# 解析构建参数
parse_build_args() {
    local build_args=()
    local run_args=()
    local parsing_run_args=false

    for arg in "$@"; do
        case $arg in
            --run-*)
                # 运行参数
                parsing_run_args=true
                run_args+=("${arg#--run-}")
                ;;
            --)
                # 分隔符，后面的参数都是运行参数
                parsing_run_args=true
                ;;
            *)
                if [ "$parsing_run_args" = true ]; then
                    run_args+=("$arg")
                else
                    case $arg in
                        clean|debug|release)
                            build_args+=("$arg")
                            ;;
                        --help|-h)
                            show_help
                            exit 0
                            ;;
                        *)
                            print_warning "未知构建参数: $arg，将传递给构建脚本"
                            build_args+=("$arg")
                            ;;
                    esac
                fi
                ;;
        esac
    done

    echo "${build_args[@]} || ${run_args[@]}"
}

# 显示使用帮助
show_help() {
    echo -e "${CYAN}RustSmartScope 一键编译运行脚本${NC}"
    echo ""
    echo "用法:"
    echo "  $0 [构建选项] [-- 运行选项]"
    echo ""
    echo "构建选项:"
    echo "  clean          清理重新构建"
    echo "  debug          调试模式构建"
    echo "  release        发布模式构建 (默认)"
    echo ""
    echo "运行选项 (在 -- 之后):"
    echo "  --debug        调试模式运行"
    echo "  --no-check     跳过运行时检查"
    echo ""
    echo "示例:"
    echo "  $0                    # 默认构建并运行"
    echo "  $0 clean              # 清理构建并运行"
    echo "  $0 debug              # 调试模式构建并运行"
    echo "  $0 -- --debug         # 默认构建，调试模式运行"
    echo "  $0 clean -- --no-check # 清理构建，跳过检查运行"
    echo ""
    echo "单独使用的脚本:"
    echo "  ./build.sh           # 只构建"
    echo "  ./run.sh            # 只运行"
}

# 执行构建
execute_build() {
    local build_args=("$@")

    print_step "开始构建阶段..."
    echo -e "${YELLOW}═══════════════════════════════════════════════${NC}"

    # 执行构建脚本
    if [ ${#build_args[@]} -eq 0 ]; then
        print_info "执行: ./build.sh"
        ./build.sh
    else
        print_info "执行: ./build.sh ${build_args[*]}"
        ./build.sh "${build_args[@]}"
    fi

    print_success "构建阶段完成"
    echo -e "${YELLOW}═══════════════════════════════════════════════${NC}"
}

# 执行运行
execute_run() {
    local run_args=("$@")

    print_step "开始运行阶段..."
    echo -e "${YELLOW}═══════════════════════════════════════════════${NC}"

    # 短暂暂停让用户准备
    echo ""
    print_info "应用程序即将启动..."
    print_info "按 Ctrl+C 可以随时退出"
    sleep 2

    # 执行运行脚本
    if [ ${#run_args[@]} -eq 0 ]; then
        print_info "执行: ./run.sh"
        ./run.sh
    else
        print_info "执行: ./run.sh ${run_args[*]}"
        ./run.sh "${run_args[@]}"
    fi

    local exit_code=$?

    echo -e "${YELLOW}═══════════════════════════════════════════════${NC}"

    if [ $exit_code -eq 0 ]; then
        print_success "运行阶段完成"
    else
        print_error "运行阶段失败，退出码: $exit_code"
    fi

    return $exit_code
}

# 显示完成总结
show_completion() {
    local total_duration=$1
    local build_success=$2
    local run_exit_code=$3

    echo ""
    print_step "执行总结"

    echo -e "${CYAN}📊 执行统计:${NC}"
    echo -e "  ⏱️  总耗时: ${total_duration}秒"
    echo -e "  🔨 构建结果: $([ "$build_success" = true ] && echo -e "${GREEN}成功${NC}" || echo -e "${RED}失败${NC}")"

    if [ "$build_success" = true ]; then
        case $run_exit_code in
            0)
                echo -e "  🚀 运行结果: ${GREEN}正常结束${NC}"
                ;;
            130)
                echo -e "  🚀 运行结果: ${YELLOW}用户中断${NC}"
                ;;
            *)
                echo -e "  🚀 运行结果: ${RED}异常结束 (退出码: $run_exit_code)${NC}"
                ;;
        esac
    else
        echo -e "  🚀 运行结果: ${RED}未执行 (构建失败)${NC}"
    fi

    echo ""
    if [ $run_exit_code -eq 0 ] || [ $run_exit_code -eq 130 ]; then
        print_success "🎉 任务完成!"
    else
        print_error "❌ 任务失败"
        echo ""
        print_info "故障排除步骤:"
        echo -e "  1. 查看构建日志: ${GREEN}./build.sh${NC}"
        echo -e "  2. 单独运行测试: ${GREEN}./run.sh --debug${NC}"
        echo -e "  3. 清理重新构建: ${GREEN}./build.sh clean${NC}"
    fi
}

# 主函数
main() {
    local start_time=$(date +%s)

    show_banner

    # 检查必要脚本
    check_scripts

    # 解析参数
    local args_result=$(parse_build_args "$@")
    local build_args_str=$(echo "$args_result" | cut -d'|' -f1 | xargs)
    local run_args_str=$(echo "$args_result" | cut -d'|' -f2 | xargs)

    # 转换为数组
    local build_args=()
    local run_args=()

    if [ -n "$build_args_str" ] && [ "$build_args_str" != " " ]; then
        read -ra build_args <<< "$build_args_str"
    fi

    if [ -n "$run_args_str" ] && [ "$run_args_str" != " " ]; then
        read -ra run_args <<< "$run_args_str"
    fi

    print_info "构建参数: ${build_args[*]:-无}"
    print_info "运行参数: ${run_args[*]:-无}"

    # 设置错误处理
    local build_success=false
    local run_exit_code=1

    # 执行构建
    if execute_build "${build_args[@]}"; then
        build_success=true

        # 执行运行
        execute_run "${run_args[@]}"
        run_exit_code=$?
    else
        print_error "构建失败，跳过运行阶段"
        run_exit_code=1
    fi

    # 计算总耗时
    local end_time=$(date +%s)
    local total_duration=$((end_time - start_time))

    # 显示完成总结
    show_completion "$total_duration" "$build_success" "$run_exit_code"

    exit $run_exit_code
}

# 错误处理
trap 'print_error "脚本执行过程中发生错误"' ERR

# 执行主函数
main "$@"