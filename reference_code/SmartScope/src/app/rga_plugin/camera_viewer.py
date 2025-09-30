#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USB摄像头查看器
支持查看指定的摄像头设备并在窗口中显示
"""

import cv2
import argparse
import sys
import os
from typing import Optional

class CameraViewer:
    def __init__(self, device_id: int = 1, width: int = 640, height: int = 480):
        """
        初始化摄像头查看器
        
        Args:
            device_id: 摄像头设备ID (0, 1, 2...)
            width: 视频宽度
            height: 视频高度
        """
        self.device_id = device_id
        self.width = width
        self.height = height
        self.cap = None
        self.window_name = f"Camera /dev/video{device_id}"
        
    def check_device_exists(self) -> bool:
        """检查摄像头设备是否存在"""
        device_path = f"/dev/video{self.device_id}"
        return os.path.exists(device_path)
    
    def list_available_cameras(self):
        """列出所有可用的摄像头设备"""
        print("可用的摄像头设备:")
        for i in range(10):  # 检查前10个设备
            device_path = f"/dev/video{i}"
            if os.path.exists(device_path):
                print(f"  /dev/video{i}")
    
    def open_camera(self) -> bool:
        """打开摄像头"""
        try:
            self.cap = cv2.VideoCapture(self.device_id)
            if not self.cap.isOpened():
                print(f"错误: 无法打开摄像头 /dev/video{self.device_id}")
                return False
            
            # 设置分辨率
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
            
            # 获取实际分辨率
            actual_width = int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            actual_height = int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            
            print(f"摄像头 /dev/video{self.device_id} 已打开")
            print(f"分辨率: {actual_width}x{actual_height}")
            return True
            
        except Exception as e:
            print(f"打开摄像头时出错: {e}")
            return False
    
    def get_camera_info(self):
        """获取摄像头信息"""
        if not self.cap:
            return
        
        print("\n摄像头信息:")
        print(f"设备ID: {self.device_id}")
        print(f"分辨率: {int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH))}x{int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT))}")
        print(f"帧率: {self.cap.get(cv2.CAP_PROP_FPS):.2f} FPS")
        print(f"格式: {int(self.cap.get(cv2.CAP_PROP_FORMAT))}")
        print(f"亮度: {self.cap.get(cv2.CAP_PROP_BRIGHTNESS)}")
        print(f"对比度: {self.cap.get(cv2.CAP_PROP_CONTRAST)}")
        print(f"饱和度: {self.cap.get(cv2.CAP_PROP_SATURATION)}")
        print(f"色调: {self.cap.get(cv2.CAP_PROP_HUE)}")
    
    def run(self):
        """运行摄像头查看器"""
        # 检查设备是否存在
        if not self.check_device_exists():
            print(f"错误: 摄像头设备 /dev/video{self.device_id} 不存在")
            self.list_available_cameras()
            return False
        
        # 打开摄像头
        if not self.open_camera():
            return False
        
        # 获取摄像头信息
        self.get_camera_info()
        
        # 创建窗口
        cv2.namedWindow(self.window_name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(self.window_name, self.width, self.height)
        
        print(f"\n按 'q' 键退出，按 's' 键保存图片")
        print("开始显示摄像头画面...")
        
        frame_count = 0
        
        try:
            while True:
                # 读取帧
                ret, frame = self.cap.read()
                if not ret:
                    print("错误: 无法读取摄像头帧")
                    break
                
                frame_count += 1
                
                # 在帧上添加信息
                info_text = f"Device: /dev/video{self.device_id} | Frame: {frame_count}"
                cv2.putText(frame, info_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 
                           0.7, (0, 255, 0), 2)
                
                # 显示帧
                cv2.imshow(self.window_name, frame)
                
                # 处理按键
                key = cv2.waitKey(1) & 0xFF
                if key == ord('q'):
                    print("用户退出")
                    break
                elif key == ord('s'):
                    # 保存图片
                    filename = f"camera_{self.device_id}_frame_{frame_count}.jpg"
                    cv2.imwrite(filename, frame)
                    print(f"图片已保存: {filename}")
                elif key == ord('i'):
                    # 显示摄像头信息
                    self.get_camera_info()
                
        except KeyboardInterrupt:
            print("\n程序被中断")
        except Exception as e:
            print(f"运行时出错: {e}")
        finally:
            self.cleanup()
        
        return True
    
    def cleanup(self):
        """清理资源"""
        if self.cap:
            self.cap.release()
        cv2.destroyAllWindows()
        print("摄像头已关闭")

def main():
    parser = argparse.ArgumentParser(description="USB摄像头查看器")
    parser.add_argument("-d", "--device", type=int, default=1, 
                       help="摄像头设备ID (默认: 1)")
    parser.add_argument("-w", "--width", type=int, default=640,
                       help="视频宽度 (默认: 640)")
    parser.add_argument("--height", type=int, default=480,
                       help="视频高度 (默认: 480)")
    parser.add_argument("-l", "--list", action="store_true",
                       help="列出所有可用的摄像头设备")
    
    args = parser.parse_args()
    
    # 检查OpenCV是否可用
    try:
        import cv2
    except ImportError:
        print("错误: 需要安装OpenCV")
        print("安装命令: pip install opencv-python")
        sys.exit(1)
    
    # 列出可用设备
    if args.list:
        viewer = CameraViewer()
        viewer.list_available_cameras()
        return
    
    # 创建并运行摄像头查看器
    viewer = CameraViewer(args.device, args.width, args.height)
    viewer.run()

if __name__ == "__main__":
    main() 