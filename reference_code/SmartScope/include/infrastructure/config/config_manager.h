#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QMutex>
#include <QSettings>
#include <memory>

namespace SmartScope {
namespace Infrastructure {

/**
 * @brief 配置管理器类，用于管理应用程序配置
 * 
 * 配置管理器使用单例模式，提供全局访问点
 * 支持从配置文件加载配置，保存配置到文件
 * 支持配置变更通知
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 配置管理器单例引用
     */
    static ConfigManager& instance();

    /**
     * @brief 析构函数
     */
    virtual ~ConfigManager();

    /**
     * @brief 初始化配置管理器
     * @param configFilePath 配置文件路径，如果为空则使用默认路径
     * @return 是否成功初始化
     */
    bool init(const QString& configFilePath = QString());

    /**
     * @brief 获取配置值
     * @param key 配置键
     * @param defaultValue 默认值，如果配置不存在则返回此值
     * @return 配置值
     */
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief 设置配置值
     * @param key 配置键
     * @param value 配置值
     * @param autoSave 是否自动保存到文件
     * @return 是否成功设置
     */
    bool setValue(const QString& key, const QVariant& value, bool autoSave = true);

    /**
     * @brief 保存配置到文件
     * @return 是否成功保存
     */
    bool saveConfig();

    /**
     * @brief 加载配置文件
     * @return 是否加载成功
     */
    bool loadConfig();

    /**
     * @brief 加载TOML格式的配置文件
     * @param configFilePath 配置文件路径
     * @return 是否加载成功
     */
    bool loadTomlConfig(const QString& configFilePath);

signals:
    /**
     * @brief 配置变更信号
     * @param key 变更的配置键
     * @param value 新的配置值
     */
    void configChanged(const QString& key, const QVariant& value);

    /**
     * @brief 配置文件加载完成信号
     * @param success 是否成功加载
     */
    void configLoaded(bool success);

    /**
     * @brief 配置文件保存完成信号
     * @param success 是否成功保存
     */
    void configSaved(bool success);

private:
    /**
     * @brief 构造函数（私有，防止外部创建实例）
     */
    ConfigManager();

    /**
     * @brief 加载默认配置
     */
    void loadDefaultConfig();

    // 禁止拷贝构造和赋值操作
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 单例实例
    static std::unique_ptr<ConfigManager> s_instance;

    // 配置数据
    QSettings* m_settings;

    // 配置文件路径
    QString m_configFilePath;

    // 互斥锁，用于线程安全
    mutable QMutex m_mutex;

    // 是否已初始化
    bool m_isInitialized;
};

// 配置管理器删除器，用于智能指针
struct ConfigManagerDeleter {
    void operator()(ConfigManager* manager) {
        delete manager;
    }
};

// 便捷宏定义，用于获取配置值
#define CONFIG_VALUE(key, defaultValue) SmartScope::Infrastructure::ConfigManager::instance().getValue(key, defaultValue)

// 便捷宏定义，用于获取配置值（与CONFIG_VALUE相同，为了保持一致性）
#define CONFIG_GET_VALUE(key, defaultValue) SmartScope::Infrastructure::ConfigManager::instance().getValue(key, defaultValue)

// 便捷宏定义，用于设置配置值
#define CONFIG_SET_VALUE(key, value) SmartScope::Infrastructure::ConfigManager::instance().setValue(key, value)

} // namespace Infrastructure
} // namespace SmartScope

#endif // CONFIG_MANAGER_H 