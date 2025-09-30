#ifndef MAGNIFIER_MANAGER_H
#define MAGNIFIER_MANAGER_H

#include <QObject>
#include <QSize>

// 使用前向声明减小头文件依赖
class QWidget;
class QLabel;
class QPoint;
class QPixmap;
class QPainter;
class QBitmap;

// 前向声明内部实现类
class MagnifierCreator;
class MagnifierRenderer;
class MagnifierController;

/**
 * @brief 放大镜管理器类
 * 
 * 用于创建和管理放大镜控件，提供放大镜的显示、隐藏、更新等功能
 */
class MagnifierManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit MagnifierManager(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MagnifierManager();

    /**
     * @brief 创建放大镜
     * @param contentWidget 内容区域窗口部件，用于计算位置
     * @param leftImageLabel 左图像标签，用于获取图像
     * @param leftAreaRatio 左区域的比例，用于计算分界线位置
     */
    void createMagnifier(QWidget* contentWidget, QLabel* leftImageLabel, float leftAreaRatio = 0.433f);

    /**
     * @brief 更新放大镜内容
     * @param leftImageLabel 左图像标签，用于获取图像
     */
    void updateMagnifierContent(QLabel* leftImageLabel);

    /**
     * @brief 隐藏放大镜
     */
    void hideMagnifier();

    /**
     * @brief 显示放大镜
     */
    void showMagnifier();

    /**
     * @brief 销毁放大镜
     */
    void destroyMagnifier();

    /**
     * @brief 设置是否启用放大镜
     * @param enabled 是否启用
     */
    void setEnabled(bool enabled) { m_magnifierEnabled = enabled; }

    /**
     * @brief 获取放大镜是否启用
     * @return 是否启用
     */
    bool isEnabled() const { return m_magnifierEnabled; }

    /**
     * @brief 设置放大镜缩放倍数
     * @param zoom 缩放倍数
     */
    void setZoom(double zoom) { m_magnifierZoom = zoom; }

    /**
     * @brief 获取放大镜缩放倍数
     * @return 缩放倍数
     */
    double getZoom() const { return m_magnifierZoom; }

    /**
     * @brief 设置放大镜大小
     * @param size 放大镜大小
     */
    void setMagnifierSize(const QSize& size) { m_magnifierSize = size; }

    /**
     * @brief 获取放大镜大小
     * @return 放大镜大小
     */
    QSize getMagnifierSize() const { return m_magnifierSize; }

private:
    QWidget* m_magnifierContainer = nullptr; // 放大镜容器
    QLabel* m_magnifierLabel = nullptr;      // 放大镜标签
    double m_magnifierZoom = 3.0;            // 放大倍数
    bool m_magnifierEnabled = false;         // 是否启用放大镜
    QSize m_magnifierSize = QSize(380, 380); // 放大镜大小

    // 内部实现类
    MagnifierCreator* m_creator = nullptr;
    MagnifierRenderer* m_renderer = nullptr;
    MagnifierController* m_controller = nullptr;
};

#endif // MAGNIFIER_MANAGER_H 