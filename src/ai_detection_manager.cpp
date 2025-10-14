#include "ai_detection_manager.h"
#include "logger.h"
#include <QImage>
#include <QFile>
#include <QVariantMap>
#include <QVector>

// Query current invert state from Rust core
extern "C" {
    bool smartscope_video_get_invert();
}

AiDetectionManager::AiDetectionManager(QObject* parent)
    : QObject(parent)
{
    // 轮询AI结果，默认关闭
    m_pollTimer.setInterval(30); // ~33 FPS
    connect(&m_pollTimer, &QTimer::timeout, this, &AiDetectionManager::pollResults);
}

AiDetectionManager::~AiDetectionManager() {
    shutdown();
}

bool AiDetectionManager::initialize(const QString& modelPath, int numWorkers) {
    QByteArray pathBytes = modelPath.toUtf8();
    int rc = smartscope_ai_init(pathBytes.constData(), numWorkers);
    if (rc != SMARTSCOPE_ERROR_SUCCESS) {
        LOG_ERROR("AiDetectionManager", "AI service init failed: ", smartscope_get_error_string(rc));
        return false;
    }
    LOG_INFO("AiDetectionManager", "AI service initialized at ", modelPath.toStdString(), " workers:", numWorkers);
    // 试图加载标签
    loadLabels("models/coco_labels.txt");
    return true;
}

void AiDetectionManager::shutdown() {
    smartscope_ai_shutdown();
    m_pollTimer.stop();
}

void AiDetectionManager::setEnabled(bool en) {
    if (m_enabled == en) return;
    m_enabled = en;
    smartscope_ai_set_enabled(en);
    if (en) {
        m_pollTimer.start();
    } else {
        m_pollTimer.stop();
        // 清空覆盖层
        emit detectionsUpdated(QVariantList{});
    }
    emit enabledChanged();
}

void AiDetectionManager::onLeftPixmap(const QPixmap& pixmap) {
    if (!m_enabled) return;
    submitPixmap(pixmap);
}

void AiDetectionManager::onSinglePixmap(const QPixmap& pixmap) {
    if (!m_enabled) return;
    submitPixmap(pixmap);
}

void AiDetectionManager::submitPixmap(const QPixmap& pixmap) {
    if (pixmap.isNull()) return;
    QMutexLocker locker(&m_submitMutex);
    QImage img = pixmap.toImage().convertToFormat(QImage::Format_RGB888);

    // Ensure AI sees pre-invert content: if display has invert ON, invert again to revert
    if (smartscope_video_get_invert()) {
        // In-place invert to restore pre-invert pixel values
        img.invertPixels();
    }

    const int width = img.width();
    const int height = img.height();
    const uchar* data = img.constBits();
    const size_t len = static_cast<size_t>(img.sizeInBytes());

    if (!data || len == 0) return;
    int rc = smartscope_ai_submit_rgb888(width, height, data, len);
    if (rc != SMARTSCOPE_ERROR_SUCCESS) {
        // 非关键错误，记录日志即可
        // qWarning() << "AI submit failed:" << rc;
    }
}

void AiDetectionManager::pollResults() {
    if (!m_enabled) return;

    smartscope_CDetection dets[128];
    int n = smartscope_ai_try_get_latest_result(dets, 128);
    if (n <= 0) {
        // 明确地推送空结果以清除上一次的绘制
        emit detectionsUpdated(QVariantList{});
        return;
    }

    QVariantList list;
    list.reserve(n);
    for (int i = 0; i < n; ++i) {
        const auto& d = dets[i];
        QVariantMap m;
        m.insert("left", d.left);
        m.insert("top", d.top);
        m.insert("right", d.right);
        m.insert("bottom", d.bottom);
        m.insert("confidence", d.confidence);
        m.insert("class_id", d.class_id);
        m.insert("label", className(d.class_id));
        list.push_back(m);
    }

    emit detectionsUpdated(list);
}

QString AiDetectionManager::className(int classId) const {
    if (classId >= 0 && classId < m_labels.size()) return m_labels[classId];
    return QString("class_%1").arg(classId);
}

bool AiDetectionManager::loadLabels(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_WARN("AiDetectionManager", "Failed to open labels file: ", path.toStdString());
        return false;
    }
    QVector<QString> labels;
    while (!f.atEnd()) {
        QByteArray line = f.readLine();
        QString s = QString::fromUtf8(line).trimmed();
        if (!s.isEmpty()) labels.push_back(s);
    }
    if (!labels.isEmpty()) {
        m_labels = std::move(labels);
        LOG_INFO("AiDetectionManager", "Loaded ", m_labels.size(), " labels from ", path.toStdString());
        return true;
    }
    return false;
}
