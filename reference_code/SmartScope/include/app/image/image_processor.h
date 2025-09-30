#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include <QImage>
#include <QPixmap>
#include <QSize>
#include <opencv2/opencv.hpp>

namespace SmartScope {
namespace App {
namespace Image {

/**
 * @brief 图像处理工具类
 * 
 * 提供各种图像处理功能，包括OpenCV Mat与Qt图像格式的转换，
 * 图像校正、预处理、显示等操作
 */
class ImageProcessor {
public:
    /**
     * @brief 将OpenCV Mat转换为QImage
     * @param mat OpenCV图像矩阵
     * @return Qt图像对象
     */
    static QImage matToQImage(const cv::Mat &mat);
    
    /**
     * @brief 应用立体校正
     * @param input 输入图像
     * @param map1 校正图1
     * @param map2 校正图2
     * @return 校正后的图像
     */
    static cv::Mat applyRectification(const cv::Mat &input, const cv::Mat &map1, const cv::Mat &map2);
    
    /**
     * @brief 创建可显示的Qt图像
     * @param input OpenCV图像
     * @return 用于显示的QPixmap
     */
    static QPixmap createDisplayableImage(const cv::Mat &input);
    
    /**
     * @brief 调整图像大小以适应标签控件
     * @param pixmap 原始图像
     * @param targetSize 目标大小
     * @param keepAspectRatio 是否保持宽高比
     * @return 调整后的图像
     */
    static QPixmap scaleToFit(const QPixmap &pixmap, const QSize &targetSize, bool keepAspectRatio = true);
    
    /**
     * @brief 计算图像在显示区域内的偏移位置
     * @param imageSize 图像尺寸
     * @param containerSize 容器尺寸
     * @return 居中显示的偏移位置
     */
    static QPoint calculateCenteredPosition(const QSize &imageSize, const QSize &containerSize);
    
    /**
     * @brief 应用伪彩色渲染到灰度图
     * @param grayImage 灰度图像
     * @param colorMap 颜色映射表类型
     * @return 伪彩色图像
     */
    static cv::Mat applyColorMap(const cv::Mat &grayImage, int colorMap = cv::COLORMAP_JET);
    
    /**
     * @brief 图像旋转
     * @param input 输入图像
     * @param angle 旋转角度(90的倍数)
     * @return 旋转后的图像
     */
    static cv::Mat rotateImage(const cv::Mat &input, int angle);
};

} // namespace Image
} // namespace App
} // namespace SmartScope

#endif // IMAGE_PROCESSOR_H 