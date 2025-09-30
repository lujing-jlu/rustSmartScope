#!/usr/bin/env python3
"""
深度分析工具
用于分析不同设备的深度计算差异
"""

import cv2 as cv
import numpy as np
import os
import sys

def load_camera_parameters(device_name=""):
    """加载相机参数"""
    try:
        # 加载左相机参数
        left_intrinsics_file = f"camera_parameters/camera0_intrinsics{device_name}.dat"
        if not os.path.exists(left_intrinsics_file):
            left_intrinsics_file = "camera_parameters/camera0_intrinsics.dat"
            
        if not os.path.exists(left_intrinsics_file):
            print(f"错误：找不到左相机内参文件 {left_intrinsics_file}")
            return None
            
        with open(left_intrinsics_file, 'r') as f:
            lines = f.readlines()
        
        # 解析左相机内参
        camera_matrix_left = []
        distortion_left = []
        reading_intrinsic = False
        reading_distortion = False
        
        for line in lines:
            if "intrinsic:" in line:
                reading_intrinsic = True
                reading_distortion = False
                continue
            elif "distortion:" in line:
                reading_intrinsic = False
                reading_distortion = True
                continue
                
            if reading_intrinsic:
                camera_matrix_left.append([float(x) for x in line.split()])
            elif reading_distortion:
                distortion_left = [float(x) for x in line.split()]
        
        camera_matrix_left = np.array(camera_matrix_left)
        distortion_left = np.array([distortion_left])
        
        # 加载右相机参数
        right_intrinsics_file = f"camera_parameters/camera1_intrinsics{device_name}.dat"
        if not os.path.exists(right_intrinsics_file):
            right_intrinsics_file = "camera_parameters/camera1_intrinsics.dat"
            
        if not os.path.exists(right_intrinsics_file):
            print(f"错误：找不到右相机内参文件 {right_intrinsics_file}")
            return None
            
        with open(right_intrinsics_file, 'r') as f:
            lines = f.readlines()
        
        # 解析右相机内参
        camera_matrix_right = []
        distortion_right = []
        reading_intrinsic = False
        reading_distortion = False
        
        for line in lines:
            if "intrinsic:" in line:
                reading_intrinsic = True
                reading_distortion = False
                continue
            elif "distortion:" in line:
                reading_intrinsic = False
                reading_distortion = True
                continue
                
            if reading_intrinsic:
                camera_matrix_right.append([float(x) for x in line.split()])
            elif reading_distortion:
                distortion_right = [float(x) for x in line.split()]
        
        camera_matrix_right = np.array(camera_matrix_right)
        distortion_right = np.array([distortion_right])
        
        # 加载外参
        rot_trans_file = f"camera_parameters/camera1_rot_trans{device_name}.dat"
        if not os.path.exists(rot_trans_file):
            rot_trans_file = "camera_parameters/camera1_rot_trans.dat"
            
        if not os.path.exists(rot_trans_file):
            print(f"错误：找不到旋转平移文件 {rot_trans_file}")
            return None
            
        with open(rot_trans_file, 'r') as f:
            lines = f.readlines()
        
        # 解析旋转矩阵
        rotation_matrix = []
        translation_vector = []
        reading_rotation = False
        reading_translation = False
        
        for line in lines:
            if "R:" in line:
                reading_rotation = True
                reading_translation = False
                continue
            elif "T:" in line:
                reading_rotation = False
                reading_translation = True
                continue
                
            if reading_rotation and line.strip():
                rotation_matrix.append([float(x) for x in line.split()])
            elif reading_translation and line.strip():
                translation_vector.append(float(line.strip()))
        
        rotation_matrix = np.array(rotation_matrix)
        translation_vector = np.array(translation_vector).reshape(3, 1)
        
        return {
            'left_matrix': camera_matrix_left,
            'left_distortion': distortion_left,
            'right_matrix': camera_matrix_right,
            'right_distortion': distortion_right,
            'rotation': rotation_matrix,
            'translation': translation_vector
        }
        
    except Exception as e:
        print(f"加载相机参数时出错: {e}")
        return None

def analyze_depth_factors(params):
    """分析影响深度的关键因素"""
    print("=== 深度影响因素分析 ===")
    
    # 1. 焦距分析
    fx_left = params['left_matrix'][0, 0]
    fy_left = params['left_matrix'][1, 1]
    fx_right = params['right_matrix'][0, 0]
    fy_right = params['right_matrix'][1, 1]
    
    print(f"左相机焦距: fx={fx_left:.2f}, fy={fy_left:.2f}")
    print(f"右相机焦距: fx={fx_right:.2f}, fy={fy_right:.2f}")
    print(f"焦距差异: Δfx={abs(fx_left-fx_right):.2f}, Δfy={abs(fy_left-fy_right):.2f}")
    
    # 2. 基线长度分析
    baseline = np.linalg.norm(params['translation'])
    print(f"基线长度: {baseline:.2f} mm")
    
    # 3. 主点分析
    cx_left = params['left_matrix'][0, 2]
    cy_left = params['left_matrix'][1, 2]
    cx_right = params['right_matrix'][0, 2]
    cy_right = params['right_matrix'][1, 2]
    
    print(f"左相机主点: ({cx_left:.2f}, {cy_left:.2f})")
    print(f"右相机主点: ({cx_right:.2f}, {cy_right:.2f})")
    print(f"主点差异: Δcx={abs(cx_left-cx_right):.2f}, Δcy={abs(cy_left-cy_right):.2f}")
    
    # 4. 畸变系数分析
    print(f"左相机畸变系数: {params['left_distortion'][0]}")
    print(f"右相机畸变系数: {params['right_distortion'][0]}")
    
    # 5. 旋转角度分析
    angles = cv.RQDecomp3x3(params['rotation'])[0]
    print(f"旋转角度 (度): 绕X轴={angles[0]:.2f}, 绕Y轴={angles[1]:.2f}, 绕Z轴={angles[2]:.2f}")
    
    return {
        'focal_length_left': (fx_left + fy_left) / 2,
        'focal_length_right': (fx_right + fy_right) / 2,
        'baseline': baseline,
        'rotation_angles': angles
    }

