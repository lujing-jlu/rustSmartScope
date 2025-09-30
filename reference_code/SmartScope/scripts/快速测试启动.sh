#!/bin/bash

# SmartScope 快速测试启动脚本
# 提供简化的测试执行选项

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 项目路径
PROJECT_ROOT="/home/eddysun/App/Qt/SmartScope"
BUILD_DIR="$PROJECT_ROOT/build"

# 显示菜单
show_menu() {
    clear
    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN}    SmartScope 快速测试启动器${NC}"
    echo -e "${CYAN}========================================${NC}"
    echo ""
    echo -e "${GREEN}请选择要执行的测试类型:${NC}"
    echo ""
    echo -e "${YELLOW}1.${NC} 快速单元测试 (5分钟)"
    echo -e "${YELLOW}2.${NC} 相机功能测试 (10分钟)"
    echo -e "${YELLOW}3.${NC} 立体视觉测试 (15分钟)"
    echo -e "${YELLOW}4.${NC} 性能基准测试 (20分钟)"
    echo -e "${YELLOW}5.${NC} 完整测试套件 (60分钟)"
    echo -e "${YELLOW}6.${NC} 自定义测试选择"
    echo -e "${YELLOW}7.${NC} 查看测试报告"
    echo -e "${YELLOW}8.${NC} 清理测试环境"
    echo -e "${YELLOW}0.${NC} 退出"
    echo ""
    echo -e "${BLUE}========================================${NC}"
}

