#include "storage_manager.h"

extern "C" {
    #include "smartscope.h"
}

// Rust -> C 回调桥（全局C函数，便于以C ABI导出）
extern "C" void storage_list_changed_trampoline(void* ctx, const char* json) {
    StorageManager* self = reinterpret_cast<StorageManager*>(ctx);
    if (!self || !json) return;
    QString s = QString::fromUtf8(json);
    QMetaObject::invokeMethod(self, "emitStorageListChanged", Qt::QueuedConnection,
                              Q_ARG(QString, s));
}

extern "C" void storage_config_changed_trampoline(void* ctx, const char* json) {
    StorageManager* self = reinterpret_cast<StorageManager*>(ctx);
    if (!self || !json) return;
    QString s = QString::fromUtf8(json);
    QMetaObject::invokeMethod(self, "emitStorageConfigChanged", Qt::QueuedConnection,
                              Q_ARG(QString, s));
}

StorageManager::StorageManager(QObject* parent)
    : QObject(parent) {
    // 切换为Rust侧推送回调，取消本地轮询
    m_pollTimer.setInterval(1000);
    m_pollTimer.stop();

    smartscope_storage_register_callbacks(this,
        storage_list_changed_trampoline,
        storage_config_changed_trampoline);
}

StorageManager::~StorageManager() {
    smartscope_storage_unregister_callbacks(this);
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

QString StorageManager::resolveScreenshotSessionPath(const QString& displayMode) {
    QByteArray dm = displayMode.toUtf8();
    char* ptr = smartscope_storage_resolve_screenshot_session_path(dm.constData());
    if (!ptr) return QString();
    QString s = QString::fromUtf8(ptr);
    smartscope_free_string(ptr);
    return s;
}

QString StorageManager::resolveCaptureSessionPath(const QString& displayMode) {
    QByteArray dm = displayMode.toUtf8();
    char* ptr = smartscope_storage_resolve_capture_session_path(dm.constData());
    if (!ptr) return QString();
    QString s = QString::fromUtf8(ptr);
    smartscope_free_string(ptr);
    return s;
}

QString StorageManager::resolveVideoSessionPath(const QString& displayMode) {
    QByteArray dm = displayMode.toUtf8();
    char* ptr = smartscope_storage_resolve_video_session_path(dm.constData());
    if (!ptr) return QString();
    QString s = QString::fromUtf8(ptr);
    smartscope_free_string(ptr);
    return s;
}

void StorageManager::emitStorageListChanged(const QString& json) {
    m_lastListJson = json;
    emit storageListChanged(json);
}

void StorageManager::emitStorageConfigChanged(const QString& json) {
    m_lastConfigJson = json;
    emit storageConfigChanged(json);
}
