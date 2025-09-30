#ifndef SCREENSHOT_PREVIEW_PAGE_H
#define SCREENSHOT_PREVIEW_PAGE_H

#include "app/ui/base_page.h"
#include <QFileSystemWatcher>
#include <QTimer>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QMap>
#include <QFileInfo>
#include <QDateTime>
#include <QDialog>
#include <QPointer>

// 前向声明
class ScreenshotImageCard;
class ScreenshotImagePreviewDialog;

/**
 * @brief 截屏预览页面组件
 */
class ScreenshotPreviewPage : public BasePage
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit ScreenshotPreviewPage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ScreenshotPreviewPage();

    /**
     * @brief 设置当前工作路径
     * @param path 工作路径
     */
    void setCurrentWorkPath(const QString &path);

private slots:
    /**
     * @brief 处理目录变化
     * @param path 变化的目录路径
     */
    void handleDirectoryChanged(const QString &path);
    
    /**
     * @brief 处理文件变化
     * @param path 变化的文件路径
     */
    void handleFileChanged(const QString &path);
    
    /**
     * @brief 显示图片预览
     * @param imagePath 图片路径
     */
    void showImagePreview(const QString &imagePath);

    /**
     * @brief 长按触发回调
     */
    void handleLongPress();

protected:
    /**
     * @brief 事件过滤器
     * @param watched 被监视的对象
     * @param event 事件
     * @return 是否处理了事件
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /**
     * @brief 初始化内容
     */
    void initContent();
    
    /**
     * @brief 加载图片
     */
    void loadImages();
    
    /**
     * @brief 创建图片卡片
     * @param filePath 文件路径
     * @return 图片卡片指针
     */
    ScreenshotImageCard* createImageCard(const QString &filePath);
    
    /**
     * @brief 清除图片卡片
     */
    void clearImageCards();
    
    /**
     * @brief 更新布局
     */
    void updateLayout();

    /**
     * @brief 显示树状操作列表（含删除）
     */
    void showContextTreeForFile(const QString &filePath);

    QString m_currentWorkPath;                      ///< 当前工作路径
    QFileSystemWatcher *m_fileWatcher;              ///< 文件系统监视器
    QTimer *m_reloadTimer;                          ///< 重新加载定时器
    QScrollArea *m_scrollArea;                      ///< 滚动区域
    QWidget *m_scrollContent;                       ///< 滚动内容
    QGridLayout *m_gridLayout;                      ///< 网格布局
    QLabel *m_emptyLabel;                           ///< 空目录提示标签
    QList<ScreenshotImageCard*> m_imageCards;       ///< 图片卡片列表
    int m_columnsCount;                             ///< 列数
    int m_cardSpacing;                              ///< 卡片间距
    int m_cardWidth;                                ///< 卡片宽度
    bool m_isLoading;                               ///< 是否正在加载
    ScreenshotImagePreviewDialog *m_previewDialog;  ///< 预览对话框
    bool m_isScrolling;                             ///< 是否正在滚动
    QPoint m_lastMousePos;                          ///< 上次鼠标位置
    QPoint m_pressPos;                              ///< 按下位置
    int m_scrollSpeed;                              ///< 滚动速度
    int m_scrollThreshold;                          ///< 滚动阈值

    // 双击检测（与拍照预览页面保持一致）
    qint64 m_lastClickTime = 0;                     ///< 上次点击时间
    ScreenshotImageCard *m_lastClickedCard = nullptr; ///< 上次点击的卡片
    ScreenshotImageCard *m_selectedCard = nullptr;  ///< 当前选中的卡片

    // 长按检测
    QTimer *m_longPressTimer = nullptr;             ///< 长按计时器
    bool m_longPressTriggered = false;              ///< 是否已触发长按
};

/**
 * @brief 截屏图片卡片组件
 */
class ScreenshotImageCard : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param filePath 文件路径
     * @param parent 父组件
     */
    explicit ScreenshotImageCard(const QString &filePath, QWidget *parent = nullptr);

    /**
     * @brief 获取图片文件路径
     * @return 图片文件路径
     */
    QString getFilePath() const;

signals:
    /**
     * @brief 双击信号
     * @param filePath 文件路径
     */
    void doubleClicked(const QString &filePath);

protected:
    /**
     * @brief 鼠标双击事件
     * @param event 事件
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    
    /**
     * @brief 绘制事件
     * @param event 事件
     */
    void paintEvent(QPaintEvent *event) override;
    
    /**
     * @brief 鼠标进入事件
     * @param event 事件
     */
    void enterEvent(QEvent *event) override;
    
    /**
     * @brief 鼠标离开事件
     * @param event 事件
     */
    void leaveEvent(QEvent *event) override;

private:
    /**
     * @brief 加载缩略图
     */
    void loadThumbnail();

    QString m_filePath;                             ///< 图片文件路径
    QFileInfo m_fileInfo;                           ///< 图片文件信息
    QPixmap m_thumbnail;                            ///< 缩略图
    QLabel *m_imageLabel;                           ///< 图片标签
    QLabel *m_nameLabel;                            ///< 名称标签
    QLabel *m_infoLabel;                            ///< 信息标签
};

/**
 * @brief 截屏图片预览对话框
 */
class ScreenshotImagePreviewDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit ScreenshotImagePreviewDialog(QWidget *parent = nullptr);
    
    /**
     * @brief 设置图片
     * @param imagePath 图片路径
     */
    void setImage(const QString &imagePath);
    
    /**
     * @brief 如果对话框打开则关闭它（静态方法）
     */
    static void closeIfOpen();

protected:
    /**
     * @brief 键盘按下事件
     * @param event 事件
     */
    void keyPressEvent(QKeyEvent *event) override;
    
    /**
     * @brief 调整大小事件
     * @param event 事件
     */
    void resizeEvent(QResizeEvent *event) override;
    
    /**
     * @brief 事件过滤器
     * @param watched 被监视的对象
     * @param event 事件
     * @return 是否处理了事件
     */
    bool eventFilter(QObject *watched, QEvent *event) override;
    
    /**
     * @brief 关闭事件
     * @param event 关闭事件
     */
    void closeEvent(QCloseEvent *event) override;

private slots:
    /**
     * @brief 放大图片
     */
    void zoomIn();
    
    /**
     * @brief 缩小图片
     */
    void zoomOut();
    
    /**
     * @brief 重置缩放
     */
    void resetZoom();

private:
    /**
     * @brief 更新图像显示
     */
    void updateImageDisplay();

    QLabel *m_imageLabel;                          ///< 图片标签
    QLabel *m_infoLabel;                           ///< 信息标签
    QString m_imagePath;                           ///< 图片路径
    QImage m_originalImage;                        ///< 原始图像
    double m_zoomFactor;                           ///< 缩放因子
    bool m_userZoomed;                            ///< 用户是否手动缩放
    
    static QPointer<ScreenshotImagePreviewDialog> m_instance; ///< 单例实例
};

#endif // SCREENSHOT_PREVIEW_PAGE_H
