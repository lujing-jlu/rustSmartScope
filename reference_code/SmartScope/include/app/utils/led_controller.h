#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <vector>
#include <cstdint>
#include <string>
#include <QObject>

class HIDCommunication;

/**
 * @brief LED灯光控制器类
 * 
 * 该类负责通过HID接口控制设备的LED灯光亮度
 */
class LedController : public QObject
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
     * @brief 获取单例实例
     * @return LedController单例实例
     */
    static LedController& instance();

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
    
    /**
     * @brief 是否连接成功
     * @return 是否连接成功
     */
    bool isConnected() const;
    
    /**
     * @brief 关闭LED控制器
     * 
     * 释放LED控制器占用的资源，关闭HID通信
     */
    void shutdown();

private:
    /**
     * @brief 构造函数
     */
    LedController();
    
    /**
     * @brief 析构函数
     */
    ~LedController();

    // 禁止复制和赋值
    LedController(const LedController&) = delete;
    LedController& operator=(const LedController&) = delete;
    
    /**
     * @brief 初始化HID通信
     */
    void initHidCommunication();
    
    /**
     * @brief 发送灯光控制命令
     * @param brightnessHigh 亮度高字节
     * @param brightnessLow 亮度低字节
     * @return 是否发送成功
     */
    bool sendLightCommand(uint8_t brightnessHigh, uint8_t brightnessLow);

private:
    HIDCommunication* m_hidCommunication;     ///< HID通信实例
    std::vector<LightLevel> m_lightLevels;    ///< 亮度级别列表
    int m_currentLevelIndex;                  ///< 当前亮度级别索引
};

#endif // LED_CONTROLLER_H 