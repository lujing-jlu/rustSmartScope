#!/bin/bash

# SmartScope 工业双目内窥镜系统 - 测试实施脚本
# 版本: 1.0
# 作者: 测试团队
# 日期: 2024-08-06

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 全局变量
PROJECT_ROOT="/home/eddysun/App/Qt/SmartScope"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_DIR="$PROJECT_ROOT/src/tests"
TEST_RESULTS_DIR="$PROJECT_ROOT/test_results"
LOG_FILE="$TEST_RESULTS_DIR/test_execution.log"

# 创建测试结果目录
mkdir -p "$TEST_RESULTS_DIR"
mkdir -p "$TEST_RESULTS_DIR/unit_tests"
mkdir -p "$TEST_RESULTS_DIR/integration_tests"
mkdir -p "$TEST_RESULTS_DIR/performance_tests"
mkdir -p "$TEST_RESULTS_DIR/reports"

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_FILE"
}

log_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1" | tee -a "$LOG_FILE"
}

# 检查环境函数
check_environment() {
    log_info "检查测试环境..."
    
    # 检查项目目录
    if [ ! -d "$PROJECT_ROOT" ]; then
        log_error "项目根目录不存在: $PROJECT_ROOT"
        exit 1
    fi
    
    # 检查构建目录
    if [ ! -d "$BUILD_DIR" ]; then
        log_warn "构建目录不存在，创建构建目录: $BUILD_DIR"
        mkdir -p "$BUILD_DIR"
    fi
    
    # 检查必要的工具
    local tools=("cmake" "make" "g++" "python3" "valgrind")
    for tool in "${tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            log_error "必要工具未安装: $tool"
            exit 1
        fi
    done
    
    # 检查相机设备
    if [ ! -c "/dev/video0" ] && [ ! -c "/dev/video1" ]; then
        log_warn "未检测到相机设备，某些测试可能无法执行"
    fi
    
    log_info "环境检查完成"
}

# 构建项目函数
build_project() {
    log_info "构建项目..."
    
    cd "$BUILD_DIR" || exit 1
    
    # 配置CMake
    if ! cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON ..; then
        log_error "CMake配置失败"
        exit 1
    fi
    
    # 编译项目
    if ! make -j$(nproc); then
        log_error "项目编译失败"
        exit 1
    fi
    
    log_info "项目构建完成"
}

