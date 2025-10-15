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
