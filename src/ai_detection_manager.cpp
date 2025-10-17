#include "ai_detection_manager.h"
#include "logger.h"
#include <QImage>
#include <QFile>
#include <QVariantMap>
#include <QVector>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

// Query current invert state from Rust core
extern "C" {
    bool smartscope_video_get_invert();
}

// AI result callback registration (Rust push)
extern "C" {
    typedef void (*smartscope_ai_result_cb)(void* ctx, const char* json);
    void smartscope_ai_register_result_callback(void* ctx, smartscope_ai_result_cb cb, int max_fps);
    void smartscope_ai_unregister_result_callback(void* ctx);
}

extern "C" void ai_result_trampoline(void* ctx, const char* json) {
    AiDetectionManager* self = reinterpret_cast<AiDetectionManager*>(ctx);
    if (!self) return;
    QString s = json ? QString::fromUtf8(json) : QString();
    QMetaObject::invokeMethod(self, "onAiResultJson", Qt::QueuedConnection,
                              Q_ARG(QString, s));
}

AiDetectionManager::AiDetectionManager(QObject* parent)
    : QObject(parent)
{
    // 轮询接口保留兼容（默认不启用）；改用Rust主动推送结果
    m_pollTimer.setInterval(30);

    // 健康监控：超过2s未收到AI结果则判定为不活跃
    m_aliveTimer.setInterval(1000);
    connect(&m_aliveTimer, &QTimer::timeout, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool alive = (m_lastDetectionsMs > 0) && ((now - m_lastDetectionsMs) <= 2000);
        if (alive != m_aiPushAlive) {
            m_aiPushAlive = alive;
            emit statsChanged();
            if (!alive && m_enabled) {
                LOG_WARN("AiDetectionManager", "No AI results for >2s while enabled");
            }
        }
    });
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
    // 试图加载英文与中文标签（中文存在则优先使用中文）
    loadLabels("models/coco_labels.txt");
    QFile zhFileCheck("models/coco_labels_zh.txt");
    if (zhFileCheck.exists()) {
        // 加载中文翻译表（与英文列表一一对应）
        QFile f("models/coco_labels_zh.txt");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QVector<QString> labels;
            while (!f.atEnd()) {
                QByteArray line = f.readLine();
                QString s = QString::fromUtf8(line).trimmed();
                if (!s.isEmpty()) labels.push_back(s);
            }
            if (!labels.isEmpty()) {
                m_labelsZh = std::move(labels);
                LOG_INFO("AiDetectionManager", "Loaded CN labels: ", m_labelsZh.size());
            }
        }
    }
    return true;
}

void AiDetectionManager::shutdown() {
    smartscope_ai_shutdown();
    m_pollTimer.stop();
    smartscope_ai_unregister_result_callback(this);
}

void AiDetectionManager::setEnabled(bool en) {
    if (m_enabled == en) return;
    m_enabled = en;
    smartscope_ai_set_enabled(en);
    if (en) {
        // 由Rust侧主动推送；max_fps=0 表示尽可能快（由Rust侧最小间隔1ms防止忙等）
        smartscope_ai_register_result_callback(this, ai_result_trampoline, 0);
        m_aliveTimer.start();
    } else {
        smartscope_ai_unregister_result_callback(this);
        m_pollTimer.stop();
        m_aliveTimer.stop();
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

void AiDetectionManager::onAiResultJson(const QString& json) {
    if (!m_enabled) return;
    if (json.isEmpty()) {
        // 不绘制：清空覆盖层
        emit detectionsUpdated(QVariantList{});
        m_lastDetectionsCount = 0;
        m_lastDetectionsMs = QDateTime::currentMSecsSinceEpoch();
        emit statsChanged();
        return;
    }
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return;
    }
    QVariantList list;
    auto arr = doc.array();
    list.reserve(arr.size());
    for (const auto& v : arr) {
        QJsonObject o = v.toObject();
        QVariantMap m;
        m.insert("left", o.value("left").toInt());
        m.insert("top", o.value("top").toInt());
        m.insert("right", o.value("right").toInt());
        m.insert("bottom", o.value("bottom").toInt());
        m.insert("confidence", o.value("confidence").toDouble());
        int cid = o.value("class_id").toInt();
        m.insert("class_id", cid);
        m.insert("label", className(cid));
        list.push_back(m);
    }
    emit detectionsUpdated(list);
    m_lastDetectionsCount = list.size();
    m_lastDetectionsMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_aiPushAlive) { m_aiPushAlive = true; emit statsChanged(); }
    else { emit statsChanged(); }
}

QString AiDetectionManager::className(int classId) const {
    // 优先返回中文翻译
    if (classId >= 0 && classId < m_labelsZh.size()) return m_labelsZh[classId];
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
