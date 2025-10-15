#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>

class StorageManager : public QObject {
    Q_OBJECT
public:
    explicit StorageManager(QObject* parent = nullptr);

    // 返回JSON数组字符串；调用方无需释放
    Q_INVOKABLE QString refreshExternalStoragesJson();

    // 获取/设置存储配置
    Q_INVOKABLE QString getStorageConfigJson();
    Q_INVOKABLE bool setStorageLocation(int location); // 0 internal, 1 external
    Q_INVOKABLE bool setStorageExternalDevice(const QString& devicePath);
    Q_INVOKABLE bool setStorageInternalBasePath(const QString& path);
    Q_INVOKABLE bool setStorageExternalRelativePath(const QString& path);
    Q_INVOKABLE bool setStorageAutoRecover(bool enabled);

    // 解析当前存储配置并创建本次截图会话目录
    // 返回目录: <base>/Screenshots/YYYY-MM-DD/YYYY-MM-DD_HH-mm-ss_<displayMode>
    Q_INVOKABLE QString resolveScreenshotSessionPath(const QString& displayMode);

    // 解析当前存储配置并创建本次拍照会话目录
    // 返回目录: <base>/Pictures/YYYY-MM-DD/YYYY-MM-DD_HH-mm-ss_<displayMode>
    Q_INVOKABLE QString resolveCaptureSessionPath(const QString& displayMode);

signals:
    void storageListChanged(const QString& json);
    void storageConfigChanged(const QString& json);

private:
    QTimer m_pollTimer;
    QString m_lastListJson;
    QString m_lastConfigJson;
};

#endif // STORAGE_MANAGER_H
