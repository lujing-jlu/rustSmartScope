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
    enum CCameraMode {
        NoCamera = 0,
        SingleCamera = 1,
        StereoCamera = 2
    };

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
        uint32_t mode;  // CCameraMode
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
    int smartscope_get_single_frame(CCameraFrame* frame_out);
    int smartscope_get_camera_status(CCameraStatus* status_out);
    uint32_t smartscope_get_camera_mode();
    bool smartscope_is_camera_running();

}

class CameraManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QImage leftFrame READ leftFrame NOTIFY leftFrameChanged)
    Q_PROPERTY(QImage rightFrame READ rightFrame NOTIFY rightFrameChanged)
    Q_PROPERTY(QImage singleFrame READ singleFrame NOTIFY singleFrameChanged)
    Q_PROPERTY(bool cameraRunning READ cameraRunning NOTIFY cameraRunningChanged)
    Q_PROPERTY(bool leftConnected READ leftConnected NOTIFY leftConnectedChanged)
    Q_PROPERTY(bool rightConnected READ rightConnected NOTIFY rightConnectedChanged)
    Q_PROPERTY(int cameraMode READ cameraMode NOTIFY cameraModeChanged)

public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager();

    Q_INVOKABLE bool startCamera();
    Q_INVOKABLE bool stopCamera();

    QImage leftFrame() const;
    QImage rightFrame() const;
    QImage singleFrame() const;
    bool cameraRunning() const;
    bool leftConnected() const;
    bool rightConnected() const;
    int cameraMode() const;

    // 获取QPixmap用于原生Qt Widget显示
    QPixmap getLeftPixmap() const;
    QPixmap getRightPixmap() const;
    QPixmap getSinglePixmap() const;

signals:
    void leftFrameChanged();
    void rightFrameChanged();
    void singleFrameChanged();
    void cameraRunningChanged();
    void leftConnectedChanged();
    void rightConnectedChanged();
    void cameraModeChanged();

    // 新增：用于原生Qt Widget的QPixmap信号
    void leftPixmapUpdated(const QPixmap& pixmap);
    void rightPixmapUpdated(const QPixmap& pixmap);
    void singlePixmapUpdated(const QPixmap& pixmap);
    // Thumbnail signal removed; front-end reuses display frame

private slots:
    void updateFrames();

private:
    QImage processFrame(const CCameraFrame& frame);
    QImage applyVideoTransforms(const QImage& image);
    void updateStatus();

    QTimer* m_updateTimer;
    QImage m_leftFrame;      // QImage用于QML
    QImage m_rightFrame;
    QImage m_singleFrame;
    QPixmap m_leftPixmap;    // QPixmap用于原生Qt Widget
    QPixmap m_rightPixmap;
    QPixmap m_singlePixmap;
    bool m_cameraRunning;
    bool m_leftConnected;
    bool m_rightConnected;
    int m_cameraMode;
    QMutex m_frameMutex;
};

#endif // CAMERA_MANAGER_H
