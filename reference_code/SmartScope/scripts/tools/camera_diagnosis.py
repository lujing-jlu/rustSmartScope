#!/usr/bin/env python3
"""
相机诊断工具
用于分析左右相机ROI不一致的问题
"""

import cv2 as cv
import numpy as np
import os
import sys

def load_camera_parameters():
    """加载相机参数"""
    try:
        # 加载左相机参数
        left_intrinsics_file = "camera_parameters/camera0_intrinsics.dat"
        if not os.path.exists(left_intrinsics_file):
            print(f"错误：找不到左相机内参文件 {left_intrinsics_file}")
            return None, None, None, None
            
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
        right_intrinsics_file = "camera_parameters/camera1_intrinsics.dat"
        if not os.path.exists(right_intrinsics_file):
            print(f"错误：找不到右相机内参文件 {right_intrinsics_file}")
            return None, None, None, None
            
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
        rot_trans_file = "camera_parameters/rot_trans.dat"
        if not os.path.exists(rot_trans_file):
            print(f"错误：找不到旋转平移文件 {rot_trans_file}")
            return None, None, None, None
            
        with open(rot_trans_file, 'r') as f:
            lines = f.readlines()
        
        # 解析旋转矩阵
        rotation_matrix = []
        for i in range(3):
            rotation_matrix.append([float(x) for x in lines[i].split()])
        rotation_matrix = np.array(rotation_matrix)
                        
        # 解析平移向量
        translation_vector = []
        for i in range(3):
            translation_vector.append(float(lines[i+3]))
        translation_vector = np.array(translation_vector).reshape(3, 1)
        
        return camera_matrix_left, distortion_left, camera_matrix_right, distortion_right, rotation_matrix, translation_vector
                
            except Exception as e:
        print(f"加载相机参数时出错: {e}")
        return None, None, None, None, None, None

def analyze_roi_consistency():
    """分析ROI一致性"""
    print("=== 相机ROI一致性分析 ===")
    
    # 加载相机参数
    params = load_camera_parameters()
    if params[0] is None:
        return
    
    cmtx_left, dist_left, cmtx_right, dist_right, R, T = params
    
    print(f"左相机内参矩阵:\n{cmtx_left}")
    print(f"左相机畸变系数: {dist_left}")
    print(f"右相机内参矩阵:\n{cmtx_right}")
    print(f"右相机畸变系数: {dist_right}")
    print(f"旋转矩阵:\n{R}")
    print(f"平移向量:\n{T}")
    
    # 测试不同图像尺寸
    test_sizes = [(1280, 720), (1920, 1080), (640, 480)]
    
    for width, height in test_sizes:
        print(f"\n--- 测试图像尺寸: {width}x{height} ---")
        
        # 计算立体校正参数
        R1, R2, P1, P2, Q, roi1, roi2 = cv.stereoRectify(
            cmtx_left, dist_left, cmtx_right, dist_right,
            (width, height), R, T, alpha=-1.0
        )
        
        print(f"左相机ROI: {roi1}")
        print(f"右相机ROI: {roi2}")
        
        # 检查ROI一致性
        if roi1[2] != roi2[2] or roi1[3] != roi2[3]:
            print("⚠️  ROI尺寸不一致！")
            print(f"   左相机ROI尺寸: {roi1[2]}x{roi1[3]}")
            print(f"   右相机ROI尺寸: {roi2[2]}x{roi2[3]}")
    
            # 计算重叠区域
            left_x1, left_y1 = roi1[0], roi1[1]
            left_x2, left_y2 = left_x1 + roi1[2], left_y1 + roi1[3]
            
            right_x1, right_y1 = roi2[0], roi2[1]
            right_x2, right_y2 = right_x1 + roi2[2], right_y1 + roi2[3]
            
            overlap_x1 = max(left_x1, right_x1)
            overlap_y1 = max(left_y1, right_y1)
            overlap_x2 = min(left_x2, right_x2)
            overlap_y2 = min(left_y2, right_y2)
            
            if overlap_x2 > overlap_x1 and overlap_y2 > overlap_y1:
                unified_width = overlap_x2 - overlap_x1
                unified_height = overlap_y2 - overlap_y1
                print(f"   重叠区域: ({overlap_x1}, {overlap_y1}, {unified_width}, {unified_height})")
            else:
                min_width = min(roi1[2], roi2[2])
                min_height = min(roi1[3], roi2[3])
                print(f"   建议统一尺寸: {min_width}x{min_height}")
        else:
            print("✅ ROI尺寸一致")
    
    print("\n=== 分析完成 ===")

def check_camera_alignment():
    """检查相机对齐情况"""
    print("\n=== 相机对齐检查 ===")
    
    params = load_camera_parameters()
    if params[0] is None:
        return
    
    cmtx_left, dist_left, cmtx_right, dist_right, R, T = params
    
    # 分析主点差异
    cx_left = cmtx_left[0, 2]
    cy_left = cmtx_left[1, 2]
    cx_right = cmtx_right[0, 2]
    cy_right = cmtx_right[1, 2]
    
    print(f"左相机主点: ({cx_left:.2f}, {cy_left:.2f})")
    print(f"右相机主点: ({cx_right:.2f}, {cy_right:.2f})")
    print(f"主点差异: Δcx={abs(cx_left-cx_right):.2f}, Δcy={abs(cy_left-cy_right):.2f}")
    
    # 分析焦距差异
    fx_left = cmtx_left[0, 0]
    fy_left = cmtx_left[1, 1]
    fx_right = cmtx_right[0, 0]
    fy_right = cmtx_right[1, 1]
    
    print(f"左相机焦距: fx={fx_left:.2f}, fy={fy_left:.2f}")
    print(f"右相机焦距: fx={fx_right:.2f}, fy={fy_right:.2f}")
    print(f"焦距差异: Δfx={abs(fx_left-fx_right):.2f}, Δfy={abs(fy_left-fy_right):.2f}")
    
    # 分析基线长度
    baseline = np.linalg.norm(T)
    print(f"基线长度: {baseline:.2f} mm")
    
    # 分析旋转角度
    # 从旋转矩阵提取欧拉角
    angles = cv.RQDecomp3x3(R)[0]
    print(f"旋转角度 (度): 绕X轴={angles[0]:.2f}, 绕Y轴={angles[1]:.2f}, 绕Z轴={angles[2]:.2f}")
    
    # 给出建议
    print("\n=== 建议 ===")
    if abs(cx_left-cx_right) > 50 or abs(cy_left-cy_right) > 50:
        print("⚠️  主点差异过大，建议重新标定")
    if abs(fx_left-fx_right) > 20 or abs(fy_left-fy_right) > 20:
        print("⚠️  焦距差异过大，可能影响立体匹配精度")
    if baseline < 10 or baseline > 200:
        print("⚠️  基线长度异常，请检查相机安装距离")
    
    print("✅ 如果以上检查都通过，ROI不一致可能是正常的，建议使用尺寸一致性调整")

if __name__ == "__main__":
    print("相机诊断工具")
    print("=" * 50)
    
    analyze_roi_consistency()
    check_camera_alignment()
