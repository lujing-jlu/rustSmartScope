#ifndef DEVICE_CONTROLLER_H
#define DEVICE_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <vector>
#include <cstdint>
#include <string>
#include <mutex>

class HIDCommunication;

/**
 * @brief 统一设备控制器类
 * 
 * 该类负责通过HID接口统一管理设备的所有功能：
 * - LED灯光控制
 * - 电量状态读取
 * - 温度传感器读取
 */
class DeviceController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 灯光级别结构
     */
    struct LightLevel {
        uint8_t highByte;    ///< 亮度高字节
        uint8_t lowByte;     ///< 亮度低字节
        int percentage;      ///< 亮度百分比（显示用）
    };

    /**
     * @brief 设备状态结构
     */
    struct DeviceStatus {
        float temperature;   ///< 温度（摄氏度）
        int batteryLevel;    ///< 电量百分比（整数）
        float batteryValue;  ///< 电量百分比（带小数）
        int lightLevel;      ///< 当前灯光级别索引
        bool isValid;        ///< 数据是否有效
        
        DeviceStatus() : temperature(0.0f), batteryLevel(0), batteryValue(0.0f), lightLevel(0), isValid(false) {}
    };

    /**
     * @brief 设备参数缓存结构
     * 用于跟踪设备的所有参数，确保只修改需要修改的参数
     */
    struct DeviceParams {
        uint8_t command;     ///< 命令类型
        int16_t temperature; ///< 温度值（摄氏度*10）
        uint8_t brightLow;   ///< 亮度低字节
        uint8_t brightHigh;  ///< 亮度高字节
        // 可以添加更多参数
        
        DeviceParams() : command(0), temperature(0), brightLow(0), brightHigh(0) {}
    };

    /**
     * @brief 获取单例实例
     * @return DeviceController单例实例
     */
    static DeviceController& instance();

    /**
     * @brief 初始化设备控制器
     * @return 是否初始化成功
     */
    bool initialize();

    /**
     * @brief 关闭设备控制器
     */
    void shutdown();

    /**
     * @brief 是否连接成功
     * @return 是否连接成功
     */
    bool isConnected() const;

    // LED控制相关
    /**
     * @brief 设置灯光亮度
     * @param levelIndex 亮度级别索引
     * @return 是否设置成功
     */
    bool setLightLevel(int levelIndex);
    
    /**
     * @brief 获取当前亮度级别索引
     * @return 当前亮度级别索引
     */
    int getCurrentLevelIndex() const;
    
    /**
     * @brief 获取当前亮度百分比
     * @return 当前亮度百分比
     */
    int getCurrentBrightnessPercentage() const;
    
    /**
     * @brief 切换灯光亮度（在预设值间循环）
     * @return 是否切换成功
     */
    bool toggleBrightness();

    // 状态读取相关
    /**
     * @brief 读取设备状态（温度、电量等）
     * @return 设备状态
     */
    DeviceStatus readDeviceStatus();

    /**
     * @brief 获取最新的温度值
     * @return 温度值（摄氏度）
     */
    float getCurrentTemperature() const;

    /**
     * @brief 获取最新的电量值
     * @return 电量百分比
     */
    int getCurrentBatteryLevel() const;

    /**
     * @brief 启动定期状态更新
     * @param intervalMs 更新间隔（毫秒）
     */
    void startPeriodicUpdate(int intervalMs = 5000);

    /**
     * @brief 停止定期状态更新
     */
    void stopPeriodicUpdate();

signals:
    /**
     * @brief 温度变化信号
     * @param temperature 新的温度值
     */
    void temperatureChanged(float temperature);

    /**
     * @brief 电量变化信号
     * @param level 新的电量百分比
     */
    void batteryLevelChanged(int level);

    /**
     * @brief 设备状态更新信号
     * @param status 设备状态
     */
    void deviceStatusUpdated(const DeviceStatus& status);

    /**
     * @brief 设备连接状态变化信号
     * @param connected 是否连接
     */
    void connectionStatusChanged(bool connected);

private slots:
    /**
     * @brief 定期更新设备状态
     */
    void updateDeviceStatus();

private:
    /**
     * @brief 构造函数
     */
    DeviceController();
    
    /**
     * @brief 析构函数
     */
    ~DeviceController();

    // 禁止复制和赋值
    DeviceController(const DeviceController&) = delete;
    DeviceController& operator=(const DeviceController&) = delete;
    
    /**
     * @brief 发送命令到设备
     * @param command 命令类型（0x01=读取，0x02=写入）
     * @param params 设备参数
     * @return 是否发送成功
     */
    bool sendCommand(uint8_t command, const DeviceParams& params);
    
    /**
     * @brief 发送灯光控制命令
     * @param brightnessHigh 亮度高字节
     * @param brightnessLow 亮度低字节
     * @return 是否发送成功
     */
    bool sendLightCommand(uint8_t brightnessHigh, uint8_t brightnessLow);

    /**
     * @brief 发送读取命令
     * @return 是否发送成功
     */
    bool sendReadCommand();

    /**
     * @brief 解析设备响应数据
     * @param response 响应数据
     * @return 解析的设备状态
     */
    DeviceStatus parseResponse(const std::vector<uint8_t>& response);

    /**
     * @brief 计算CRC16校验
     * @param data 数据
     * @param length 数据长度
     * @return CRC16值
     */
    uint16_t calculateCRC16(const uint8_t* data, uint32_t length);

    /**
     * @brief 初始化CRC16查找表
     */
    void initCRC16Table();

private:
    HIDCommunication* m_hidCommunication;     ///< HID通信实例
    std::vector<LightLevel> m_lightLevels;    ///< 亮度级别列表
    int m_currentLevelIndex;                  ///< 当前亮度级别索引
    
    DeviceStatus m_lastStatus;                ///< 最后读取的设备状态
    DeviceParams m_deviceParams;              ///< 设备参数缓存
    std::mutex m_paramsMutex;                 ///< 参数访问互斥锁
    
    QTimer* m_updateTimer;                    ///< 定期更新定时器
    
    static uint16_t s_crc16Table[256];        ///< CRC16查找表
    static bool s_crc16TableInitialized;      ///< CRC16表是否已初始化
};

#endif // DEVICE_CONTROLLER_H 