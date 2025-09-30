#ifndef KEYBOARD_LISTENER_H
#define KEYBOARD_LISTENER_H

#include <QObject>
#include <QMap>
#include <QEvent>
#include <QKeyEvent>
#include <functional>

/**
 * @brief 键盘监听器类
 * 
 * 该类负责监听键盘事件并分发到注册的处理函数
 */
class KeyboardListener : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return KeyboardListener单例实例
     */
    static KeyboardListener& instance();

    /**
     * @brief 注册按键处理函数
     * @param key 要监听的按键
     * @param callback 按键触发时的回调函数
     * @param context 回调函数的上下文（通常是this指针）
     * @return 是否注册成功
     */
    bool registerKeyHandler(Qt::Key key, std::function<void()> callback, QObject* context);
    
    /**
     * @brief 注销按键处理函数
     * @param key 要注销的按键
     * @param context 回调函数的上下文
     * @return 是否注销成功
     */
    bool unregisterKeyHandler(Qt::Key key, QObject* context);
    
    /**
     * @brief 处理键盘事件
     * @param event 键盘事件
     * @return 事件是否被处理
     */
    bool handleKeyEvent(QKeyEvent* event);
    
    /**
     * @brief 安装全局事件过滤器
     * @param target 要安装到的对象
     */
    void installEventFilter(QObject* target);

protected:
    /**
     * @brief 事件过滤器
     * @param watched 监视的对象
     * @param event 事件
     * @return 是否处理事件
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /**
     * @brief 构造函数
     */
    KeyboardListener();
    
    /**
     * @brief 析构函数
     */
    ~KeyboardListener();

    // 禁止复制和赋值
    KeyboardListener(const KeyboardListener&) = delete;
    KeyboardListener& operator=(const KeyboardListener&) = delete;

private:
    // 按键处理函数注册表，键是按键值，值是回调函数和上下文的映射
    QMap<int, QMap<QObject*, std::function<void()>>> m_keyHandlers;
};

#endif // KEYBOARD_LISTENER_H 