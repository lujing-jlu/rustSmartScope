#include "storage_manager.h"

extern "C" {
    #include "smartscope.h"
}

StorageManager::StorageManager(QObject* parent)
    : QObject(parent) {
    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, [this]() {
        // poll storage list
        QString listJson = refreshExternalStoragesJson();
        if (listJson != m_lastListJson) {
            m_lastListJson = listJson;
            emit storageListChanged(listJson);
        }
        // poll config json
        QString cfgJson = getStorageConfigJson();
        if (cfgJson != m_lastConfigJson) {
            m_lastConfigJson = cfgJson;
            emit storageConfigChanged(cfgJson);
        }
    });
    m_pollTimer.start();
}

QString StorageManager::refreshExternalStoragesJson() {
    char* ptr = smartscope_list_external_storages_json();
    if (!ptr) return QStringLiteral("[]");
    QString json = QString::fromUtf8(ptr);
    smartscope_free_string(ptr);
    return json;
}

QString StorageManager::getStorageConfigJson() {
    char* ptr = smartscope_storage_get_config_json();
    if (!ptr) return QString();
    QString json = QString::fromUtf8(ptr);
    smartscope_free_string(ptr);
    return json;
}

bool StorageManager::setStorageLocation(int location) {
    int rc = smartscope_storage_set_location((unsigned int)location);
    return rc == 0;
}

bool StorageManager::setStorageExternalDevice(const QString& devicePath) {
    QByteArray b = devicePath.toUtf8();
    int rc = smartscope_storage_set_external_device(b.constData());
    return rc == 0;
}

bool StorageManager::setStorageInternalBasePath(const QString& path) {
    QByteArray b = path.toUtf8();
    int rc = smartscope_storage_set_internal_base_path(b.constData());
    return rc == 0;
}

bool StorageManager::setStorageExternalRelativePath(const QString& path) {
    QByteArray b = path.toUtf8();
    int rc = smartscope_storage_set_external_relative_path(b.constData());
    return rc == 0;
}

bool StorageManager::setStorageAutoRecover(bool enabled) {
    int rc = smartscope_storage_set_auto_recover(enabled);
    return rc == 0;
}

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static QString joinPath(const QString& a, const QString& b) {
    if (a.isEmpty()) return b;
    if (b.isEmpty()) return a;
    if (a.endsWith('/')) return a + b;
    return a + "/" + b;
}

QString StorageManager::resolveScreenshotSessionPath(const QString& displayMode) {
    // 读取存储配置
    QString cfgJson = getStorageConfigJson();
    if (cfgJson.isEmpty()) return QString();
    QJsonDocument doc = QJsonDocument::fromJson(cfgJson.toUtf8());
    if (!doc.isObject()) return QString();
    QJsonObject storage = doc.object();
    QString location = storage.value("location").toString();

    QString basePath;
    if (location == "external") {
        // 找到当前外置设备的挂载点
        QString dev = storage.value("external_device").toString();
        QString rel = storage.value("external_relative_path").toString();
        QString listJson = refreshExternalStoragesJson();
        QJsonDocument ldoc = QJsonDocument::fromJson(listJson.toUtf8());
        if (ldoc.isArray()) {
            for (const auto& v : ldoc.array()) {
                QJsonObject o = v.toObject();
                if (o.value("device").toString() == dev) {
                    QString mp = o.value("mount_point").toString();
                    basePath = joinPath(mp, rel);
                    break;
                }
            }
        }
        if (basePath.isEmpty()) {
            // 外置找不到则回退到机内
            basePath = storage.value("internal_base_path").toString();
        }
    } else {
        basePath = storage.value("internal_base_path").toString();
    }

    if (basePath.isEmpty()) return QString();

    // 构建会话目录
    const QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    const QString timeStr = QTime::currentTime().toString("HH-mm-ss");
    const QString sessionName = QString("%1_%2_%3").arg(dateStr, timeStr, displayMode);

    QString screenshotsDir = joinPath(basePath, "Screenshots");
    QString dateDir = joinPath(screenshotsDir, dateStr);
    QString sessionDir = joinPath(dateDir, sessionName);

    QDir dir;
    dir.mkpath(sessionDir);
    return sessionDir;
}

QString StorageManager::resolveCaptureSessionPath(const QString& displayMode) {
    // 读取存储配置
    QString cfgJson = getStorageConfigJson();
    if (cfgJson.isEmpty()) return QString();
    QJsonDocument doc = QJsonDocument::fromJson(cfgJson.toUtf8());
    if (!doc.isObject()) return QString();
    QJsonObject storage = doc.object();
    QString location = storage.value("location").toString();

    QString basePath;
    if (location == "external") {
        QString dev = storage.value("external_device").toString();
        QString rel = storage.value("external_relative_path").toString();
        QString listJson = refreshExternalStoragesJson();
        QJsonDocument ldoc = QJsonDocument::fromJson(listJson.toUtf8());
        if (ldoc.isArray()) {
            for (const auto& v : ldoc.array()) {
                QJsonObject o = v.toObject();
                if (o.value("device").toString() == dev) {
                    QString mp = o.value("mount_point").toString();
                    basePath = joinPath(mp, rel);
                    break;
                }
            }
        }
        if (basePath.isEmpty()) {
            basePath = storage.value("internal_base_path").toString();
        }
    } else {
        basePath = storage.value("internal_base_path").toString();
    }

    if (basePath.isEmpty()) return QString();

    const QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    const QString timeStr = QTime::currentTime().toString("HH-mm-ss");
    const QString sessionName = QString("%1_%2_%3").arg(dateStr, timeStr, displayMode);

    QString picturesDir = joinPath(basePath, "Pictures");
    QString dateDir = joinPath(picturesDir, dateStr);
    QString sessionDir = joinPath(dateDir, sessionName);

    QDir dir;
    dir.mkpath(sessionDir);
    return sessionDir;
}

QString StorageManager::resolveVideoSessionPath(const QString& displayMode) {
    // 读取存储配置
    QString cfgJson = getStorageConfigJson();
    if (cfgJson.isEmpty()) return QString();
    QJsonDocument doc = QJsonDocument::fromJson(cfgJson.toUtf8());
    if (!doc.isObject()) return QString();
    QJsonObject storage = doc.object();
    QString location = storage.value("location").toString();

    QString basePath;
    if (location == "external") {
        QString dev = storage.value("external_device").toString();
        QString rel = storage.value("external_relative_path").toString();
        QString listJson = refreshExternalStoragesJson();
        QJsonDocument ldoc = QJsonDocument::fromJson(listJson.toUtf8());
        if (ldoc.isArray()) {
            for (const auto& v : ldoc.array()) {
                QJsonObject o = v.toObject();
                if (o.value("device").toString() == dev) {
                    QString mp = o.value("mount_point").toString();
                    basePath = joinPath(mp, rel);
                    break;
                }
            }
        }
        if (basePath.isEmpty()) {
            basePath = storage.value("internal_base_path").toString();
        }
    } else {
        basePath = storage.value("internal_base_path").toString();
    }

    if (basePath.isEmpty()) return QString();

    const QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    const QString timeStr = QTime::currentTime().toString("HH-mm-ss");
    const QString sessionName = QString("%1_%2_%3").arg(dateStr, timeStr, displayMode);

    QString videosDir = joinPath(basePath, "Videos");
    QString dateDir = joinPath(videosDir, dateStr);
    QString sessionDir = joinPath(dateDir, sessionName);

    QDir dir;
    dir.mkpath(sessionDir);
    return sessionDir;
}
