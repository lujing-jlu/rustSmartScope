#!/usr/bin/env python3
"""
深度校准诊断工具
用于分析深度校准问题，包括：
1. 深度数据质量分析
2. 校准参数优化建议
3. 2D/3D测量一致性检查
4. 坐标转换验证
"""

import cv2
import numpy as np
import matplotlib.pyplot as plt
import argparse
import os
import sys
from pathlib import Path

def load_depth_map(file_path):
    """加载深度图"""
    if not os.path.exists(file_path):
        print(f"错误：找不到深度图文件 {file_path}")
        return None
    
    # 尝试不同的加载方式
    depth_map = cv2.imread(file_path, cv2.IMREAD_UNCHANGED)
    if depth_map is None:
        # 尝试作为numpy数组加载
        try:
            depth_map = np.load(file_path)
        except:
            print(f"错误：无法加载深度图 {file_path}")
            return None
    
    return depth_map

def analyze_depth_quality(depth_map, name="深度图"):
    """分析深度图质量"""
    print(f"\n=== {name} 质量分析 ===")
    
    if depth_map is None:
        print("深度图为空")
        return
    
    # 基本统计信息
    valid_mask = depth_map > 0
    valid_pixels = np.sum(valid_mask)
    total_pixels = depth_map.size
    valid_ratio = valid_pixels / total_pixels * 100
    
    print(f"图像尺寸: {depth_map.shape}")
    print(f"有效像素数: {valid_pixels} / {total_pixels} ({valid_ratio:.1f}%)")
    
    if valid_pixels == 0:
        print("警告：没有有效深度值")
        return
    
    # 深度值统计
    valid_depths = depth_map[valid_mask]
    min_depth = np.min(valid_depths)
    max_depth = np.max(valid_depths)
    mean_depth = np.mean(valid_depths)
    std_depth = np.std(valid_depths)
    
    print(f"深度范围: {min_depth:.1f} - {max_depth:.1f} mm")
    print(f"平均深度: {mean_depth:.1f} mm")
    print(f"深度标准差: {std_depth:.1f} mm")
    
    # 深度分布分析
    hist, bins = np.histogram(valid_depths, bins=50)
    peak_idx = np.argmax(hist)
    peak_depth = (bins[peak_idx] + bins[peak_idx + 1]) / 2
    
    print(f"主要深度: {peak_depth:.1f} mm")
    
    # 噪声分析
    if depth_map.shape[0] > 1 and depth_map.shape[1] > 1:
        # 计算梯度
        grad_x = cv2.Sobel(depth_map, cv2.CV_32F, 1, 0, ksize=3)
        grad_y = cv2.Sobel(depth_map, cv2.CV_32F, 0, 1, ksize=3)
        gradient_magnitude = np.sqrt(grad_x**2 + grad_y**2)
        
        valid_gradients = gradient_magnitude[valid_mask]
        mean_gradient = np.mean(valid_gradients)
        print(f"平均梯度: {mean_gradient:.1f} mm/pixel")
        
        # 高梯度区域比例
        high_gradient_ratio = np.sum(valid_gradients > mean_gradient * 2) / valid_pixels * 100
        print(f"高梯度区域比例: {high_gradient_ratio:.1f}%")
    
    return {
        'valid_ratio': valid_ratio,
        'min_depth': min_depth,
        'max_depth': max_depth,
        'mean_depth': mean_depth,
        'std_depth': std_depth,
        'peak_depth': peak_depth
    }

