//! 测试C++代码中的printf是否被禁用

use rknn_inference::{ImageBuffer, Yolov8Detector};

fn main() {
    println!("开始测试C++打印是否被禁用...");

    // 尝试初始化一个不存在的模型来触发错误路径
    let result = Yolov8Detector::new("nonexistent_model.rknn");

    match result {
        Ok(_) => println!("意外成功"),
        Err(e) => println!("预期的错误: {:?}", e),
    }

    println!("测试完成。如果上面没有看到C++的printf输出，说明禁用成功！");
}