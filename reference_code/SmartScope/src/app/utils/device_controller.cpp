#include "app/utils/device_controller.h"
#include "app/utils/hid_com/hid_communication.h"
#include "infrastructure/logging/logger.h"
#include <QDebug>

// 使用日志宏
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

// 静态成员初始化
uint16_t DeviceController::s_crc16Table[256];
bool DeviceController::s_crc16TableInitialized = false;

// 单例实例化
DeviceController& DeviceController::instance()
{
    static DeviceController instance;
    return instance;
}

DeviceController::DeviceController()
    : m_hidCommunication(nullptr),
      m_currentLevelIndex(0),
      m_updateTimer(nullptr)
{
    // 初始化亮度级别（从最亮到最暗，5个等级：4档亮度+关闭）
    m_lightLevels = {
        {0xFF, 0x04, 100},  // 最大亮度 100%
        {0xBF, 0x03, 75},   // 75% 亮度
        {0x7F, 0x02, 50},   // 50% 亮度
        {0x1F, 0x01, 25},   // 25% 亮度
        {0x00, 0x00, 0}     // 关闭 0%
    };
    
    // 初始化设备状态
    m_lastStatus = DeviceStatus();
    m_lastStatus.temperature = 0.0f;
    m_lastStatus.batteryLevel = 0;
    m_lastStatus.batteryValue = 0.0f;
    m_lastStatus.lightLevel = 0;
    m_lastStatus.isValid = false;
    
    // 初始化设备参数缓存
    m_deviceParams.command = 0;
    m_deviceParams.temperature = 0;
    m_deviceParams.brightLow = m_lightLevels[0].lowByte;
    m_deviceParams.brightHigh = m_lightLevels[0].highByte;
    
    // 初始化CRC16查找表
    if (!s_crc16TableInitialized) {
        initCRC16Table();
        s_crc16TableInitialized = true;
    }
    
    // 创建定时器但不启动
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &DeviceController::updateDeviceStatus);
    
    LOG_INFO("统一设备控制器创建完成");
}

DeviceController::~DeviceController()
{
    shutdown();
    LOG_INFO("统一设备控制器已销毁");
}

bool DeviceController::initialize()
{
    LOG_INFO("初始化统一设备控制器...");
    
    if (m_hidCommunication) {
        LOG_WARNING("设备控制器已经初始化");
        return true;
    }
    
    try {
        // 创建HID通信实例
        m_hidCommunication = new HIDCommunication();
        
        // 尝试打开设备
        if (m_hidCommunication->open()) {
            LOG_INFO("HID设备连接成功: " + QString::fromStdString(m_hidCommunication->getManufacturer()) + 
                     " - " + QString::fromStdString(m_hidCommunication->getProduct()));
            
            // 发射连接状态变化信号
            emit connectionStatusChanged(true);
            
            // 初始化时将亮度设置为最大（第一档）
            m_currentLevelIndex = 0; // 默认最大亮度
            const auto& level = m_lightLevels[m_currentLevelIndex];
            if (sendLightCommand(level.highByte, level.lowByte)) {
                LOG_INFO("初始化时已设置灯光亮度为100%（最大亮度）");
            } else {
                LOG_WARNING("初始化时设置最大亮度失败");
            }
            
            // 读取初始设备状态
            DeviceStatus status = readDeviceStatus();
            if (status.isValid) {
                m_lastStatus = status;
                emit deviceStatusUpdated(status);
                LOG_INFO(QString("初始设备状态 - 温度: %1°C, 电量: %2%")
                        .arg(status.temperature, 0, 'f', 1)
                        .arg(status.batteryLevel));
            }
            
            return true;
        } else {
            LOG_WARNING("无法连接到HID设备");
            delete m_hidCommunication;
            m_hidCommunication = nullptr;
            emit connectionStatusChanged(false);
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("HID通信初始化失败: %1").arg(e.what()));
        if (m_hidCommunication) {
            delete m_hidCommunication;
            m_hidCommunication = nullptr;
        }
        emit connectionStatusChanged(false);
        return false;
    }
}

void DeviceController::shutdown()
{
    LOG_INFO("关闭统一设备控制器...");
    
    // 停止定期更新
    stopPeriodicUpdate();
    
    // 关闭HID通信
    if (m_hidCommunication) {
        m_hidCommunication->close();
        delete m_hidCommunication;
        m_hidCommunication = nullptr;
        emit connectionStatusChanged(false);
    }
}