def calculate_depth_from_disparity(disparity, baseline, focal_length):
    """从视差计算深度"""
    # 避免除零
    disparity = np.where(disparity > 0, disparity, 1e-6)
    
    # 深度公式: Z = baseline * focal_length / disparity
    depth = baseline * focal_length / disparity
    return depth

def simulate_depth_calculation(params, test_disparities=[10, 20, 50, 100]):
    """模拟深度计算"""
    print("\n=== 深度计算模拟 ===")
    
    baseline = params['baseline']
    focal_length = (params['focal_length_left'] + params['focal_length_right']) / 2
    
    print(f"使用参数: 基线={baseline:.2f}mm, 焦距={focal_length:.2f}像素")
    
    for disp in test_disparities:
        depth = calculate_depth_from_disparity(disp, baseline, focal_length)
        print(f"视差={disp}像素 → 深度={depth:.2f}mm")
    
    # 计算深度缩放因子
    print(f"\n深度缩放因子: {baseline * focal_length:.2f}")
    print("注意: 深度与视差成反比，视差越大，深度越小")

def compare_devices(device1_name="", device2_name="_device2"):
    """比较两台设备的参数"""
    print("=== 设备参数比较 ===")
    
    # 加载两台设备的参数
    params1 = load_camera_parameters(device1_name)
    params2 = load_camera_parameters(device2_name)
    
    if params1 is None or params2 is None:
        print("无法加载设备参数，请检查文件是否存在")
        return
    
    # 分析第一台设备
    print(f"\n--- 设备1 ({device1_name if device1_name else '默认'}) ---")
    factors1 = analyze_depth_factors(params1)
    
    # 分析第二台设备
    print(f"\n--- 设备2 ({device2_name}) ---")
    factors2 = analyze_depth_factors(params2)
    
    # 比较关键参数
    print(f"\n--- 参数差异 ---")
    focal_diff = abs(factors1['focal_length_left'] - factors2['focal_length_left'])
    baseline_diff = abs(factors1['baseline'] - factors2['baseline'])
    
    print(f"焦距差异: {focal_diff:.2f}像素")
    print(f"基线长度差异: {baseline_diff:.2f}mm")
    
    # 计算深度计算差异
    if baseline_diff > 0.1 or focal_diff > 1.0:
        print("⚠️  参数差异较大，可能导致深度计算偏差")
        
        # 模拟相同视差下的深度差异
        test_disparity = 50
        depth1 = calculate_depth_from_disparity(test_disparity, factors1['baseline'], factors1['focal_length_left'])
        depth2 = calculate_depth_from_disparity(test_disparity, factors2['baseline'], factors2['focal_length_left'])
        
        print(f"视差{test_disparity}像素时的深度差异:")
        print(f"  设备1: {depth1:.2f}mm")
        print(f"  设备2: {depth2:.2f}mm")
        print(f"  差异: {abs(depth1-depth2):.2f}mm ({abs(depth1-depth2)/depth1*100:.1f}%)")
    
    # 给出建议
    print(f"\n=== 建议 ===")
    if baseline_diff > 5:
        print("1. 基线长度差异较大，检查相机安装距离")
    if focal_diff > 10:
        print("2. 焦距差异较大，可能需要重新标定")
    if abs(factors1['rotation_angles'][1]) > 5 or abs(factors2['rotation_angles'][1]) > 5:
        print("3. 相机Y轴旋转角度较大，可能影响深度精度")

def main():
    print("深度分析工具")
    print("=" * 50)
    
    # 分析当前设备
    params = load_camera_parameters()
    if params:
        factors = analyze_depth_factors(params)
        simulate_depth_calculation(factors)
    
    # 比较两台设备（如果存在）
    if os.path.exists("camera_parameters/camera0_intrinsics_device2.dat"):
        compare_devices()
    else:
        print("\n未找到第二台设备的参数文件")
        print("如需比较，请将第二台设备的参数文件命名为:")
        print("  camera0_intrinsics_device2.dat")
        print("  camera1_intrinsics_device2.dat")
        print("  camera1_rot_trans_device2.dat")

if __name__ == "__main__":
    main() 