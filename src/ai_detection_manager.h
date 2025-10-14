#ifndef AI_DETECTION_MANAGER_H
#define AI_DETECTION_MANAGER_H

#include <QObject>
#include <QPixmap>
#include <QTimer>
#include <QMutex>
#include <QVariant>

// FFI header
extern "C" {
    #include "smartscope.h"
}

class AiDetectionManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit AiDetectionManager(QObject* parent = nullptr);
    ~AiDetectionManager();

    // 初始化推理服务
    bool initialize(const QString& modelPath, int numWorkers = 6);
    void shutdown();

    bool enabled() const { return m_enabled; }
    Q_INVOKABLE void setEnabled(bool en);

public slots:
    // 连接到 CameraManager 的 pixmap 信号
    void onLeftPixmap(const QPixmap& pixmap);
    void onSinglePixmap(const QPixmap& pixmap);

signals:
    void enabledChanged();
    // 推理结果（QVariantList: list of { left, top, right, bottom, confidence, class_id })
    void detectionsUpdated(const QVariantList& detections);

private slots:
    void pollResults();

private:
    void submitPixmap(const QPixmap& pixmap);
    QString className(int classId) const;
    bool loadLabels(const QString& path);

    bool m_enabled { false };
    QTimer m_pollTimer;
    QMutex m_submitMutex;
    QVector<QString> m_labels;
};

#endif // AI_DETECTION_MANAGER_H