bool DeviceController::isConnected() const
{
    return m_hidCommunication && m_hidCommunication->isConnected();
}

bool DeviceController::setLightLevel(int levelIndex)
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

int DeviceController::getCurrentLevelIndex() const
{
    return m_currentLevelIndex;
}

int DeviceController::getCurrentBrightnessPercentage() const
{
    if (m_currentLevelIndex >= 0 && m_currentLevelIndex < static_cast<int>(m_lightLevels.size())) {
        return m_lightLevels[m_currentLevelIndex].percentage;
    }
    return 0;
}

bool DeviceController::toggleBrightness()
{
    // 切换到下一个亮度级别
    m_currentLevelIndex = (m_currentLevelIndex + 1) % m_lightLevels.size();
    const auto& level = m_lightLevels[m_currentLevelIndex];
    
    LOG_INFO(QString("切换灯光亮度到级别 %1 (%2%)").arg(m_currentLevelIndex).arg(level.percentage));
    
    return sendLightCommand(level.highByte, level.lowByte);
}

DeviceController::DeviceStatus DeviceController::readDeviceStatus()
{
    // 初始化状态，保留当前亮度级别
    DeviceStatus status = DeviceStatus();
    status.temperature = 0.0f;
    status.batteryLevel = 0;
    status.batteryValue = 0.0f;
    status.lightLevel = m_currentLevelIndex;
    status.isValid = false;
    
    if (!isConnected()) {
        LOG_WARNING("无法读取设备状态: HID设备未连接");
        return status;
    }
    
    try {
        // 准备设备参数，使用当前缓存的参数
        DeviceParams params;
        {
            std::lock_guard<std::mutex> lock(m_paramsMutex);
            params = m_deviceParams; // 复制当前参数
        }
        
        // 确保使用当前亮度设置
        if (m_currentLevelIndex >= 0 && m_currentLevelIndex < static_cast<int>(m_lightLevels.size())) {
            const auto& level = m_lightLevels[m_currentLevelIndex];
            params.brightLow = level.lowByte;
            params.brightHigh = level.highByte;
            
            LOG_DEBUG(QString("读取命令中保留当前亮度: [0x%1, 0x%2]")
                     .arg(level.highByte, 2, 16, QChar('0'))
                     .arg(level.lowByte, 2, 16, QChar('0')));
        }
        
        // 发送读取命令
        std::vector<uint8_t> cmd(32, 0);
        cmd[0] = 0xAA;  // Header
        cmd[1] = 0x55;  // Header
        cmd[2] = 0x01;  // Command (0x01 = 读取)
        cmd[5] = params.brightLow;   // 亮度低字节
        cmd[6] = params.brightHigh;  // 亮度高字节
        
        // 计算CRC16（Header到Payload，30字节）
        uint16_t crc = calculateCRC16(cmd.data(), 30);
        cmd[30] = crc & 0xFF;        // CRC低字节
        cmd[31] = (crc >> 8) & 0xFF; // CRC高字节
        
        LOG_DEBUG("发送读取命令");
        
        // 发送并接收响应
        auto response = m_hidCommunication->sendReceive(cmd);
        
        if (response.size() >= 32) {
            // 解析响应，但保留当前亮度级别
            int savedLightLevel = m_currentLevelIndex;
            status = parseResponse(response);
            
            // 确保亮度级别不会被覆盖
            if (status.lightLevel != savedLightLevel) {
                LOG_DEBUG(QString("保持亮度级别不变: %1").arg(savedLightLevel));
                status.lightLevel = savedLightLevel;
            }
            
            if (status.isValid) {
                // 检查数据是否有变化
                bool temperatureChanged = abs(status.temperature - m_lastStatus.temperature) > 0.1f;
                bool batteryChanged = status.batteryLevel != m_lastStatus.batteryLevel;
                
                // 发射变化信号
                if (temperatureChanged) {
                    emit this->temperatureChanged(status.temperature);
                }
                if (batteryChanged) {
                    emit this->batteryLevelChanged(status.batteryLevel);
                }
                
                // 更新最后状态，但保留亮度级别
                m_lastStatus.temperature = status.temperature;
                m_lastStatus.batteryLevel = status.batteryLevel;
                m_lastStatus.batteryValue = status.batteryValue;
                m_lastStatus.isValid = true;
                
                LOG_DEBUG(QString("读取设备状态成功 - 温度: %1°C, 电量: %2%, 亮度级别: %3")
                         .arg(status.temperature, 0, 'f', 1)
                         .arg(status.batteryValue, 0, 'f', 1)
                         .arg(m_currentLevelIndex));
            }
        } else {
            LOG_WARNING(QString("设备响应数据长度不足: %1 字节").arg(response.size()));
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("读取设备状态异常: %1").arg(e.what()));
    }
    
    return status;
}