# 单元测试函数
run_unit_tests() {
    log_info "开始执行单元测试..."
    
    local test_results="$TEST_RESULTS_DIR/unit_tests"
    local test_count=0
    local pass_count=0
    local fail_count=0
    
    # 基础设施层测试
    log_info "执行基础设施层测试..."
    
    # 配置管理测试
    if [ -f "$BUILD_DIR/test_config" ]; then
        log_debug "执行配置管理测试..."
        if timeout 60 "$BUILD_DIR/test_config" > "$test_results/config_test.log" 2>&1; then
            log_info "✓ 配置管理测试通过"
            ((pass_count++))
        else
            log_error "✗ 配置管理测试失败"
            ((fail_count++))
        fi
        ((test_count++))
    fi
    
    # 日志系统测试
    if [ -f "$BUILD_DIR/test_logging" ]; then
        log_debug "执行日志系统测试..."
        if timeout 60 "$BUILD_DIR/test_logging" > "$test_results/logging_test.log" 2>&1; then
            log_info "✓ 日志系统测试通过"
            ((pass_count++))
        else
            log_error "✗ 日志系统测试失败"
            ((fail_count++))
        fi
        ((test_count++))
    fi
    
    # 异常处理测试
    if [ -f "$BUILD_DIR/test_exception" ]; then
        log_debug "执行异常处理测试..."
        if timeout 60 "$BUILD_DIR/test_exception" > "$test_results/exception_test.log" 2>&1; then
            log_info "✓ 异常处理测试通过"
            ((pass_count++))
        else
            log_error "✗ 异常处理测试失败"
            ((fail_count++))
        fi
        ((test_count++))
    fi
    
    # 核心层测试
    log_info "执行核心层测试..."
    
    # 相机模块测试
    if [ -f "$BUILD_DIR/test_camera" ]; then
        log_debug "执行相机模块测试..."
        if timeout 120 "$BUILD_DIR/test_camera" > "$test_results/camera_test.log" 2>&1; then
            log_info "✓ 相机模块测试通过"
            ((pass_count++))
        else
            log_error "✗ 相机模块测试失败"
            ((fail_count++))
        fi
        ((test_count++))
    fi
    
    # 多相机管理测试
    if [ -f "$BUILD_DIR/test_multi_camera" ]; then
        log_debug "执行多相机管理测试..."
        if timeout 180 "$BUILD_DIR/test_multi_camera" --duration 30 --no-ui > "$test_results/multi_camera_test.log" 2>&1; then
            log_info "✓ 多相机管理测试通过"
            ((pass_count++))
        else
            log_error "✗ 多相机管理测试失败"
            ((fail_count++))
        fi
        ((test_count++))
    fi
    
    # 相机同步测试
    if [ -f "$BUILD_DIR/test_camera_sync" ]; then
        log_debug "执行相机同步测试..."
        if timeout 300 "$BUILD_DIR/test_camera_sync" --duration 60 > "$test_results/camera_sync_test.log" 2>&1; then
            log_info "✓ 相机同步测试通过"
            ((pass_count++))
        else
            log_error "✗ 相机同步测试失败"
            ((fail_count++))
        fi
        ((test_count++))
    fi
    
    # 生成单元测试报告
    generate_unit_test_report "$test_count" "$pass_count" "$fail_count"
    
    log_info "单元测试完成: $pass_count/$test_count 通过"
    
    if [ $fail_count -gt 0 ]; then
        return 1
    fi
    return 0
}

# 集成测试函数
run_integration_tests() {
    log_info "开始执行集成测试..."
    
    local test_results="$TEST_RESULTS_DIR/integration_tests"
    local test_count=0
    local pass_count=0
    local fail_count=0
    
    # 相机-立体视觉集成测试
    log_info "执行相机-立体视觉集成测试..."
    
    # 端到端图像处理流程测试
    if [ -f "$BUILD_DIR/test_end_to_end" ]; then
        log_debug "执行端到端流程测试..."
        if timeout 600 "$BUILD_DIR/test_end_to_end" > "$test_results/end_to_end_test.log" 2>&1; then
            log_info "✓ 端到端流程测试通过"
            ((pass_count++))
        else
            log_error "✗ 端到端流程测试失败"
            ((fail_count++))
        fi
        ((test_count++))
    fi
    
    # 推理服务集成测试
    if [ -f "$BUILD_DIR/test_inference_integration" ]; then
        log_debug "执行推理服务集成测试..."
        if timeout 300 "$BUILD_DIR/test_inference_integration" > "$test_results/inference_integration_test.log" 2>&1; then
            log_info "✓ 推理服务集成测试通过"
            ((pass_count++))
        else
            log_error "✗ 推理服务集成测试失败"
            ((fail_count++))
        fi
        ((test_count++))
    fi
    
    # 数据流集成测试
    log_debug "执行数据流集成测试..."
    if test_data_flow_integration; then
        log_info "✓ 数据流集成测试通过"
        ((pass_count++))
    else
        log_error "✗ 数据流集成测试失败"
        ((fail_count++))
    fi
    ((test_count++))
    
    # 生成集成测试报告
    generate_integration_test_report "$test_count" "$pass_count" "$fail_count"
    
    log_info "集成测试完成: $pass_count/$test_count 通过"
    
    if [ $fail_count -gt 0 ]; then
        return 1
    fi
    return 0
}

