#include "storage_manager.h"

extern "C" {
    #include "smartscope.h"
}

StorageManager::StorageManager(QObject* parent)
    : QObject(parent) {
}

QString StorageManager::refreshExternalStoragesJson() {
    char* ptr = smartscope_list_external_storages_json();
    if (!ptr) return QStringLiteral("[]");
    QString json = QString::fromUtf8(ptr);
    smartscope_free_string(ptr);
    return json;
}

