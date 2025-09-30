#ifndef PREVIEW_SELECTION_PAGE_H
#define PREVIEW_SELECTION_PAGE_H

#include "app/ui/base_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

/**
 * @brief 预览选择页面
 * 
 * 用户选择查看拍照预览还是截屏预览
 */
class PreviewSelectionPage : public BasePage
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit PreviewSelectionPage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~PreviewSelectionPage();

signals:
    /**
     * @brief 选择拍照预览信号
     */
    void photoPreviewSelected();
    
    /**
     * @brief 选择截屏预览信号
     */
    void screenshotPreviewSelected();

    /**
     * @brief 选择视频预览信号
     */
    void videoPreviewSelected();

private slots:
    /**
     * @brief 拍照预览按钮点击
     */
    void onPhotoPreviewClicked();
    
    /**
     * @brief 截屏预览按钮点击
     */
    void onScreenshotPreviewClicked();

    /**
     * @brief 视频预览按钮点击
     */
    void onVideoPreviewClicked();

private:
    /**
     * @brief 初始化内容
     */
    void initContent();
    
    /**
     * @brief 创建选择按钮
     * @param iconPath 图标路径
     * @param title 标题
     * @param description 描述
     * @return 按钮指针
     */
    QPushButton* createSelectionButton(const QString &iconPath, const QString &title, const QString &description);

    QLabel *m_titleLabel;               ///< 标题标签
    QPushButton *m_photoButton;         ///< 拍照预览按钮
    QPushButton *m_screenshotButton;    ///< 截屏预览按钮
    QPushButton *m_videoButton;         ///< 视频预览按钮
};

#endif // PREVIEW_SELECTION_PAGE_H
