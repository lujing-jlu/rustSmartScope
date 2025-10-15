#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <QObject>
#include <QString>

class StorageManager : public QObject {
    Q_OBJECT
public:
    explicit StorageManager(QObject* parent = nullptr);

    // 返回JSON数组字符串；调用方无需释放
    Q_INVOKABLE QString refreshExternalStoragesJson();
};

#endif // STORAGE_MANAGER_H
