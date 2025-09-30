#include "infrastructure/config/config_manager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMutexLocker>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace SmartScope {
namespace Infrastructure {

// 初始化静态单例指针
std::unique_ptr<ConfigManager> ConfigManager::s_instance = nullptr;

// 获取单例实例
ConfigManager& ConfigManager::instance()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<ConfigManager>(new ConfigManager());
    }
    return *s_instance;
}

// 构造函数
ConfigManager::ConfigManager() : QObject(nullptr),
    m_settings(nullptr),
    m_isInitialized(false)
{
    qDebug() << "ConfigManager: 创建配置管理器实例";
}

// 析构函数
ConfigManager::~ConfigManager()
{
    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }
    qDebug() << "ConfigManager: 销毁配置管理器实例";
}

// 初始化配置管理器
bool ConfigManager::init(const QString& configFilePath)
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "ConfigManager: 初始化配置管理器";
    
    // 如果已经初始化，先清理之前的资源
    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }
    
    // 使用内存中的配置
    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, 
                              QCoreApplication::organizationName(), 
                              QCoreApplication::applicationName());
    
    // 检查配置是否可用
    if (m_settings->status() != QSettings::NoError) {
        qWarning() << "ConfigManager: 配置不可用";
        delete m_settings;
        m_settings = nullptr;
        return false;
    }
    
    // 保存配置文件路径
    m_configFilePath = configFilePath;
    
    // 加载默认配置
    loadDefaultConfig();
    
    m_isInitialized = true;
    qDebug() << "ConfigManager: 初始化完成";
    
    return true;
}

// 加载TOML配置文件
bool ConfigManager::loadTomlConfig(const QString& configFilePath)
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "ConfigManager: 加载TOML配置文件:" << configFilePath;
    
    if (!m_settings) {
        qWarning() << "ConfigManager: 配置管理器未初始化";
        return false;
    }
    
    // 检查文件是否存在
    QFile file(configFilePath);
    if (!file.exists()) {
        qWarning() << "ConfigManager: 配置文件不存在:" << configFilePath;
        return false;
    }
    
    // 打开文件
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ConfigManager: 无法打开配置文件:" << file.errorString();
        return false;
    }
    
    // 读取文件内容
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    // 解析TOML文件内容
    QStringList lines = content.split('\n');
    QString currentSection;
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        
        // 跳过空行和注释
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
            continue;
        }
        
        // 处理节名
        if (trimmedLine.startsWith('[') && trimmedLine.endsWith(']')) {
            currentSection = trimmedLine.mid(1, trimmedLine.length() - 2);
            continue;
        }
        
        // 处理键值对
        int equalPos = trimmedLine.indexOf('=');
        if (equalPos > 0) {
            QString key = trimmedLine.left(equalPos).trimmed();
            QString value = trimmedLine.mid(equalPos + 1).trimmed();
            
            // 构建完整的键名
            QString fullKey = currentSection.isEmpty() ? key : currentSection + "/" + key;
            
            // 替换点号为斜杠，使键名符合QSettings的格式
            fullKey = fullKey.replace('.', '/');
            
            // 解析值
            if (value.startsWith('[') && value.endsWith(']')) {
                // 数组类型
                value = value.mid(1, value.length() - 2);
                QStringList items = value.split(',');
                QVariantList variantList;
                
                for (QString& item : items) {
                    item = item.trimmed();
                    if (item.startsWith('"') && item.endsWith('"')) {
                        // 字符串类型
                        item = item.mid(1, item.length() - 2);
                        variantList.append(item);
                    } else if (item.toLower() == "true" || item.toLower() == "false") {
                        // 布尔类型
                        variantList.append(item.toLower() == "true");
                    } else {
                        // 尝试转换为数字
                        bool ok;
                        double num = item.toDouble(&ok);
                        if (ok) {
                            variantList.append(num);
                        } else {
                            // 如果无法转换，则作为字符串处理
                            variantList.append(item);
                        }
                    }
                }
                
                // 设置数组值
                m_settings->setValue(fullKey, variantList);
            } else if (value.startsWith('"') && value.endsWith('"')) {
                // 字符串类型
                value = value.mid(1, value.length() - 2);
                m_settings->setValue(fullKey, value);
            } else if (value.toLower() == "true" || value.toLower() == "false") {
                // 布尔类型
                m_settings->setValue(fullKey, value.toLower() == "true");
            } else {
                // 尝试转换为数字
                bool ok;
                double num = value.toDouble(&ok);
                if (ok) {
                    m_settings->setValue(fullKey, num);
                } else {
                    // 如果无法转换，则作为字符串处理
                    m_settings->setValue(fullKey, value);
                }
            }
        }
    }
    
    // 保存配置文件路径
    m_configFilePath = configFilePath;
    
    // 发送配置加载信号
    emit configLoaded(true);
    
    qDebug() << "ConfigManager: TOML配置文件加载成功";
    return true;
}

