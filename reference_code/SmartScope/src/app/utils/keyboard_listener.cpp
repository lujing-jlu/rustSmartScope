#include "app/utils/keyboard_listener.h"
#include "infrastructure/logging/logger.h"

// 使用日志宏
#define LOG_INFO(message) Logger::instance().info(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARNING(message) Logger::instance().warning(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR(message) Logger::instance().error(message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG(message) Logger::instance().debug(message, __FILE__, __LINE__, __FUNCTION__)

// 单例实例化
KeyboardListener& KeyboardListener::instance()
{
    static KeyboardListener instance;
    return instance;
}

KeyboardListener::KeyboardListener()
{
    LOG_INFO("键盘监听器初始化完成");
}

KeyboardListener::~KeyboardListener()
{
    // 清空处理函数注册表
    m_keyHandlers.clear();
    LOG_INFO("键盘监听器已销毁");
}

bool KeyboardListener::registerKeyHandler(Qt::Key key, std::function<void()> callback, QObject* context)
{
    if (!context) {
        LOG_WARNING("注册按键处理函数失败：上下文为空");
        return false;
    }
    
    // 将按键处理函数添加到注册表
    m_keyHandlers[key][context] = callback;
    
    // 当上下文对象销毁时，自动移除相关的处理函数
    connect(context, &QObject::destroyed, this, [this, key, context]() {
        this->unregisterKeyHandler(key, context);
    }, Qt::UniqueConnection);
    
    LOG_INFO(QString("注册按键处理函数：按键=%1，上下文=%2").arg(key).arg((quintptr)context));
    return true;
}

bool KeyboardListener::unregisterKeyHandler(Qt::Key key, QObject* context)
{
    if (!context) {
        LOG_WARNING("注销按键处理函数失败：上下文为空");
        return false;
    }
    
    // 检查按键是否已注册
    if (!m_keyHandlers.contains(key)) {
        LOG_WARNING(QString("注销按键处理函数失败：按键未注册 (key=%1)").arg(key));
        return false;
    }
    
    // 检查上下文是否已注册
    if (!m_keyHandlers[key].contains(context)) {
        LOG_WARNING(QString("注销按键处理函数失败：上下文未注册 (key=%1, context=%2)").arg(key).arg((quintptr)context));
        return false;
    }
    
    // 移除处理函数
    m_keyHandlers[key].remove(context);
    
    // 如果没有更多的处理函数，则移除按键
    if (m_keyHandlers[key].isEmpty()) {
        m_keyHandlers.remove(key);
    }
    
    LOG_INFO(QString("注销按键处理函数：按键=%1，上下文=%2").arg(key).arg((quintptr)context));
    return true;
}

bool KeyboardListener::handleKeyEvent(QKeyEvent* event)
{
    if (!event) {
        return false;
    }
    
    // 打印所有键盘事件信息
    QString eventTypeStr;
    if (event->type() == QEvent::KeyPress) {
        eventTypeStr = "KeyPress";
    } else if (event->type() == QEvent::KeyRelease) {
        eventTypeStr = "KeyRelease";
    } else {
        eventTypeStr = "Other(" + QString::number(event->type()) + ")";
    }
    
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    QString text = event->text();
    
    LOG_INFO(QString("键盘事件: type=%1, key=%2 (0x%3), modifiers=0x%4, text='%5'")
             .arg(eventTypeStr)
             .arg(key)
             .arg(key, 0, 16)
             .arg(modifiers, 0, 16)
             .arg(text));
    
    // 只处理按键按下事件
    if (event->type() != QEvent::KeyPress) {
        return false;
    }
    
    // 检查按键是否已注册
    if (!m_keyHandlers.contains(key)) {
        LOG_INFO(QString("未注册的按键: key=%1").arg(key));
        return false;
    }
    
    // 调用所有注册的处理函数
    bool handled = false;
    for (auto it = m_keyHandlers[key].begin(); it != m_keyHandlers[key].end(); ++it) {
        if (it.value()) {
            LOG_INFO(QString("正在执行按键处理函数: key=%1, context=%2").arg(key).arg((quintptr)it.key()));
            it.value()();
            handled = true;
            LOG_INFO(QString("处理按键事件：按键=%1，上下文=%2").arg(key).arg((quintptr)it.key()));
        }
    }
    
    return handled;
}

void KeyboardListener::installEventFilter(QObject* target)
{
    if (!target) {
        LOG_WARNING("安装事件过滤器失败：目标对象为空");
        return;
    }
    
    // 修改为正确的方式：将键盘监听器作为目标对象的事件过滤器
    target->installEventFilter(this);
    LOG_INFO(QString("已安装事件过滤器到对象：%1 (类型: %2)").arg((quintptr)target).arg(target->metaObject()->className()));
}

bool KeyboardListener::eventFilter(QObject* watched, QEvent* event)
{
    // 记录所有事件类型
    static QMap<int, int> eventCount;
    int eventType = event->type();
    eventCount[eventType]++;
    
    // 每100个事件打印一次统计信息
    static int totalCount = 0;
    totalCount++;
    if (totalCount % 1000 == 0) {
        QString stats;
        for (auto it = eventCount.begin(); it != eventCount.end(); ++it) {
            stats += QString("%1:%2 ").arg(it.key()).arg(it.value());
        }
        LOG_DEBUG(QString("事件统计: %1").arg(stats));
    }
    
    // 处理键盘事件
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        LOG_INFO(QString("事件过滤器捕获到按键事件，对象: %1 (%2), 按键: %3").arg((quintptr)watched)
                .arg(watched->metaObject()->className())
                .arg(keyEvent->key()));
        
        if (event->type() == QEvent::KeyPress) {
            bool handled = handleKeyEvent(keyEvent);
            if (handled) {
                LOG_INFO("按键事件已处理，阻止传递");
                return true; // 事件已处理，阻止传递
            }
        }
    }
    
    // 其他事件交给基类处理
    return QObject::eventFilter(watched, event);
} 