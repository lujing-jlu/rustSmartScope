#ifndef VIDEO_TRANSFORM_MANAGER_H
#define VIDEO_TRANSFORM_MANAGER_H

#include <QObject>
#include <QString>

// 视频变换管理器 - QML桥接类
class VideoTransformManager : public QObject
{
    Q_OBJECT

    // QML属性
    Q_PROPERTY(int rotationDegrees READ rotationDegrees NOTIFY rotationChanged)
    Q_PROPERTY(bool flipHorizontal READ flipHorizontal NOTIFY flipHorizontalChanged)
    Q_PROPERTY(bool flipVertical READ flipVertical NOTIFY flipVerticalChanged)
    Q_PROPERTY(bool invertColors READ invertColors NOTIFY invertColorsChanged)
    Q_PROPERTY(bool rgaAvailable READ rgaAvailable CONSTANT)

public:
    explicit VideoTransformManager(QObject *parent = nullptr);
    ~VideoTransformManager();

    // 属性getter
    int rotationDegrees() const;
    bool flipHorizontal() const;
    bool flipVertical() const;
    bool invertColors() const;
    bool rgaAvailable() const;

public slots:
    // 视频变换操作
    void applyRotation();
    void setRotation(int degrees);
    void toggleFlipHorizontal();
    void toggleFlipVertical();
    void toggleInvert();
    void resetAll();

    // 刷新状态
    void refreshStatus();

signals:
    // 状态变化信号
    void rotationChanged();
    void flipHorizontalChanged();
    void flipVerticalChanged();
    void invertColorsChanged();
    void transformApplied();
    void error(const QString &message);

private:
    // 缓存的状态
    int m_rotationDegrees;
    bool m_flipHorizontal;
    bool m_flipVertical;
    bool m_invertColors;
    bool m_rgaAvailable;

    // 内部辅助函数
    void updateStatus();
};

#endif // VIDEO_TRANSFORM_MANAGER_H