// 获取配置值
QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_settings) {
        qWarning() << "ConfigManager: 配置管理器未初始化";
        return defaultValue;
    }
    
    return m_settings->value(key, defaultValue);
}

// 设置配置值
bool ConfigManager::setValue(const QString& key, const QVariant& value, bool autoSave)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_settings) {
        qWarning() << "ConfigManager: 配置管理器未初始化";
        return false;
    }
    
    // 检查值是否变化
    QVariant oldValue = m_settings->value(key);
    if (oldValue == value) {
        return true; // 值未变化，无需更新
    }
    
    // 设置新值
    m_settings->setValue(key, value);
    
    // 发送配置变更信号
    emit configChanged(key, value);
    
    return true;
}

// 保存配置到文件
bool ConfigManager::saveConfig()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_settings) {
        qWarning() << "ConfigManager: 配置管理器未初始化";
        return false;
    }
    
    // 同步配置
    m_settings->sync();
    
    bool success = (m_settings->status() == QSettings::NoError);
    
    // 发送配置保存信号
    emit configSaved(success);
    
    return success;
}

// 加载配置文件
bool ConfigManager::loadConfig()
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "ConfigManager: 加载配置文件";
    
    if (!m_settings) {
        qWarning() << "ConfigManager: 配置管理器未初始化";
        return false;
    }
    
    // 如果配置文件路径不为空，尝试加载TOML配置文件
    if (!m_configFilePath.isEmpty()) {
        return loadTomlConfig(m_configFilePath);
    }
    
    // 同步配置
    m_settings->sync();
    
    // 发送配置加载信号
    emit configLoaded(true);
    
    return true;
}

// 加载默认配置
void ConfigManager::loadDefaultConfig()
{
    if (!m_settings) {
        return;
    }
    
    // 应用程序信息
    if (!m_settings->contains("app/version")) {
        m_settings->setValue("app/version", QCoreApplication::applicationVersion());
    }
    if (!m_settings->contains("app/name")) {
        m_settings->setValue("app/name", QCoreApplication::applicationName());
    }
    
    // 日志配置
    if (!m_settings->contains("log/level")) {
        m_settings->setValue("log/level", "INFO");
    }
    if (!m_settings->contains("log/console_enabled")) {
        m_settings->setValue("log/console_enabled", true);
    }
    if (!m_settings->contains("log/file_enabled")) {
        m_settings->setValue("log/file_enabled", false);
    }
    
    // UI配置
    if (!m_settings->contains("ui/theme")) {
        m_settings->setValue("ui/theme", "dark");
    }
    if (!m_settings->contains("ui/language")) {
        m_settings->setValue("ui/language", "zh_CN");
    }
    if (!m_settings->contains("ui/show_fps")) {
        m_settings->setValue("ui/show_fps", false);  // 默认不显示帧率
    }
    
    // 相机配置
    if (!m_settings->contains("camera/left_id")) {
        m_settings->setValue("camera/left_id", "");
    }
    if (!m_settings->contains("camera/right_id")) {
        m_settings->setValue("camera/right_id", "");
    }
    if (!m_settings->contains("camera/resolution_width")) {
        m_settings->setValue("camera/resolution_width", 1280);
    }
    if (!m_settings->contains("camera/resolution_height")) {
        m_settings->setValue("camera/resolution_height", 720);
    }
    if (!m_settings->contains("camera/frame_rate")) {
        m_settings->setValue("camera/frame_rate", 30);
    }
    
    // 文件管理配置
    if (!m_settings->contains("file/save_path")) {
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/SmartScope";
        m_settings->setValue("file/save_path", defaultPath);
    }
    if (!m_settings->contains("file/auto_save")) {
        m_settings->setValue("file/auto_save", false);
    }
    if (!m_settings->contains("file/auto_save_interval")) {
        m_settings->setValue("file/auto_save_interval", 300); // 5分钟
    }
}

} // namespace Infrastructure
} // namespace SmartScope 