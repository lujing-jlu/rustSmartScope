#include "app/ui/temperature_icon.h"
#include <QPainter>
#include <QPaintEvent>
#include <QFont>

namespace SmartScope {

TemperatureIcon::TemperatureIcon(QWidget *parent)
    : QWidget(parent), m_temperature(0.0f), m_color(QColor("#666666")), m_notDetected(true)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // 初始化为未检测到状态（灰色）
    m_color = QColor("#666666");
}

void TemperatureIcon::setTemperature(float temperature)
{
    // 如果温度值为负数，隐藏整个温度显示区域
    if (temperature < 0) {
        if (!isHidden()) {
            hide();
        }
        return;
    }

    // 如果之前因为负温度被隐藏，现在需要显示
    if (isHidden()) {
        show();
    }

    if (qAbs(m_temperature - temperature) > 0.1f || m_notDetected) {

        m_temperature = temperature;
        m_notDetected = false;
        m_color = getTemperatureColor(temperature);

        update(); // 触发重绘
    }
}

void TemperatureIcon::setNotDetected()
{
    // 确保在"未检测到"状态下显示组件
    if (isHidden()) {
        show();
    }

    m_notDetected = true;
    m_temperature = 0.0f;
    m_color = QColor("#666666"); // 灰色表示未检测到

    update(); // 触发重绘
}

float TemperatureIcon::getTemperature() const
{
    return m_temperature;
}

QSize TemperatureIcon::sizeHint() const
{
    // 与电量图标保持一致的尺寸
    return QSize(180, 60);
}

void TemperatureIcon::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 计算温度计图标和文本的布局
    int thermometerWidth = 24;
    int thermometerHeight = height() - 24;
    int textWidth = width() - thermometerWidth - 20;

    if (m_notDetected) {
        // 未检测到状态：绘制带问号的温度计图标（白色）
        QRectF thermometerBody(8, 12, thermometerWidth - 8, thermometerHeight - 12);
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::transparent);
        painter.drawRoundedRect(thermometerBody, 6, 6);

        // 绘制温度计底部圆球（白色）
        QRectF thermometerBulb(5, height() - 20, thermometerWidth - 2, 16);
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(thermometerBulb);

        // 在温度计中心绘制问号（白色）
        painter.setPen(Qt::white);
        painter.setFont(QFont("WenQuanYi Zen Hei", 12, QFont::Bold));
        painter.drawText(thermometerBody, Qt::AlignCenter, "?");

        // 绘制温度计刻度线（白色）
        painter.setPen(QPen(Qt::white, 1));
        for (int i = 1; i < 5; i++) {
            int y = 12 + (thermometerHeight - 24) * i / 5;
            painter.drawLine(thermometerWidth + 2, y, thermometerWidth + 6, y);
        }
    } else {
        // 正常状态：绘制温度计图标
        QRectF thermometerBody(8, 12, thermometerWidth - 8, thermometerHeight - 12);
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::transparent);
        painter.drawRoundedRect(thermometerBody, 6, 6);

        // 绘制温度计底部圆球
        QRectF thermometerBulb(5, height() - 20, thermometerWidth - 2, 16);
        painter.setBrush(m_color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(thermometerBulb);

        // 绘制温度计内部液体（根据温度高低填充不同高度）
        // 温度范围假设：0°C到60°C，正常工作温度20-40°C
        float normalizedTemp = qBound(0.0f, (m_temperature - 0.0f) / 60.0f, 1.0f);
        float liquidHeight = (thermometerHeight - 24) * normalizedTemp;

        if (liquidHeight > 0) {
            QRectF liquidRect(11, height() - 16 - liquidHeight, thermometerWidth - 14, liquidHeight);
            painter.setBrush(m_color);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(liquidRect, 2, 2);
        }

        // 绘制温度计刻度线（简单版本）
        painter.setPen(QPen(Qt::white, 1));
        for (int i = 1; i < 5; i++) {
            int y = 12 + (thermometerHeight - 24) * i / 5;
            painter.drawLine(thermometerWidth + 2, y, thermometerWidth + 6, y);
        }
    }

    // 绘制温度值文本或未检测到状态
    QString displayText;
    if (m_notDetected) {
        displayText = "未检测到";
        painter.setPen(Qt::white); // 白色文字
        painter.setFont(QFont("WenQuanYi Zen Hei", 20, QFont::Normal)); // 稍小的字体
    } else {
        displayText = QString::number(m_temperature, 'f', 1) + "°C";
        painter.setPen(Qt::white);
        painter.setFont(QFont("WenQuanYi Zen Hei", 24, QFont::Bold));
    }

    // 在温度计图标右侧绘制文本
    QRectF textRect(thermometerWidth + 12, 0, textWidth, height());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, displayText);
}

QColor TemperatureIcon::getTemperatureColor(float temperature) const
{
    // 根据温度设置颜色
    if (temperature < 10.0f) {
        return QColor("#2196F3"); // 蓝色 - 很冷
    } else if (temperature < 20.0f) {
        return QColor("#00BCD4"); // 青色 - 冷
    } else if (temperature < 30.0f) {
        return QColor("#4CAF50"); // 绿色 - 正常
    } else if (temperature < 40.0f) {
        return QColor("#FF9800"); // 橙色 - 温热
    } else if (temperature < 50.0f) {
        return QColor("#FF5722"); // 深橙色 - 热
    } else {
        return QColor("#F44336"); // 红色 - 很热
    }
}

} // namespace SmartScope 