#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QMutex>
#include <memory>

// C FFI declarations
extern "C" {
    struct CCameraFrame {
        const uint8_t* data;
        size_t data_len;
        uint32_t width;
        uint32_t height;
        uint32_t format;
        uint64_t timestamp_sec;
        uint32_t timestamp_nsec;
    };

    struct CCameraStatus {
        bool running;
        bool left_camera_connected;
        bool right_camera_connected;
        uint64_t last_left_frame_sec;
        uint64_t last_right_frame_sec;
    };

    int smartscope_start_camera();
    int smartscope_stop_camera();
    int smartscope_process_camera_frames();
    int smartscope_get_left_frame(CCameraFrame* frame_out);
    int smartscope_get_right_frame(CCameraFrame* frame_out);
    int smartscope_get_camera_status(CCameraStatus* status_out);
    bool smartscope_is_camera_running();
}

class CameraManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QImage leftFrame READ leftFrame NOTIFY leftFrameChanged)
    Q_PROPERTY(QImage rightFrame READ rightFrame NOTIFY rightFrameChanged)
    Q_PROPERTY(bool cameraRunning READ cameraRunning NOTIFY cameraRunningChanged)
    Q_PROPERTY(bool leftConnected READ leftConnected NOTIFY leftConnectedChanged)
    Q_PROPERTY(bool rightConnected READ rightConnected NOTIFY rightConnectedChanged)

public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager();

    Q_INVOKABLE bool startCamera();
    Q_INVOKABLE bool stopCamera();

    QImage leftFrame() const;
    QImage rightFrame() const;
    bool cameraRunning() const;
    bool leftConnected() const;
    bool rightConnected() const;

    // 获取QPixmap用于原生Qt Widget显示
    QPixmap getLeftPixmap() const;
    QPixmap getRightPixmap() const;

signals:
    void leftFrameChanged();
    void rightFrameChanged();
    void cameraRunningChanged();
    void leftConnectedChanged();
    void rightConnectedChanged();

    // 新增：用于原生Qt Widget的QPixmap信号
    void leftPixmapUpdated(const QPixmap& pixmap);
    void rightPixmapUpdated(const QPixmap& pixmap);

private slots:
    void updateFrames();

private:
    QImage processFrame(const CCameraFrame& frame);
    void updateStatus();

    QTimer* m_updateTimer;
    QImage m_leftFrame;      // QImage用于QML
    QImage m_rightFrame;
    QPixmap m_leftPixmap;    // QPixmap用于原生Qt Widget
    QPixmap m_rightPixmap;
    bool m_cameraRunning;
    bool m_leftConnected;
    bool m_rightConnected;
    QMutex m_frameMutex;
};

#endif // CAMERA_MANAGER_H