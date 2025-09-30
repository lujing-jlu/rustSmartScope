#ifndef TOAST_NOTIFICATION_H
#define TOAST_NOTIFICATION_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPaintEvent>
#include <QPropertyAnimation>

/**
 * @brief 轻量级Toast通知组件
 * 
 * 用于显示非阻塞的临时提示信息，一段时间后自动消失
 */
class ToastNotification : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float opacity READ opacity WRITE setOpacity)

public:
    enum Position {
        TopCenter,    ///< 顶部居中
        TopRight,     ///< 右上角
        TopLeft,      ///< 左上角
        BottomCenter, ///< 底部居中
        BottomRight,  ///< 右下角
        BottomLeft,   ///< 左下角
        Center        ///< 正中心
    };

    enum Type {
        Info,      ///< 信息提示
        Success,   ///< 成功提示
        Warning,   ///< 警告提示
        Error      ///< 错误提示
    };

    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit ToastNotification(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ToastNotification();

    /**
     * @brief 显示提示信息
     * @param message 提示信息内容
     * @param duration 显示时长(毫秒)
     * @param position 显示位置
     * @param type 提示类型
     */
    void showMessage(const QString &message, int duration = 2000, 
                     Position position = BottomCenter, Type type = Info);

    /**
     * @brief 设置不透明度
     * @param opacity 不透明度值(0.0-1.0)
     */
    void setOpacity(float opacity);
    
    /**
     * @brief 获取不透明度
     * @return 不透明度值
     */
    float opacity() const;

protected:
    /**
     * @brief 绘制事件
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent *event) override;
    
    /**
     * @brief 调整大小事件
     * @param event 调整大小事件
     */
    void resizeEvent(QResizeEvent *event) override;

private:
    /**
     * @brief 计算位置
     * @param position 位置类型
     */
    void calculatePosition(Position position);
    
    /**
     * @brief 初始化UI
     */
    void initUI();
    
    /**
     * @brief 应用样式
     * @param type 提示类型
     */
    void applyStyle(Type type);

private:
    QLabel *m_messageLabel;       ///< 消息标签
    QTimer *m_timer;              ///< 定时器
    float m_opacity;              ///< 不透明度
    QPropertyAnimation *m_animation; ///< 动画
};

/**
 * @brief 显示一条Toast通知
 * @param parent 父组件
 * @param message 消息内容
 * @param duration 显示时长(毫秒)
 * @param position 显示位置
 * @param type 提示类型
 */
void showToast(QWidget *parent, const QString &message, int duration = 2000, 
               ToastNotification::Position position = ToastNotification::BottomCenter, 
               ToastNotification::Type type = ToastNotification::Info);

#endif // TOAST_NOTIFICATION_H 