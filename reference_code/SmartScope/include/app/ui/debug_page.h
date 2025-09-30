#ifndef DEBUG_PAGE_H
#define DEBUG_PAGE_H

#include "app/ui/base_page.h"
#include "app/ui/clickable_image_label.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QImage>
#include <opencv2/opencv.hpp>
#include <QtWidgets>

namespace SmartScope {
namespace App {
namespace Ui {

/**
 * @brief 调试页面组件
 * 
 * 显示校正后的左图、深度图、预测深度图、校准后的预测深度图
 */
class DebugPage : public BasePage
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父组件
     */
    explicit DebugPage(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~DebugPage() override;

    /**
     * @brief 设置调试图像
     * @param rectifiedLeftImage 校正后的左图
     * @param depthMap 深度图
     * @param predictedDepthMap 预测深度图
     * @param calibratedPredictedDepthMap 校准后的预测深度图
     */
    void setDebugImages(const cv::Mat &rectifiedLeftImage, 
                       const cv::Mat &depthMap,
                       const cv::Mat &predictedDepthMap,
                       const cv::Mat &calibratedPredictedDepthMap);

protected:
    /**
     * @brief 初始化内容区域
     */
    void initContent() override;
    
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

private slots:
    /**
     * @brief 处理返回按钮点击
     */
    void handleBackButtonClicked();

private:
    /**
     * @brief 创建图像显示区域
     */
    void createImageDisplayArea();
    
    /**
     * @brief 更新图像显示
     */
    void updateImageDisplays();
    
    /**
     * @brief 将OpenCV Mat转换为QPixmap
     * @param mat OpenCV Mat
     * @return QPixmap
     */
    QPixmap matToPixmap(const cv::Mat &mat);
    
    /**
     * @brief 将深度图转换为彩色图像
     * @param depthMap 深度图
     * @return 彩色深度图
     */
    cv::Mat depthMapToColor(const cv::Mat &depthMap);

private:
    // 图像标签
    ClickableImageLabel *m_rectifiedLeftLabel;      // 校正后的左图
    ClickableImageLabel *m_depthMapLabel;           // 深度图
    ClickableImageLabel *m_predictedDepthLabel;     // 预测深度图
    ClickableImageLabel *m_calibratedDepthLabel;    // 校准后的预测深度图
    
    // 图像数据
    cv::Mat m_rectifiedLeftImage;
    cv::Mat m_depthMap;
    cv::Mat m_predictedDepthMap;
    cv::Mat m_calibratedPredictedDepthMap;
    
    // 布局
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_imageLayout;
    
    // 标签
    QLabel *m_rectifiedLeftTitle;
    QLabel *m_depthMapTitle;
    QLabel *m_predictedDepthTitle;
    QLabel *m_calibratedDepthTitle;
    
    // 返回按钮
    QPushButton *m_backButton;
};

} // namespace Ui
} // namespace App
} // namespace SmartScope

#endif // DEBUG_PAGE_H 