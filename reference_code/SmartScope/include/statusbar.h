#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QPainter>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QListView>
#include <QTreeView>
#include <QFileSystemModel>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QDebug>
#include <QStyleOption>

// 包含必要的头文件
#include "app/utils/device_controller.h"
#include "infrastructure/config/config_manager.h"
#include "infrastructure/logging/logger.h"
#include "app/ui/utils/dialog_utils.h"

namespace SmartScope {

// 无需前向声明，已经包含了头文件

// 自定义电池图标类
class BatteryIcon : public QWidget
{
    Q_OBJECT
public:
    explicit BatteryIcon(QWidget *parent = nullptr);
    void setBatteryLevel(int level);
    void setDecimalBatteryLevel(float level);
    void setNotDetected(); ///< 设置为未检测到状态


    /**
     * @brief 获取推荐大小
     * @return 推荐大小
     */
    QSize sizeHint() const override;

protected:
    /**
     * @brief 绘制事件处理
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent *event) override;

private:
    float m_level;       ///< 电量百分比
    bool m_hasDecimal;   ///< 是否有小数
    bool m_notDetected;  ///< 是否为未检测到状态
    QColor m_color;      ///< 电量颜色
};

// 自定义文件选择对话框，类似安卓风格
class AndroidStyleFileDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AndroidStyleFileDialog(const QString &rootDir, const QString &currentDir, QWidget *parent = nullptr);
    QString selectedPath() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onItemDoubleClicked(const QModelIndex &index);
    void onCreateFolder();
    void onDeleteFolder();
    void onRenameFolder();
    void onSelectionChanged(const QModelIndex &current, const QModelIndex &previous);

private:
    QFileSystemModel *m_model;
    QTreeView *m_treeView;
    QPushButton *m_createFolderButton;
    QPushButton *m_deleteFolderButton;
    QPushButton *m_renameFolderButton;
    QString m_rootDirectory;
    QString m_selectedPath;
    
    // 拖动相关
    bool m_isDragging;
    QPoint m_dragPosition;
};

// 路径选择器类
class PathSelector : public QPushButton
{
    Q_OBJECT
public:
    explicit PathSelector(QWidget *parent = nullptr);
    QString getCurrentPath() const;
    void setCurrentPath(const QString &path);

signals:
    void pathChanged(const QString &path);

public slots:
    void showFileDialog();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // 获取用于显示的路径（相对于根目录）
    QString getDisplayPath(const QString &path) const;
    
    QString m_currentPath;
    QString m_rootDirectory;
};

// 状态栏组件类
class StatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit StatusBar(QWidget *parent = nullptr);
    ~StatusBar();
    
    /**
     * @brief 计算状态栏的最佳宽度
     * @return 最佳宽度
     */
    int calculateOptimalWidth() const;
    
    /**
     * @brief 获取路径选择器
     * @return 路径选择器指针
     */
    PathSelector* getPathSelector() const;

public slots:
    void updateDateTime();
    void updateBatteryStatus();
    void updateFpsDisplay(float leftFps, float rightFps);
    void updateTemperatureDisplay(float temperature);
    void onDeviceStatusUpdated(const DeviceController::DeviceStatus& status);

protected:
    /**
     * @brief 显示事件处理
     * @param event 显示事件
     */
    void showEvent(QShowEvent *event) override;
    
    /**
     * @brief 隐藏事件处理
     * @param event 隐藏事件
     */
    void hideEvent(QHideEvent *event) override;
    
    /**
     * @brief 调整大小事件处理
     * @param event 调整大小事件
     */
    void resizeEvent(QResizeEvent *event) override;
    
    /**
     * @brief 绘制事件处理
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUI();
    void startTimers();
    
    /**
     * @brief 调整状态栏大小以适应内容
     */
    void adjustSizeToContent();

    /**
     * @brief 初始化设备控制器连接
     */
    void initDeviceController();

    QLabel *m_appNameLabel;           ///< 应用名称标签
    QLabel *m_dateTimeLabel;          ///< 日期时间标签
    BatteryIcon *m_batteryIcon;       ///< 电池图标
    QLabel *m_temperatureIconLabel;   ///< 温度SVG图标标签
    QLabel *m_temperatureTextLabel;   ///< 温度文本标签
    QLabel *m_fpsLabel;               ///< 帧率标签
    PathSelector *m_pathSelector;     ///< 路径选择器
    QTimer *m_dateTimeTimer;          ///< 日期时间定时器
    QTimer *m_batteryTimer;           ///< 电池状态定时器（将被设备控制器替代）
};

} // namespace SmartScope

#endif // STATUSBAR_H 