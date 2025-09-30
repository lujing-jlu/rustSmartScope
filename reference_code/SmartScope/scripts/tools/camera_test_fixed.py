#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修正版相机测试脚本
基于实际的设备映射进行测试
"""

import cv2
import numpy as np
import time
import sys

def test_camera_direct(device_path, camera_name, window_name, position=(0, 0)):
    """直接测试指定的相机设备"""
    print(f"正在测试 {camera_name} ({device_path})...")
    
    # 尝试不同的方式打开设备
    cap = None
    
    # 方法1: 直接使用设备路径
    try:
        cap = cv2.VideoCapture(device_path)
        if cap.isOpened():
            ret, frame = cap.read()
            if ret and frame is not None:
                print(f"  ✅ 直接路径成功: {frame.shape[1]}x{frame.shape[0]}")
            else:
                cap.release()
                cap = None
    except Exception as e:
        print(f"  ❌ 直接路径失败: {e}")
        cap = None
    
    # 方法2: 使用V4L2后端
    if cap is None:
        try:
            cap = cv2.VideoCapture(device_path, cv2.CAP_V4L2)
            if cap.isOpened():
                ret, frame = cap.read()
                if ret and frame is not None:
                    print(f"  ✅ V4L2后端成功: {frame.shape[1]}x{frame.shape[0]}")
                else:
                    cap.release()
                    cap = None
        except Exception as e:
            print(f"  ❌ V4L2后端失败: {e}")
            cap = None
    
    if cap is None:
        print(f"  ❌ 无法打开 {camera_name}")
        return None
    
    # 设置相机参数
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    cap.set(cv2.CAP_PROP_FPS, 30)
    
    # 获取实际参数
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    
    print(f"  参数: {width}x{height} @ {fps}fps")
    
    # 创建窗口
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    cv2.resizeWindow(window_name, 640, 480)
    cv2.moveWindow(window_name, position[0], position[1])
    
    return cap

def main():
    print("=== 修正版相机测试脚本 ===")
    print("基于实际设备映射:")
    print("  左相机: /dev/video3")
    print("  右相机: /dev/video1")
    print()
    print("按 'q' 退出程序")
    print("按 's' 保存当前帧")
    print("按 'r' 重新初始化相机")
    print()
    
    # 初始化相机
    cap_left = test_camera_direct("/dev/video3", "左相机", "左相机", (0, 0))
    cap_right = test_camera_direct("/dev/video1", "右相机", "右相机", (660, 0))
    
    if cap_left is None and cap_right is None:
        print("错误: 无法初始化任何相机")
        return
    
    # 统计信息
    frame_count = 0
    start_time = time.time()
    last_fps_time = start_time
    left_success = 0
    right_success = 0
    
    print("\n开始读取相机画面...")
    
    try:
        while True:
            current_time = time.time()
            
            # 读取左相机
            frame_left = None
            if cap_left is not None:
                ret_left, frame_left = cap_left.read()
                if ret_left and frame_left is not None:
                    left_success += 1
                else:
                    frame_left = np.zeros((480, 640, 3), dtype=np.uint8)
                    cv2.putText(frame_left, "Left Camera Error", (50, 240), 
                               cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            else:
                frame_left = np.zeros((480, 640, 3), dtype=np.uint8)
                cv2.putText(frame_left, "Left Camera Not Available", (20, 240), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.8, (128, 128, 128), 2)
            
            # 读取右相机
            frame_right = None
            if cap_right is not None:
                ret_right, frame_right = cap_right.read()
                if ret_right and frame_right is not None:
                    right_success += 1
                else:
                    frame_right = np.zeros((480, 640, 3), dtype=np.uint8)
                    cv2.putText(frame_right, "Right Camera Error", (50, 240), 
                               cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            else:
                frame_right = np.zeros((480, 640, 3), dtype=np.uint8)
                cv2.putText(frame_right, "Right Camera Not Available", (20, 240), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.8, (128, 128, 128), 2)
            
            # 添加信息到画面
            frame_count += 1
            
            # 计算FPS
            if current_time - last_fps_time >= 1.0:
                fps = frame_count / (current_time - start_time)
                left_rate = (left_success / frame_count) * 100 if frame_count > 0 else 0
                right_rate = (right_success / frame_count) * 100 if frame_count > 0 else 0
                last_fps_time = current_time
                print(f"FPS: {fps:.1f}, 左相机成功率: {left_rate:.1f}%, 右相机成功率: {right_rate:.1f}%")
            
            # 在画面上显示信息
            info_text = f"Frame: {frame_count}, Time: {current_time - start_time:.1f}s"
            
            if frame_left is not None:
                cv2.putText(frame_left, info_text, (10, 30), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.putText(frame_left, "Left Camera (/dev/video3)", (10, 60), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
                cv2.imshow("左相机", frame_left)
            
            if frame_right is not None:
                cv2.putText(frame_right, info_text, (10, 30), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.putText(frame_right, "Right Camera (/dev/video1)", (10, 60), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
                cv2.imshow("右相机", frame_right)
            
            # 处理按键
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                print("用户退出")
                break
            elif key == ord('s'):
                # 保存当前帧
                timestamp = time.strftime("%Y%m%d_%H%M%S")
                if frame_left is not None:
                    cv2.imwrite(f"left_camera_{timestamp}.jpg", frame_left)
                if frame_right is not None:
                    cv2.imwrite(f"right_camera_{timestamp}.jpg", frame_right)
                print(f"已保存图片: {timestamp}")
            elif key == ord('r'):
                # 重新初始化相机
                print("重新初始化相机...")
                if cap_left:
                    cap_left.release()
                if cap_right:
                    cap_right.release()
                
                time.sleep(1)
                
                cap_left = test_camera_direct("/dev/video3", "左相机", "左相机", (0, 0))
                cap_right = test_camera_direct("/dev/video1", "右相机", "右相机", (660, 0))
    
    except KeyboardInterrupt:
        print("\n程序被中断")
    
    except Exception as e:
        print(f"程序异常: {e}")
    
    finally:
        # 清理资源
        if cap_left:
            cap_left.release()
        if cap_right:
            cap_right.release()
        cv2.destroyAllWindows()
        
        # 显示统计信息
        total_time = time.time() - start_time
        avg_fps = frame_count / total_time if total_time > 0 else 0
        left_rate = (left_success / frame_count) * 100 if frame_count > 0 else 0
        right_rate = (right_success / frame_count) * 100 if frame_count > 0 else 0
        
        print(f"\n=== 测试完成 ===")
        print(f"总运行时间: {total_time:.1f}秒")
        print(f"总帧数: {frame_count}")
        print(f"平均FPS: {avg_fps:.1f}")
        print(f"左相机成功率: {left_rate:.1f}% ({left_success}/{frame_count})")
        print(f"右相机成功率: {right_rate:.1f}% ({right_success}/{frame_count})")

if __name__ == "__main__":
    main()
