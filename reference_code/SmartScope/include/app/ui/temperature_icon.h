#ifndef TEMPERATURE_ICON_H
#define TEMPERATURE_ICON_H

#include <QWidget>
#include <QColor>

namespace SmartScope {

/**
 * @brief 温度图标组件
 * 
 * 显示温度传感器的温度值，采用与电量图标相似的样式
 */
class TemperatureIcon : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit TemperatureIcon(QWidget *parent = nullptr);

    /**
     * @brief 设置温度值
     * @param temperature 温度值（摄氏度）
     */
    void setTemperature(float temperature);

    /**
     * @brief 设置为未检测到状态
     */
    void setNotDetected();

    /**
     * @brief 获取当前温度值
     * @return 当前温度值（摄氏度）
     */
    float getTemperature() const;

protected:
    /**
     * @brief 建议尺寸
     * @return 建议的组件尺寸
     */
    QSize sizeHint() const override;

    /**
     * @brief 绘制事件
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent *event) override;

private:
    /**
     * @brief 根据温度值确定颜色
     * @param temperature 温度值
     * @return 对应的颜色
     */
    QColor getTemperatureColor(float temperature) const;

private:
    float m_temperature;    ///< 当前温度值
    QColor m_color;         ///< 显示颜色
    bool m_notDetected;     ///< 是否为未检测到状态
};

} // namespace SmartScope

#endif // TEMPERATURE_ICON_H 