float DeviceController::getCurrentTemperature() const
{
    return m_lastStatus.temperature;
}

int DeviceController::getCurrentBatteryLevel() const
{
    return m_lastStatus.batteryLevel;
}

void DeviceController::startPeriodicUpdate(int intervalMs)
{
    if (m_updateTimer && !m_updateTimer->isActive()) {
        m_updateTimer->start(intervalMs);
        LOG_INFO(QString("启动定期状态更新，间隔: %1ms").arg(intervalMs));
    }
}

void DeviceController::stopPeriodicUpdate()
{
    if (m_updateTimer && m_updateTimer->isActive()) {
        m_updateTimer->stop();
        LOG_INFO("停止定期状态更新");
    }
}

void DeviceController::updateDeviceStatus()
{
    DeviceStatus status = readDeviceStatus();
    if (status.isValid) {
        // 保存当前亮度级别
        int currentLightLevel = m_currentLevelIndex;

        // 发送状态更新信号
        emit deviceStatusUpdated(status);



        // 确保亮度级别没有被意外修改
        if (m_currentLevelIndex != currentLightLevel) {
            LOG_WARNING(QString("亮度级别被意外修改，恢复到之前的级别: %1").arg(currentLightLevel));
            m_currentLevelIndex = currentLightLevel;
        }
    }
}

bool DeviceController::sendLightCommand(uint8_t brightnessHigh, uint8_t brightnessLow)
{
    // 准备设备参数
    DeviceParams params;
    {
        std::lock_guard<std::mutex> lock(m_paramsMutex);
        params = m_deviceParams; // 复制当前参数
    }
    
    // 只更新亮度参数
    params.brightHigh = brightnessHigh;
    params.brightLow = brightnessLow;
    
    // 发送写入命令
    LOG_INFO(QString("发送灯光控制命令: 亮度[0x%1, 0x%2]")
            .arg(brightnessHigh, 2, 16, QChar('0'))
            .arg(brightnessLow, 2, 16, QChar('0')));
    
    return sendCommand(0x02, params);
}

bool DeviceController::sendReadCommand()
{
    // 这个方法已经不再需要，readDeviceStatus直接发送命令
    return true;
}

DeviceController::DeviceStatus DeviceController::parseResponse(const std::vector<uint8_t>& response)
{
    DeviceStatus status = DeviceStatus();
    status.temperature = 0.0f;
    status.batteryLevel = 0;
    status.batteryValue = 0.0f;
    status.lightLevel = m_currentLevelIndex;
    status.isValid = false;
    
    if (response.size() < 32) {
        LOG_WARNING("响应数据长度不足");
        return status;
    }
    
    // 检查响应头（根据协议文档：stm32 -> rk3588）
    if (response[0] != 0x55 || response[1] != 0xAA) {
        LOG_WARNING(QString("响应头错误: 0x%1 0x%2").arg(response[0], 2, 16, QChar('0')).arg(response[1], 2, 16, QChar('0')));
        return status;
    }
    
    // 检查命令类型
    uint8_t command = response[2];
    if (command != 0x81 && command != 0x82) {
        LOG_WARNING(QString("响应命令类型错误: 0x%1").arg(command, 2, 16, QChar('0')));
        return status;
    }
    
    // 验证CRC16
    uint16_t receivedCrc = response[30] | (response[31] << 8);
    uint16_t calculatedCrc = calculateCRC16(response.data(), 30);
    if (receivedCrc != calculatedCrc) {
        LOG_WARNING(QString("CRC校验失败: 接收0x%1, 计算0x%2")
                   .arg(receivedCrc, 4, 16, QChar('0'))
                   .arg(calculatedCrc, 4, 16, QChar('0')));
        return status;
    }
    
    // 解析温度（字节偏移3-4，int16_t，摄氏度*10）
    int16_t tempRaw = response[3] | (response[4] << 8);
    status.temperature = tempRaw / 10.0f;
    
    // 解析亮度（字节偏移5-6，uint16_t）
    uint16_t brightnessRaw = response[5] | (response[6] << 8);
    
    // 根据通讯协议，亮度后两位就是电量（数值包含一位小数）
    // 获取亮度值的后两位作为电量
    uint16_t batteryRaw = response[7] | (response[8] << 8);
    float batteryValue = batteryRaw / 10.0f;  // 转换为带一位小数的值
    
    // 直接使用带小数的电量值，而不是四舍五入为整数
    status.batteryValue = batteryValue;  // 保存带一位小数的电量值
    status.batteryLevel = static_cast<int>(batteryValue);  // 整数部分作为兼容的batteryLevel
    
    // 确保电量在有效范围内
    status.batteryValue = qBound(0.0f, status.batteryValue, 100.0f);
    status.batteryLevel = qBound(0, status.batteryLevel, 100);
    
    // 保持当前亮度级别不变
    status.lightLevel = m_currentLevelIndex;
    status.isValid = true;
    
    LOG_DEBUG(QString("解析响应成功 - 温度原始值: %1, 温度: %2°C, 亮度原始值: %3, 电量原始值: %4, 电量: %5%")
             .arg(tempRaw).arg(status.temperature, 0, 'f', 1)
             .arg(brightnessRaw).arg(batteryRaw).arg(status.batteryValue, 0, 'f', 1));
    
    return status;
}

