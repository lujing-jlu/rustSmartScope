#ifndef PROFILE_CHART_DIALOG_H
#define PROFILE_CHART_DIALOG_H

#include <QDialog>
#include <QVector>
#include <QPointF>

// Forward declaration for QCustomPlot (included in cpp)
class QCustomPlot;
class QCPGraph;
class QVBoxLayout;
class QPushButton;
class QLabel;

namespace SmartScope {
namespace App {
namespace Ui {

/**
 * @brief 用于显示轮廓测量数据的图表对话框
 */
class ProfileChartDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口部件
     */
    explicit ProfileChartDialog(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ProfileChartDialog() override;

    /**
     * @brief 设置要显示的轮廓数据并更新图表
     * @param profileData 一系列 (距离, 深度/高度) 数据点
     * @param title 图表标题 (可选)
     */
    void setData(const QVector<QPointF>& profileData, const QString& title = "Profile Data");

protected:
    // --- 添加鼠标事件处理器以实现拖动 ---
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    // --- 结束添加 ---

private:
    /**
     * @brief 初始化UI组件
     */
    void setupUi();

    /**
     * @brief 配置图表外观和坐标轴
     */
    void setupPlot();

    QCustomPlot* m_customPlot;  ///< QCustomPlot 控件
    QCPGraph* m_profileGraph; ///< 图表中的曲线
    QVBoxLayout* m_mainLayout; ///< 主布局

    // --- 添加标题栏和拖动相关成员 ---
    QWidget* m_titleBar;        // 自定义标题栏容器
    QLabel* m_titleLabel;       // 显示标题的标签
    QPushButton* m_closeButton; // 关闭按钮
    QPoint m_dragPosition;      // 存储鼠标按下时的相对位置
    bool m_dragging;            // 标记是否正在拖动
    // --- 结束添加 ---
};

} // namespace Ui
} // namespace App
} // namespace SmartScope

#endif // PROFILE_CHART_DIALOG_H 