def compare_depth_maps(mono_depth, stereo_depth, disparity=None):
    """比较单目和双目深度图"""
    print("\n=== 深度图比较分析 ===")
    
    if mono_depth is None or stereo_depth is None:
        print("错误：深度图为空")
        return
    
    # 确保尺寸一致
    if mono_depth.shape != stereo_depth.shape:
        print(f"警告：深度图尺寸不一致 {mono_depth.shape} vs {stereo_depth.shape}")
        # 调整尺寸
        stereo_depth = cv2.resize(stereo_depth, (mono_depth.shape[1], mono_depth.shape[0]))
    
    # 创建有效像素掩码
    mono_valid = mono_depth > 0
    stereo_valid = stereo_depth > 0
    both_valid = mono_valid & stereo_valid
    
    valid_pixels = np.sum(both_valid)
    print(f"共同有效像素数: {valid_pixels}")
    
    if valid_pixels == 0:
        print("警告：没有共同的有效像素")
        return
    
    # 提取有效深度值
    mono_valid_depths = mono_depth[both_valid]
    stereo_valid_depths = stereo_depth[both_valid]
    
    # 计算深度比值
    depth_ratios = stereo_valid_depths / mono_valid_depths
    mean_ratio = np.mean(depth_ratios)
    std_ratio = np.std(depth_ratios)
    
    print(f"深度比值统计:")
    print(f"  均值: {mean_ratio:.3f}")
    print(f"  标准差: {std_ratio:.3f}")
    print(f"  范围: {np.min(depth_ratios):.3f} - {np.max(depth_ratios):.3f}")
    
    # 计算深度差异
    depth_diffs = stereo_valid_depths - mono_valid_depths
    mean_diff = np.mean(depth_diffs)
    std_diff = np.std(depth_diffs)
    max_diff = np.max(np.abs(depth_diffs))
    
    print(f"深度差异统计:")
    print(f"  平均差异: {mean_diff:.1f} mm")
    print(f"  差异标准差: {std_diff:.1f} mm")
    print(f"  最大绝对差异: {max_diff:.1f} mm")
    
    # 分析差异分布
    large_diff_ratio = np.sum(np.abs(depth_diffs) > 50) / valid_pixels * 100
    print(f"大差异像素比例 (>50mm): {large_diff_ratio:.1f}%")
    
    return {
        'mean_ratio': mean_ratio,
        'std_ratio': std_ratio,
        'mean_diff': mean_diff,
        'std_diff': std_diff,
        'max_diff': max_diff,
        'large_diff_ratio': large_diff_ratio
    }

def analyze_calibration_quality(mono_depth, stereo_depth, calibration_result=None):
    """分析校准质量"""
    print("\n=== 校准质量分析 ===")
    
    if calibration_result is None:
        # 简单的线性校准分析
        mono_valid = mono_depth > 0
        stereo_valid = stereo_depth > 0
        both_valid = mono_valid & stereo_valid
        
        if np.sum(both_valid) == 0:
            print("错误：没有有效像素进行校准分析")
            return
        
        mono_valid_depths = mono_depth[both_valid]
        stereo_valid_depths = stereo_depth[both_valid]
        
        # 简单线性拟合
        coeffs = np.polyfit(mono_valid_depths, stereo_valid_depths, 1)
        scale_factor = coeffs[0]
        bias = coeffs[1]
        
        # 计算预测值
        predicted = scale_factor * mono_valid_depths + bias
        residuals = stereo_valid_depths - predicted
        
        rms_error = np.sqrt(np.mean(residuals**2))
        max_error = np.max(np.abs(residuals))
        
        print(f"线性校准参数:")
        print(f"  比例因子: {scale_factor:.4f}")
        print(f"  偏置: {bias:.1f} mm")
        print(f"  RMS误差: {rms_error:.1f} mm")
        print(f"  最大误差: {max_error:.1f} mm")
        
        # 误差分布
        error_hist, _ = np.histogram(residuals, bins=20)
        print(f"误差分布: 最小={np.min(residuals):.1f}, 最大={np.max(residuals):.1f}")
        
        return {
            'scale_factor': scale_factor,
            'bias': bias,
            'rms_error': rms_error,
            'max_error': max_error
        }
    
    return calibration_result

def suggest_calibration_parameters(analysis_results):
    """根据分析结果建议校准参数"""
    print("\n=== 校准参数建议 ===")
    
    if 'std_ratio' in analysis_results:
        std_ratio = analysis_results['std_ratio']
        if std_ratio > 0.2:
            print("建议：深度比值标准差较大，建议降低RANSAC阈值")
            suggested_threshold = max(5.0, std_ratio * 100)
            print(f"  建议RANSAC阈值: {suggested_threshold:.1f} mm")
        else:
            print("深度比值标准差较小，当前RANSAC阈值可能合适")
    
    if 'large_diff_ratio' in analysis_results:
        large_diff_ratio = analysis_results['large_diff_ratio']
        if large_diff_ratio > 20:
            print("建议：大差异像素比例较高，建议启用异常值检测")
            print("  建议异常值阈值: 2.0")
        else:
            print("大差异像素比例较低，异常值检测可能不需要")
    
    if 'rms_error' in analysis_results:
        rms_error = analysis_results['rms_error']
        if rms_error > 20:
            print("建议：RMS误差较大，建议启用分层校准")
            print("  建议深度层数: 5")
        else:
            print("RMS误差较小，线性校准可能足够")