uint16_t DeviceController::calculateCRC16(const uint8_t* data, uint32_t length)
{
    uint16_t crc = 0xFFFF; // 初始值
    for (uint32_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ s_crc16Table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

void DeviceController::initCRC16Table()
{
    uint16_t crc;
    for (int i = 0; i < 256; i++) {
        crc = i << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
        s_crc16Table[i] = crc;
    }
}

bool DeviceController::sendCommand(uint8_t command, const DeviceParams& params)
{
    if (!isConnected()) {
        LOG_WARNING("无法发送命令: HID设备未连接");
        return false;
    }
    
    try {
        // 构建命令数据包
        std::vector<uint8_t> cmd(32, 0);
        cmd[0] = 0xAA;                  // Header
        cmd[1] = 0x55;                  // Header
        cmd[2] = command;               // Command (0x01=读取, 0x02=写入)
        
        // 使用提供的参数填充命令数据
        int16_t tempValue = params.temperature;
        cmd[3] = tempValue & 0xFF;      // 温度低字节
        cmd[4] = (tempValue >> 8) & 0xFF; // 温度高字节
        cmd[5] = params.brightLow;      // 亮度低字节
        cmd[6] = params.brightHigh;     // 亮度高字节
        
        // 计算CRC16（Header到Payload，30字节）
        uint16_t crc = calculateCRC16(cmd.data(), 30);
        cmd[30] = crc & 0xFF;           // CRC低字节
        cmd[31] = (crc >> 8) & 0xFF;    // CRC高字节
        
        // 记录命令内容用于调试
        QString cmdStr;
        if (command == 0x01) {
            cmdStr = QString("读取命令: 保持亮度[0x%1, 0x%2]")
                .arg(params.brightHigh, 2, 16, QChar('0'))
                .arg(params.brightLow, 2, 16, QChar('0'));
        } else if (command == 0x02) {
            cmdStr = QString("写入命令: 设置亮度[0x%1, 0x%2]")
                .arg(params.brightHigh, 2, 16, QChar('0'))
                .arg(params.brightLow, 2, 16, QChar('0'));
        } else {
            cmdStr = QString("未知命令: 0x%1").arg(command, 2, 16, QChar('0'));
        }
        
        LOG_INFO(cmdStr);
        
        // 发送并接收响应
        auto response = m_hidCommunication->sendReceive(cmd);
        
        // 检查响应是否有效
        if (response.size() > 0) {
            LOG_DEBUG(QString("收到命令响应，%1 字节").arg(response.size()));
            
            // 更新设备参数缓存
            std::lock_guard<std::mutex> lock(m_paramsMutex);
            if (command == 0x02) { // 只在写入命令时更新缓存
                m_deviceParams = params;
                m_deviceParams.command = command;
            }
            
            return true;
        } else {
            LOG_WARNING("命令响应为空");
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("发送命令异常: %1").arg(e.what()));
        return false;
    }
} 