# 性能测试函数
run_performance_tests() {
    log_info "开始执行性能测试..."
    
    local test_results="$TEST_RESULTS_DIR/performance_tests"
    local test_count=0
    local pass_count=0
    local fail_count=0
    
    # 实时处理性能测试
    log_info "执行实时处理性能测试..."
    
    # 图像采集性能测试
    if test_camera_performance; then
        log_info "✓ 图像采集性能测试通过"
        ((pass_count++))
    else
        log_error "✗ 图像采集性能测试失败"
        ((fail_count++))
    fi
    ((test_count++))
    
    # 立体匹配性能测试
    if test_stereo_performance; then
        log_info "✓ 立体匹配性能测试通过"
        ((pass_count++))
    else
        log_error "✗ 立体匹配性能测试失败"
        ((fail_count++))
    fi
    ((test_count++))
    
    # 内存使用测试
    if test_memory_usage; then
        log_info "✓ 内存使用测试通过"
        ((pass_count++))
    else
        log_error "✗ 内存使用测试失败"
        ((fail_count++))
    fi
    ((test_count++))
    
    # 生成性能测试报告
    generate_performance_test_report "$test_count" "$pass_count" "$fail_count"
    
    log_info "性能测试完成: $pass_count/$test_count 通过"
    
    if [ $fail_count -gt 0 ]; then
        return 1
    fi
    return 0
}

# 数据流集成测试
test_data_flow_integration() {
    log_debug "测试配置-模块联动..."
    
    # 创建测试配置文件
    local test_config="$TEST_RESULTS_DIR/test_config.toml"
    cat > "$test_config" << EOF
[app]
name = "Test SmartScope"
version = "1.0.0"
log_level = "debug"

[camera.left]
name = ["testCameraL"]
parameters_path = "test_camera0.dat"

[camera.right]
name = ["testCameraR"]
parameters_path = "test_camera1.dat"
EOF
    
    # 测试配置加载
    if [ -f "$BUILD_DIR/test_config_simple" ]; then
        if timeout 30 "$BUILD_DIR/test_config_simple" -c "$test_config" > "$TEST_RESULTS_DIR/integration_tests/config_integration.log" 2>&1; then
            return 0
        fi
    fi
    
    return 1
}

# 相机性能测试
test_camera_performance() {
    log_debug "测试相机采集性能..."
    
    # 检查是否有可用的相机
    if [ ! -c "/dev/video0" ] && [ ! -c "/dev/video1" ]; then
        log_warn "无可用相机设备，跳过相机性能测试"
        return 0
    fi
    
    # 运行相机性能测试
    if [ -f "$PROJECT_ROOT/camera_test.py" ]; then
        if timeout 120 python3 "$PROJECT_ROOT/camera_test.py" > "$TEST_RESULTS_DIR/performance_tests/camera_performance.log" 2>&1; then
            return 0
        fi
    fi
    
    return 1
}

# 立体匹配性能测试
test_stereo_performance() {
    log_debug "测试立体匹配性能..."
    
    # 检查测试数据
    local test_data_dir="$PROJECT_ROOT/reference_code/lightstereo_inference/test_data"
    if [ ! -d "$test_data_dir" ]; then
        log_warn "测试数据不存在，跳过立体匹配性能测试"
        return 0
    fi
    
    # 运行立体匹配性能测试
    cd "$PROJECT_ROOT/reference_code/lightstereo_inference" || return 1
    if timeout 300 ./run_example.sh > "$TEST_RESULTS_DIR/performance_tests/stereo_performance.log" 2>&1; then
        return 0
    fi
    
    return 1
}

# 内存使用测试
test_memory_usage() {
    log_debug "测试内存使用..."
    
    # 使用valgrind检测内存泄漏
    if [ -f "$BUILD_DIR/SmartScopeQt" ]; then
        if timeout 300 valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
           "$BUILD_DIR/SmartScopeQt" --test-mode > "$TEST_RESULTS_DIR/performance_tests/memory_usage.log" 2>&1; then
            # 检查是否有内存泄漏
            if grep -q "definitely lost: 0 bytes" "$TEST_RESULTS_DIR/performance_tests/memory_usage.log"; then
                return 0
            fi
        fi
    fi
    
    return 1
}