def visualize_depth_comparison(mono_depth, stereo_depth, output_dir="output"):
    """可视化深度图比较"""
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # 创建对比图
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    
    # 单目深度图
    im1 = axes[0, 0].imshow(mono_depth, cmap='jet', vmin=0, vmax=5000)
    axes[0, 0].set_title('单目深度图')
    axes[0, 0].axis('off')
    plt.colorbar(im1, ax=axes[0, 0], label='深度 (mm)')
    
    # 双目深度图
    im2 = axes[0, 1].imshow(stereo_depth, cmap='jet', vmin=0, vmax=5000)
    axes[0, 1].set_title('双目深度图')
    axes[0, 1].axis('off')
    plt.colorbar(im2, ax=axes[0, 1], label='深度 (mm)')
    
    # 深度差异图
    if mono_depth.shape == stereo_depth.shape:
        diff_map = stereo_depth - mono_depth
        im3 = axes[1, 0].imshow(diff_map, cmap='RdBu', vmin=-100, vmax=100)
        axes[1, 0].set_title('深度差异图 (双目 - 单目)')
        axes[1, 0].axis('off')
        plt.colorbar(im3, ax=axes[1, 0], label='差异 (mm)')
    
    # 深度比值图
    if mono_depth.shape == stereo_depth.shape:
        ratio_map = np.zeros_like(mono_depth, dtype=np.float32)
        valid_mask = (mono_depth > 0) & (stereo_depth > 0)
        ratio_map[valid_mask] = stereo_depth[valid_mask] / mono_depth[valid_mask]
        im4 = axes[1, 1].imshow(ratio_map, cmap='viridis', vmin=0.5, vmax=1.5)
        axes[1, 1].set_title('深度比值图 (双目/单目)')
        axes[1, 1].axis('off')
        plt.colorbar(im4, ax=axes[1, 1], label='比值')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'depth_comparison.png'), dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"深度比较图已保存到: {os.path.join(output_dir, 'depth_comparison.png')}")

def main():
    parser = argparse.ArgumentParser(description='深度校准诊断工具')
    parser.add_argument('--mono_depth', type=str, help='单目深度图路径')
    parser.add_argument('--stereo_depth', type=str, help='双目深度图路径')
    parser.add_argument('--disparity', type=str, help='视差图路径（可选）')
    parser.add_argument('--output_dir', type=str, default='output', help='输出目录')
    parser.add_argument('--visualize', action='store_true', help='生成可视化图表')
    
    args = parser.parse_args()
    
    if not args.mono_depth or not args.stereo_depth:
        print("错误：请提供单目和双目深度图路径")
        parser.print_help()
        return
    
    # 加载深度图
    print("加载深度图...")
    mono_depth = load_depth_map(args.mono_depth)
    stereo_depth = load_depth_map(args.stereo_depth)
    disparity = load_depth_map(args.disparity) if args.disparity else None
    
    if mono_depth is None or stereo_depth is None:
        print("错误：无法加载深度图")
        return
    
    # 分析深度质量
    mono_analysis = analyze_depth_quality(mono_depth, "单目深度图")
    stereo_analysis = analyze_depth_quality(stereo_depth, "双目深度图")
    
    # 比较深度图
    comparison_analysis = compare_depth_maps(mono_depth, stereo_depth, disparity)
    
    # 分析校准质量
    calibration_analysis = analyze_calibration_quality(mono_depth, stereo_depth)
    
    # 合并分析结果
    all_analysis = {}
    if mono_analysis:
        all_analysis.update(mono_analysis)
    if stereo_analysis:
        all_analysis.update(stereo_analysis)
    if comparison_analysis:
        all_analysis.update(comparison_analysis)
    if calibration_analysis:
        all_analysis.update(calibration_analysis)
    
    # 建议校准参数
    suggest_calibration_parameters(all_analysis)
    
    # 生成可视化图表
    if args.visualize:
        visualize_depth_comparison(mono_depth, stereo_depth, args.output_dir)
    
    print("\n=== 诊断完成 ===")
    print("建议检查以下方面：")
    print("1. 深度图质量是否满足要求")
    print("2. 单目和双目深度的一致性")
    print("3. 校准参数的合理性")
    print("4. 坐标转换的正确性")

if __name__ == "__main__":
    main()
