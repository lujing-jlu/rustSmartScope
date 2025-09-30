#include "app/utils/led_controller.h"
#include "app/utils/hid_com/hid_communication.h"
#include "infrastructure/logging/logger.h"

// 使用日志宏
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)

// 单例实例化
LedController& LedController::instance()
{
    static LedController instance;
    return instance;
}

LedController::LedController()
    : m_hidCommunication(nullptr),
      m_currentLevelIndex(0)
{
    // 初始化亮度级别（从最亮到最暗，5个等级：4档亮度+关闭）
    m_lightLevels = {
        {0xFF, 0x04, 100},  // 最大亮度 100%
        {0xBF, 0x03, 75},   // 75% 亮度
        {0x7F, 0x02, 50},   // 50% 亮度
        {0x1F, 0x01, 25},   // 25% 亮度
        {0x00, 0x00, 0}     // 关闭 0%
    };
    
    // 初始化HID通信
    initHidCommunication();
    
    LOG_INFO("LED控制器初始化完成");
}

LedController::~LedController()
{
    if (m_hidCommunication) {
        m_hidCommunication->close();
        delete m_hidCommunication;
        m_hidCommunication = nullptr;
    }
    
    LOG_INFO("LED控制器已销毁");
}

void LedController::initHidCommunication()
{
    try {
        // 创建HID通信实例（使用默认的VID和PID）
        m_hidCommunication = new HIDCommunication();
        
        // 尝试打开设备
        if (m_hidCommunication->open()) {
            LOG_INFO("HID设备连接成功: " + QString::fromStdString(m_hidCommunication->getManufacturer()) + 
                     " - " + QString::fromStdString(m_hidCommunication->getProduct()));
            
            // 初始化时将亮度设置为最大（第一档）
            m_currentLevelIndex = 0; // 默认最大亮度
            const auto& level = m_lightLevels[m_currentLevelIndex];
            if (sendLightCommand(level.highByte, level.lowByte)) {
                LOG_INFO(QString("初始化时已设置灯光亮度为100%（最大亮度）"));
            } else {
                LOG_WARNING("初始化时设置最大亮度失败");
            }
        } else {
            LOG_WARNING("无法连接到HID设备");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("HID通信初始化失败: %1").arg(e.what()));
        if (m_hidCommunication) {
            delete m_hidCommunication;
            m_hidCommunication = nullptr;
        }
    }
}

bool LedController::isConnected() const
{
    return m_hidCommunication && m_hidCommunication->isConnected();
}

bool LedController::setLightLevel(int levelIndex)
{
    // 检查索引范围
    if (levelIndex < 0 || levelIndex >= static_cast<int>(m_lightLevels.size())) {
        LOG_WARNING(QString("亮度级别索引超出范围: %1").arg(levelIndex));
        return false;
    }
    
    m_currentLevelIndex = levelIndex;
    const auto& level = m_lightLevels[levelIndex];
    
    LOG_INFO(QString("设置灯光亮度到级别 %1 (%2%)").arg(levelIndex).arg(level.percentage));
    
    return sendLightCommand(level.highByte, level.lowByte);
}

int LedController::getCurrentLevelIndex() const
{
    return m_currentLevelIndex;
}

int LedController::getCurrentBrightnessPercentage() const
{
    if (m_currentLevelIndex >= 0 && m_currentLevelIndex < static_cast<int>(m_lightLevels.size())) {
        return m_lightLevels[m_currentLevelIndex].percentage;
    }
    return 0;
}

bool LedController::toggleBrightness()
{
    // 切换到下一个亮度级别
    m_currentLevelIndex = (m_currentLevelIndex + 1) % m_lightLevels.size();
    const auto& level = m_lightLevels[m_currentLevelIndex];
    
    LOG_INFO(QString("切换灯光亮度到级别 %1 (%2%)").arg(m_currentLevelIndex).arg(level.percentage));
    
    return sendLightCommand(level.highByte, level.lowByte);
}

bool LedController::sendLightCommand(uint8_t brightnessHigh, uint8_t brightnessLow)
{
    if (!isConnected()) {
        LOG_WARNING("无法发送灯光命令: HID设备未连接");
        return false;
    }
    
    try {
        // 构建命令
        std::vector<uint8_t> cmd = {0xAA, 0x55, 0x02, 0x00, 0x00, brightnessHigh, brightnessLow};
        
        // 发送命令
        LOG_INFO(QString("发送灯光控制命令: [0xAA, 0x55, 0x02, 0x00, 0x00, 0x%1, 0x%2]").
            arg(brightnessHigh, 2, 16, QChar('0')).arg(brightnessLow, 2, 16, QChar('0')));
        
        // 发送并接收响应
        auto response = m_hidCommunication->sendReceive(cmd);
        
        // 检查响应是否有效
        if (response.size() > 0) {
            LOG_INFO(QString("收到灯光控制响应，%1 字节").arg(response.size()));
            return true;
        } else {
            LOG_WARNING("灯光控制响应为空");
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("发送灯光命令异常: %1").arg(e.what()));
        return false;
    }
}

void LedController::shutdown()
{
    LOG_INFO("关闭LED控制器...");
    
    // 如果已连接，先尝试将亮度设为0
    if (isConnected()) {
        // 将灯光设置为关闭状态
        int offLevelIndex = m_lightLevels.size() - 1;  // 最后一个级别应该是关闭
        if (setLightLevel(offLevelIndex)) {
            LOG_INFO("已将LED灯光关闭");
        } else {
            LOG_WARNING("无法关闭LED灯光");
        }
        
        // 关闭HID通信
        if (m_hidCommunication) {
            m_hidCommunication->close();
            LOG_INFO("HID通信已关闭");
        }
    }
} 