# 检查环境
check_environment() {
    echo -e "${BLUE}检查测试环境...${NC}"
    
    if [ ! -d "$PROJECT_ROOT" ]; then
        echo -e "${RED}错误: 项目目录不存在${NC}"
        exit 1
    fi
    
    cd "$PROJECT_ROOT" || exit 1
    
    # 检查必要工具
    local missing_tools=()
    
    if ! command -v cmake &> /dev/null; then
        missing_tools+=("cmake")
    fi
    
    if ! command -v make &> /dev/null; then
        missing_tools+=("make")
    fi
    
    if ! command -v python3 &> /dev/null; then
        missing_tools+=("python3")
    fi
    
    if [ ${#missing_tools[@]} -gt 0 ]; then
        echo -e "${RED}错误: 缺少必要工具: ${missing_tools[*]}${NC}"
        echo -e "${YELLOW}请安装缺少的工具后重试${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}环境检查通过${NC}"
}

# 构建项目
build_project() {
    echo -e "${BLUE}构建项目...${NC}"
    
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
    fi
    
    cd "$BUILD_DIR" || exit 1
    
    if ! cmake -DCMAKE_BUILD_TYPE=Debug ..; then
        echo -e "${RED}CMake配置失败${NC}"
        return 1
    fi
    
    if ! make -j$(nproc); then
        echo -e "${RED}项目编译失败${NC}"
        return 1
    fi
    
    echo -e "${GREEN}项目构建完成${NC}"
    return 0
}

# 快速单元测试
quick_unit_test() {
    echo -e "${CYAN}执行快速单元测试...${NC}"
    
    local test_count=0
    local pass_count=0
    
    # 配置管理测试
    if [ -f "$BUILD_DIR/test_config_simple" ]; then
        echo -e "${BLUE}测试配置管理...${NC}"
        if timeout 30 "$BUILD_DIR/test_config_simple"; then
            echo -e "${GREEN}✓ 配置管理测试通过${NC}"
            ((pass_count++))
        else
            echo -e "${RED}✗ 配置管理测试失败${NC}"
        fi
        ((test_count++))
    fi
    
    # 异常处理测试
    if [ -f "$BUILD_DIR/test_exception" ]; then
        echo -e "${BLUE}测试异常处理...${NC}"
        if timeout 30 "$BUILD_DIR/test_exception"; then
            echo -e "${GREEN}✓ 异常处理测试通过${NC}"
            ((pass_count++))
        else
            echo -e "${RED}✗ 异常处理测试失败${NC}"
        fi
        ((test_count++))
    fi
    
    # 文件管理测试
    if [ -f "$BUILD_DIR/test_file_manager" ]; then
        echo -e "${BLUE}测试文件管理...${NC}"
        if timeout 30 "$BUILD_DIR/test_file_manager"; then
            echo -e "${GREEN}✓ 文件管理测试通过${NC}"
            ((pass_count++))
        else
            echo -e "${RED}✗ 文件管理测试失败${NC}"
        fi
        ((test_count++))
    fi
    
    echo ""
    echo -e "${CYAN}快速单元测试完成: ${pass_count}/${test_count} 通过${NC}"
    
    if [ $pass_count -eq $test_count ]; then
        echo -e "${GREEN}所有测试通过！${NC}"
    else
        echo -e "${YELLOW}部分测试失败，请检查详细日志${NC}"
    fi
}

# 相机功能测试
camera_function_test() {
    echo -e "${CYAN}执行相机功能测试...${NC}"
    
    # 检查相机设备
    if [ ! -c "/dev/video0" ] && [ ! -c "/dev/video1" ]; then
        echo -e "${YELLOW}警告: 未检测到相机设备，将使用模拟模式${NC}"
    fi
    
    local test_count=0
    local pass_count=0
    
    # 相机基础测试
    if [ -f "$BUILD_DIR/test_camera" ]; then
        echo -e "${BLUE}测试相机基础功能...${NC}"
        if timeout 60 "$BUILD_DIR/test_camera"; then
            echo -e "${GREEN}✓ 相机基础功能测试通过${NC}"
            ((pass_count++))
        else
            echo -e "${RED}✗ 相机基础功能测试失败${NC}"
        fi
        ((test_count++))
    fi
    
    # 多相机管理测试
    if [ -f "$BUILD_DIR/test_multi_camera" ]; then
        echo -e "${BLUE}测试多相机管理...${NC}"
        if timeout 120 "$BUILD_DIR/test_multi_camera" --duration 30 --no-ui; then
            echo -e "${GREEN}✓ 多相机管理测试通过${NC}"
            ((pass_count++))
        else
            echo -e "${RED}✗ 多相机管理测试失败${NC}"
        fi
        ((test_count++))
    fi
    
    # Python相机测试
    if [ -f "$PROJECT_ROOT/camera_test.py" ]; then
        echo -e "${BLUE}测试Python相机接口...${NC}"
        if timeout 60 python3 "$PROJECT_ROOT/camera_test.py"; then
            echo -e "${GREEN}✓ Python相机接口测试通过${NC}"
            ((pass_count++))
        else
            echo -e "${RED}✗ Python相机接口测试失败${NC}"
        fi
        ((test_count++))
    fi
    
    echo ""
    echo -e "${CYAN}相机功能测试完成: ${pass_count}/${test_count} 通过${NC}"
}

# 立体视觉测试
stereo_vision_test() {
    echo -e "${CYAN}执行立体视觉测试...${NC}"
    
    local test_count=0
    local pass_count=0
    
    # 检查立体视觉推理模块
    local stereo_dir="$PROJECT_ROOT/reference_code/lightstereo_inference"
    if [ -d "$stereo_dir" ]; then
        echo -e "${BLUE}测试立体视觉推理...${NC}"
        cd "$stereo_dir" || return 1
        
        if [ -f "./run_example.sh" ]; then
            if timeout 300 ./run_example.sh; then
                echo -e "${GREEN}✓ 立体视觉推理测试通过${NC}"
                ((pass_count++))
            else
                echo -e "${RED}✗ 立体视觉推理测试失败${NC}"
            fi
            ((test_count++))
        fi
    fi
    
    # 深度计算测试
    if [ -f "$PROJECT_ROOT/test_depth_calculation.py" ]; then
        echo -e "${BLUE}测试深度计算...${NC}"
        cd "$PROJECT_ROOT" || return 1
        if timeout 180 python3 test_depth_calculation.py; then
            echo -e "${GREEN}✓ 深度计算测试通过${NC}"
            ((pass_count++))
        else
            echo -e "${RED}✗ 深度计算测试失败${NC}"
        fi
        ((test_count++))
    fi
    
    echo ""
    echo -e "${CYAN}立体视觉测试完成: ${pass_count}/${test_count} 通过${NC}"
}

# 性能基准测试
performance_benchmark_test() {
    echo -e "${CYAN}执行性能基准测试...${NC}"
    
    echo -e "${BLUE}测试系统性能指标...${NC}"
    
    # CPU信息
    echo -e "${YELLOW}CPU信息:${NC}"
    lscpu | grep "Model name\|CPU(s)\|Thread(s)"
    
    # 内存信息
    echo -e "${YELLOW}内存信息:${NC}"
    free -h
    
    # 存储性能测试
    echo -e "${BLUE}测试存储性能...${NC}"
    if command -v dd &> /dev/null; then
        echo "写入性能测试:"
        dd if=/dev/zero of=/tmp/test_write bs=1M count=100 2>&1 | grep -E "copied|MB/s"
        echo "读取性能测试:"
        dd if=/tmp/test_write of=/dev/null bs=1M 2>&1 | grep -E "copied|MB/s"
        rm -f /tmp/test_write
    fi
    
    # 相机性能测试
    if [ -f "$PROJECT_ROOT/scripts/tools/camera_test_fixed.py" ]; then
        echo -e "${BLUE}测试相机性能...${NC}"
        if timeout 120 python3 "$PROJECT_ROOT/scripts/tools/camera_test_fixed.py"; then
            echo -e "${GREEN}✓ 相机性能测试完成${NC}"
        else
            echo -e "${RED}✗ 相机性能测试失败${NC}"
        fi
    fi
    
    echo -e "${CYAN}性能基准测试完成${NC}"
}

# 完整测试套件
full_test_suite() {
    echo -e "${CYAN}执行完整测试套件...${NC}"
    
    if [ -f "$PROJECT_ROOT/测试实施脚本.sh" ]; then
        chmod +x "$PROJECT_ROOT/测试实施脚本.sh"
        "$PROJECT_ROOT/测试实施脚本.sh"
    else
        echo -e "${YELLOW}完整测试脚本不存在，执行分步测试...${NC}"
        quick_unit_test
        camera_function_test
        stereo_vision_test
        performance_benchmark_test
    fi
}

# 自定义测试选择
custom_test_selection() {
    echo -e "${CYAN}自定义测试选择${NC}"
    echo ""
    echo "可用的测试程序:"
    
    local test_programs=()
    local index=1
    
    # 查找可执行的测试程序
    if [ -d "$BUILD_DIR" ]; then
        for test_file in "$BUILD_DIR"/test_*; do
            if [ -x "$test_file" ]; then
                test_programs+=("$test_file")
                echo "$index. $(basename "$test_file")"
                ((index++))
            fi
        done
    fi
    
    # 查找Python测试脚本
    for test_file in "$PROJECT_ROOT"/*test*.py; do
        if [ -f "$test_file" ]; then
            test_programs+=("$test_file")
            echo "$index. $(basename "$test_file")"
            ((index++))
        fi
    done
    
    echo ""
    read -p "请选择要执行的测试 (输入数字，多个用空格分隔): " selections
    
    for selection in $selections; do
        if [[ "$selection" =~ ^[0-9]+$ ]] && [ "$selection" -ge 1 ] && [ "$selection" -lt "$index" ]; then
            local test_program="${test_programs[$((selection-1))]}"
            echo -e "${BLUE}执行: $(basename "$test_program")${NC}"
            
            if [[ "$test_program" == *.py ]]; then
                timeout 300 python3 "$test_program"
            else
                timeout 300 "$test_program"
            fi
        else
            echo -e "${RED}无效选择: $selection${NC}"
        fi
    done
}

# 查看测试报告
view_test_reports() {
    echo -e "${CYAN}查看测试报告${NC}"
    
    local reports_dir="$PROJECT_ROOT/test_results/reports"
    
    if [ -d "$reports_dir" ]; then
        echo "可用的测试报告:"
        ls -la "$reports_dir"/*.html 2>/dev/null || echo "暂无HTML报告"
        
        echo ""
        echo "最近的测试日志:"
        if [ -f "$PROJECT_ROOT/test_results/test_execution.log" ]; then
            tail -20 "$PROJECT_ROOT/test_results/test_execution.log"
        else
            echo "暂无测试日志"
        fi
    else
        echo -e "${YELLOW}测试报告目录不存在，请先执行测试${NC}"
    fi
    
    echo ""
    read -p "按回车键继续..."
}

# 清理测试环境
clean_test_environment() {
    echo -e "${CYAN}清理测试环境${NC}"
    
    # 清理构建目录
    if [ -d "$BUILD_DIR" ]; then
        echo "清理构建目录..."
        rm -rf "$BUILD_DIR"
        echo -e "${GREEN}构建目录已清理${NC}"
    fi
    
    # 清理测试结果
    if [ -d "$PROJECT_ROOT/test_results" ]; then
        echo "清理测试结果..."
        rm -rf "$PROJECT_ROOT/test_results"
        echo -e "${GREEN}测试结果已清理${NC}"
    fi
    
    # 清理临时文件
    echo "清理临时文件..."
    find "$PROJECT_ROOT" -name "*.log" -type f -delete 2>/dev/null
    find "$PROJECT_ROOT" -name "core.*" -type f -delete 2>/dev/null
    find "$PROJECT_ROOT" -name "*.tmp" -type f -delete 2>/dev/null
    
    echo -e "${GREEN}测试环境清理完成${NC}"
}

# 主函数
main() {
    # 检查环境
    check_environment
    
    while true; do
        show_menu
        read -p "请输入选择 [0-8]: " choice
        
        case $choice in
            1)
                echo ""
                if build_project; then
                    quick_unit_test
                fi
                echo ""
                read -p "按回车键继续..."
                ;;
            2)
                echo ""
                if build_project; then
                    camera_function_test
                fi
                echo ""
                read -p "按回车键继续..."
                ;;
            3)
                echo ""
                if build_project; then
                    stereo_vision_test
                fi
                echo ""
                read -p "按回车键继续..."
                ;;
            4)
                echo ""
                performance_benchmark_test
                echo ""
                read -p "按回车键继续..."
                ;;
            5)
                echo ""
                if build_project; then
                    full_test_suite
                fi
                echo ""
                read -p "按回车键继续..."
                ;;
            6)
                echo ""
                if build_project; then
                    custom_test_selection
                fi
                echo ""
                read -p "按回车键继续..."
                ;;
            7)
                echo ""
                view_test_reports
                ;;
            8)
                echo ""
                clean_test_environment
                echo ""
                read -p "按回车键继续..."
                ;;
            0)
                echo -e "${GREEN}退出测试启动器${NC}"
                exit 0
                ;;
            *)
                echo -e "${RED}无效选择，请重新输入${NC}"
                sleep 2
                ;;
        esac
    done
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
