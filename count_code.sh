#!/bin/bash

# RustSmartScope 项目代码量统计脚本
# 作者: EDDYSUN
# 版本: 1.0.0

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# 打印带颜色的消息
print_info() { echo -e "${CYAN}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }
print_header() { echo -e "${BOLD}${BLUE}=== $1 ===${NC}"; }

# 显示横幅
show_banner() {
    echo -e "${BOLD}${BLUE}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║     📊 RustSmartScope 项目代码量统计分析工具               ║"
    echo "║                                                            ║"
    echo "║     自动统计各种源码文件的代码行数、文件数等详细数据       ║"
    echo "║                      v1.0.0                                ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# 统计指定类型文件的信息
count_file_stats() {
    local pattern="$1"
    local description="$2"
    local exclude_pattern="$3"

    if [ -z "$exclude_pattern" ]; then
        files=$(find . -type f \( -name "$pattern" \) 2>/dev/null)
    else
        files=$(find . -type f \( -name "$pattern" \) -not -path "$exclude_pattern" 2>/dev/null)
    fi

    if [ -z "$files" ]; then
        echo "0,0,0,0,0,0,$description"
        return
    fi

    file_count=$(echo "$files" | wc -l)
    line_count=$(echo "$files" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
    code_count=$(echo "$files" | xargs grep -v '^\s*$' 2>/dev/null | wc -l)
    comment_count=$(echo "$files" | xargs grep -E '^\s*//' 2>/dev/null | wc -l)
    blank_count=$((line_count - code_count))

    # 计算平均行数
    avg_lines=$((line_count / file_count))

    echo "$file_count,$line_count,$code_count,$comment_count,$blank_count,$avg_lines,$description"
}

# 统计 Rust 文件详细信息
count_rust_stats() {
    print_header "Rust 源码详细分析"

    echo -e "${BOLD}Rust 文件类型分布:${NC}"
    echo ""

    # 统计不同类型的 Rust 文件
    declare -A rust_types=(
        ["lib.rs"]="库主文件"
        ["main.rs"]="主程序文件"
        ["mod.rs"]="模块文件"
        ["ffi.rs"]="FFI接口文件"
        ["api.rs"]="API文件"
        ["types.rs"]="类型定义文件"
        ["error.rs"]="错误处理文件"
        ["config.rs"]="配置文件"
        ["service.rs"]="服务文件"
        ["manager.rs"]="管理器文件"
        ["*.rs"]="其他Rust文件"
    )

    printf "%-15s %-8s %-10s %-10s %-10s %-10s %-15s\n" "文件类型" "文件数" "总行数" "代码行" "注释行" "空白行" "平均行数"
    printf "%-15s %-8s %-10s %-10s %-10s %-10s %-15s\n" "─────────────" "────────" "──────────" "──────────" "──────────" "──────────" "───────────────"

    local total_files=0
    local total_lines=0
    local total_code=0
    local total_comments=0
    local total_blanks=0

    for pattern in "${!rust_types[@]}"; do
        local description="${rust_types[$pattern]}"
        local stats

        if [ "$pattern" = "*.rs" ]; then
            # 排除已经统计过的特定文件类型
            stats=$(count_file_stats "$pattern" "$description" "*/target/*" -not -name "lib.rs" -not -name "main.rs" -not -name "mod.rs" -not -name "*ffi.rs" -not -name "*api.rs" -not -name "types.rs" -not -name "error.rs" -not -name "config.rs" -not -name "service.rs" -not -name "manager.rs")
        else
            stats=$(count_file_stats "$pattern" "$description" "*/target/*")
        fi

        IFS=',' read -r file_count line_count code_count comment_count blank_count avg_lines desc <<< "$stats"

        printf "%-15s %-8s %-10s %-10s %-10s %-10s %-15s\n" "$desc" "$file_count" "$line_count" "$code_count" "$comment_count" "$blank_count" "$avg_lines"

        total_files=$((total_files + file_count))
        total_lines=$((total_lines + line_count))
        total_code=$((total_code + code_count))
        total_comments=$((total_comments + comment_count))
        total_blanks=$((total_blanks + blank_count))
    done

    printf "%-15s %-8s %-10s %-10s %-10s %-10s %-15s\n" "─────────────" "────────" "──────────" "──────────" "──────────" "──────────" "───────────────"
    printf "%-15s ${BOLD}%-8s %-10s %-10s %-10s %-10s %-15s${NC}\n" "总计" "$total_files" "$total_lines" "$total_code" "$total_comments" "$total_blanks" "-"

    echo ""
}

# 统计各 crate 的代码量
count_crate_stats() {
    print_header "Rust Crate 代码量分布"

    echo -e "${BOLD}各 Crate 详细统计:${NC}"
    echo ""

    printf "%-25s %-8s %-10s %-10s %-10s %-10s %-15s\n" "Crate名称" "文件数" "总行数" "代码行" "注释行" "空白行" "平均行数"
    printf "%-25s %-8s %-10s %-10s %-10s %-10s %-15s\n" "─────────────────────────" "────────" "──────────" "──────────" "──────────" "──────────" "───────────────"

    local total_files=0
    local total_lines=0
    local total_code=0
    local total_comments=0
    local total_blanks=0

    for crate_dir in crates/*/; do
        if [ -d "$crate_dir" ]; then
            crate_name=$(basename "$crate_dir")
            files=$(find "$crate_dir" -name "*.rs" 2>/dev/null)

            if [ -n "$files" ]; then
                file_count=$(echo "$files" | wc -l)
                line_count=$(echo "$files" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
                code_count=$(echo "$files" | xargs grep -v '^\s*$' 2>/dev/null | wc -l)
                comment_count=$(echo "$files" | xargs grep -E '^\s*//' 2>/dev/null | wc -l)
                blank_count=$((line_count - code_count))
                avg_lines=$((line_count / file_count))

                printf "%-25s %-8s %-10s %-10s %-10s %-10s %-15s\n" "$crate_name" "$file_count" "$line_count" "$code_count" "$comment_count" "$blank_count" "$avg_lines"

                total_files=$((total_files + file_count))
                total_lines=$((total_lines + line_count))
                total_code=$((total_code + code_count))
                total_comments=$((total_comments + comment_count))
                total_blanks=$((total_blanks + blank_count))
            fi
        fi
    done

    printf "%-25s %-8s %-10s %-10s %-10s %-10s %-15s\n" "─────────────────────────" "────────" "──────────" "──────────" "──────────" "──────────" "───────────────"
    printf "%-25s ${BOLD}%-8s %-10s %-10s %-10s %-10s %-15s${NC}\n" "总计" "$total_files" "$total_lines" "$total_code" "$total_comments" "$total_blanks" "-"

    echo ""
}

# 主统计函数
main_statistics() {
    print_header "项目总体代码量统计"

    echo -e "${BOLD}文件类型分布:${NC}"
    echo ""

    printf "%-15s %-8s %-10s %-10s %-10s %-10s %-15s\n" "文件类型" "文件数" "总行数" "代码行" "注释行" "空白行" "平均行数"
    printf "%-15s %-8s %-10s %-10s %-10s %-10s %-15s\n" "─────────────" "────────" "──────────" "──────────" "──────────" "──────────" "───────────────"

    # 定义要统计的文件类型
    declare -A file_types=(
        ["*.rs"]="Rust源码"
        ["*.cpp"]="C++源码"
        ["*.h"]="C++头文件"
        ["*.hpp"]="C++头文件"
        ["*.qml"]="QML界面"
        ["*.js"]="JavaScript"
        ["*.sh"]="Shell脚本"
        ["*.toml"]="TOML配置"
        ["*.cmake"]="CMake脚本"
        ["CMakeLists.txt"]="CMake配置"
        ["*.md"]="Markdown文档"
    )

    local project_total_files=0
    local project_total_lines=0
    local project_total_code=0
    local project_total_comments=0
    local project_total_blanks=0

    for pattern in "${!file_types[@]}"; do
        local description="${file_types[$pattern]}"
        local exclude_pattern="*/target/*"

        if [[ "$pattern" == "*.rs" || "$pattern" == "*.toml" ]]; then
            stats=$(count_file_stats "$pattern" "$description" "$exclude_pattern")
        else
            stats=$(count_file_stats "$pattern" "$description" "")
        fi

        IFS=',' read -r file_count line_count code_count comment_count blank_count avg_lines desc <<< "$stats"

        if [ "$file_count" -gt 0 ]; then
            printf "%-15s %-8s %-10s %-10s %-10s %-10s %-15s\n" "$desc" "$file_count" "$line_count" "$code_count" "$comment_count" "$blank_count" "$avg_lines"

            project_total_files=$((project_total_files + file_count))
            project_total_lines=$((project_total_lines + line_count))
            project_total_code=$((project_total_code + code_count))
            project_total_comments=$((project_total_comments + comment_count))
            project_total_blanks=$((project_total_blanks + blank_count))
        fi
    done

    printf "%-15s %-8s %-10s %-10s %-10s %-10s %-15s\n" "─────────────" "────────" "──────────" "──────────" "──────────" "──────────" "───────────────"
    printf "%-15s ${BOLD}%-8s %-10s %-10s %-10s %-10s %-15s${NC}\n" "项目总计" "$project_total_files" "$project_total_lines" "$project_total_code" "$project_total_comments" "$project_total_blanks" "-"

    echo ""
    echo -e "${BOLD}项目统计摘要:${NC}"
    echo "  📁 总文件数: ${GREEN}$project_total_files${NC}"
    echo "  📝 总代码行数: ${GREEN}$project_total_lines${NC}"
    echo "  💻 有效代码行: ${GREEN}$project_total_code${NC}"
    echo "  📋 注释行数: ${YELLOW}$project_total_comments${NC}"
    echo "  ⬜ 空白行数: ${GRAY}$project_total_blanks${NC}"
    echo "  📊 代码注释率: ${PURPLE}$(echo "scale=1; $project_total_comments * 100 / $project_total_code" | bc -l)%${NC}"
    echo "  📈 平均文件大小: ${CYAN}$(echo "scale=1; $project_total_lines / $project_total_files" | bc -l)${NC} 行/文件"
    echo ""
}

# 生成详细报告
generate_detailed_report() {
    local report_file="code_statistics_report_$(date +%Y%m%d_%H%M%S).txt"

    print_header "生成详细报告"
    print_info "正在生成报告文件: $report_file"

    {
        echo "RustSmartScope 项目代码量统计报告"
        echo "生成时间: $(date)"
        echo "项目路径: $(pwd)"
        echo ""

        main_statistics
        count_rust_stats
        count_crate_stats

        echo ""
        echo "=== 文件详细列表 ==="
        echo ""

        # 输出所有 Rust 文件详细信息
        echo "Rust 源码文件列表:"
        find . -name "*.rs" -not -path "*/target/*" -exec wc -l {} + | sort -nr

        echo ""
        echo "C++ 源码文件列表:"
        find . -name "*.cpp" -exec wc -l {} + | sort -nr

        echo ""
        echo "QML 文件列表:"
        find . -name "*.qml" -exec wc -l {} + | sort -nr

    } > "$report_file"

    print_success "详细报告已保存到: $report_file"
}

# 主函数
main() {
    show_banner

    # 切换到项目根目录
    cd "$(dirname "$0")"

    print_info "开始统计项目代码量..."
    print_info "项目路径: $(pwd)"
    echo ""

    # 执行统计
    main_statistics
    count_rust_stats
    count_crate_stats

    # 生成报告
    if [ "$1" = "--report" ]; then
        generate_detailed_report
    fi

    print_success "代码量统计完成！"

    echo ""
    print_info "使用方法:"
    echo "  $0              # 基础统计"
    echo "  $0 --report     # 生成详细报告文件"
}

# 错误处理
trap 'print_error "统计过程中发生错误"' ERR

# 执行主函数
main "$@"