# 生成测试报告函数
generate_unit_test_report() {
    local total=$1
    local passed=$2
    local failed=$3
    
    cat > "$TEST_RESULTS_DIR/reports/unit_test_report.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>单元测试报告</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 10px; }
        .summary { margin: 20px 0; }
        .pass { color: green; }
        .fail { color: red; }
    </style>
</head>
<body>
    <div class="header">
        <h1>SmartScope 单元测试报告</h1>
        <p>生成时间: $(date)</p>
    </div>
    <div class="summary">
        <h2>测试摘要</h2>
        <p>总测试数: $total</p>
        <p class="pass">通过: $passed</p>
        <p class="fail">失败: $failed</p>
        <p>通过率: $(( passed * 100 / total ))%</p>
    </div>
</body>
</html>
EOF
}

generate_integration_test_report() {
    local total=$1
    local passed=$2
    local failed=$3
    
    cat > "$TEST_RESULTS_DIR/reports/integration_test_report.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>集成测试报告</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 10px; }
        .summary { margin: 20px 0; }
        .pass { color: green; }
        .fail { color: red; }
    </style>
</head>
<body>
    <div class="header">
        <h1>SmartScope 集成测试报告</h1>
        <p>生成时间: $(date)</p>
    </div>
    <div class="summary">
        <h2>测试摘要</h2>
        <p>总测试数: $total</p>
        <p class="pass">通过: $passed</p>
        <p class="fail">失败: $failed</p>
        <p>通过率: $(( passed * 100 / total ))%</p>
    </div>
</body>
</html>
EOF
}

generate_performance_test_report() {
    local total=$1
    local passed=$2
    local failed=$3
    
    cat > "$TEST_RESULTS_DIR/reports/performance_test_report.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>性能测试报告</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 10px; }
        .summary { margin: 20px 0; }
        .pass { color: green; }
        .fail { color: red; }
    </style>
</head>
<body>
    <div class="header">
        <h1>SmartScope 性能测试报告</h1>
        <p>生成时间: $(date)</p>
    </div>
    <div class="summary">
        <h2>测试摘要</h2>
        <p>总测试数: $total</p>
        <p class="pass">通过: $passed</p>
        <p class="fail">失败: $failed</p>
        <p>通过率: $(( passed * 100 / total ))%</p>
    </div>
</body>
</html>
EOF
}

# 主函数
main() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  SmartScope 测试执行脚本${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # 记录开始时间
    local start_time=$(date +%s)
    
    # 检查环境
    check_environment
    
    # 构建项目
    build_project
    
    # 执行测试
    local unit_result=0
    local integration_result=0
    local performance_result=0
    
    if ! run_unit_tests; then
        unit_result=1
    fi
    
    if ! run_integration_tests; then
        integration_result=1
    fi
    
    if ! run_performance_tests; then
        performance_result=1
    fi
    
    # 计算总耗时
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # 生成总体报告
    log_info "========================================="
    log_info "测试执行完成"
    log_info "总耗时: ${duration}秒"
    log_info "单元测试: $([ $unit_result -eq 0 ] && echo "通过" || echo "失败")"
    log_info "集成测试: $([ $integration_result -eq 0 ] && echo "通过" || echo "失败")"
    log_info "性能测试: $([ $performance_result -eq 0 ] && echo "通过" || echo "失败")"
    log_info "详细报告位置: $TEST_RESULTS_DIR/reports/"
    log_info "========================================="
    
    # 返回总体结果
    if [ $unit_result -eq 0 ] && [ $integration_result -eq 0 ] && [ $performance_result -eq 0 ]; then
        log_info "所有测试通过！"
        exit 0
    else
        log_error "部分测试失败！"
        exit 1
    fi
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
