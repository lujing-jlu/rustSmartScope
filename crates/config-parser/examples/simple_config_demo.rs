use config_parser::{AppConfig, ConfigParser};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("=== SmartScope 简化配置管理示例 ===\n");

    // 创建默认配置
    let mut config = AppConfig::default();
    println!("1. 默认配置:");
    println!("   左相机搜索关键词: {:?}", config.camera.left.search_keywords);
    println!("   左相机格式: {}", config.camera.left.format);
    println!("   左相机帧率: {}", config.camera.left.frame_rate);
    println!("   左相机分辨率: {}x{}", config.camera.left.resolution.width, config.camera.left.resolution.height);
    println!();
    println!("   右相机搜索关键词: {:?}", config.camera.right.search_keywords);
    println!("   右相机格式: {}", config.camera.right.format);
    println!("   右相机帧率: {}", config.camera.right.frame_rate);
    println!("   右相机分辨率: {}x{}", config.camera.right.resolution.width, config.camera.right.resolution.height);
    println!();

    // 自定义配置
    println!("2. 自定义配置:");
    config.camera.left.add_search_keyword("mycameraL".to_string());
    config.camera.right.add_search_keyword("mycameraR".to_string());
    
    // 修改分辨率和帧率
    config.camera.left.resolution.width = 1920;
    config.camera.left.resolution.height = 1080;
    config.camera.left.frame_rate = 60;
    config.camera.left.format = "YUYV".to_string();
    
    println!("   更新后左相机搜索关键词: {:?}", config.camera.left.search_keywords);
    println!("   更新后左相机格式: {}", config.camera.left.format);
    println!("   更新后左相机帧率: {}", config.camera.left.frame_rate);
    println!("   更新后左相机分辨率: {}x{}", config.camera.left.resolution.width, config.camera.left.resolution.height);
    println!();

    // 测试关键词匹配
    println!("3. 关键词匹配测试:");
    let test_devices = vec![
        "USB Camera Left",
        "mycameraL_device", 
        "cameraR_controller",
        "hdmirx_device",
        "camera_usb",
    ];
    
    for device in &test_devices {
        let left_matches = config.camera.left.matches_keyword(device);
        let right_matches = config.camera.right.matches_keyword(device);
        println!("   '{}' -> 左相机: {}, 右相机: {}", 
                device, 
                if left_matches { "✅ 匹配" } else { "❌ 不匹配" },
                if right_matches { "✅ 匹配" } else { "❌ 不匹配" }
        );
    }
    println!();

    // 保存配置文件
    println!("4. 保存配置文件:");
    let json_path = "/tmp/simple_camera_config.json";
    ConfigParser::save_to_file(&config, json_path)?;
    println!("   JSON配置已保存到: {}", json_path);

    // 从文件加载并验证
    println!("5. 加载和验证配置:");
    let loaded_config = ConfigParser::load_from_file(json_path)?;
    
    match ConfigParser::validate_config(&loaded_config) {
        Ok(()) => println!("   ✅ 配置验证通过"),
        Err(e) => println!("   ✗ 配置验证失败: {}", e),
    }
    println!();

    // 展示配置结构
    println!("6. 完整配置结构:");
    let json_str = ConfigParser::serialize_config(&config, config_parser::ConfigFormat::Json)?;
    println!("{}", json_str);

    // 清理临时文件
    let _ = std::fs::remove_file(json_path);
    
    println!("\n=== 示例完成 ===");
    Ok(())
}
