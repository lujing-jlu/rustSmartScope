#include "app/image/image_processor.h"
#include <QDebug>

namespace SmartScope {
namespace App {
namespace Image {

QImage ImageProcessor::matToQImage(const cv::Mat &mat)
{
    // 检查图像为空
    if (mat.empty()) {
        qWarning() << "尝试转换空的Mat为QImage";
        return QImage();
    }

    // 根据不同的图像类型进行转换
    switch (mat.type()) {
        case CV_8UC1: {
            // 单通道灰度图像
            QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
            return image.copy(); // 创建深拷贝，因为mat.data可能在函数返回后被释放
        }
        case CV_8UC3: {
            // 三通道彩色图像（BGR格式）
            QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_RGB888);
            return image.rgbSwapped().copy(); // BGR -> RGB 并创建深拷贝
        }
        case CV_8UC4: {
            // 四通道彩色图像（BGRA格式）
            QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_ARGB32);
            return image.copy(); // 创建深拷贝
        }
        default: {
            // 处理其他格式的图像
            qWarning() << "不支持的Mat类型:" << mat.type() << "尝试转换...";
            
            // 尝试将图像转换为8位3通道
            cv::Mat temp;
            if (mat.channels() == 1) {
                // 如果是单通道，转换为三通道
                cv::cvtColor(mat, temp, cv::COLOR_GRAY2BGR);
            } else {
                // 否则尝试直接转换
                mat.convertTo(temp, CV_8UC3);
            }
            
            // 递归调用以处理转换后的图像
            return matToQImage(temp);
        }
    }
}

cv::Mat ImageProcessor::applyRectification(const cv::Mat &input, const cv::Mat &map1, const cv::Mat &map2)
{
    // 检查输入和映射表
    if (input.empty() || map1.empty() || map2.empty()) {
        qWarning() << "应用立体校正时遇到空输入";
        return input.clone(); // 返回原图像的副本
    }

    // 使用重映射进行校正
    cv::Mat rectified;
    try {
        cv::remap(input, rectified, map1, map2, cv::INTER_LINEAR);
        qDebug() << "图像校正成功，尺寸:" << rectified.cols << "x" << rectified.rows;
    } catch (const cv::Exception &e) {
        qWarning() << "图像校正失败:" << e.what();
        return input.clone(); // 出错时返回原图像的副本
    }

    return rectified;
}

QPixmap ImageProcessor::createDisplayableImage(const cv::Mat &input)
{
    if (input.empty()) {
        qWarning() << "尝试从空Mat创建显示图像";
        return QPixmap();
    }

    // 根据图像类型进行预处理
    cv::Mat displayImage;
    
    // 检查图像类型，进行适当转换
    if (input.type() == CV_8UC1) {
        // 灰度图转RGB
        cv::cvtColor(input, displayImage, cv::COLOR_GRAY2BGR);
    } else if (input.type() == CV_8UC3) {
        // 已经是BGR格式，直接使用
        displayImage = input;
    } else if (input.type() == CV_16UC1) {
        // 16位灰度图（如深度图），归一化后转彩色
        cv::Mat normalized;
        cv::normalize(input, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
        cv::cvtColor(normalized, displayImage, cv::COLOR_GRAY2BGR);
    } else {
        // 其他类型尝试转换
        qWarning() << "不支持的图像类型:" << input.type() << "尝试转换为8位RGB";
        try {
            input.convertTo(displayImage, CV_8U);
            if (displayImage.channels() == 1) {
                cv::cvtColor(displayImage, displayImage, cv::COLOR_GRAY2BGR);
            }
        } catch (const cv::Exception &e) {
            qWarning() << "图像类型转换失败:" << e.what();
            return QPixmap(); // 失败返回空图像
        }
    }
    
    // 将OpenCV Mat转换为QImage
    QImage qImg = matToQImage(displayImage);
    if (qImg.isNull()) {
        qWarning() << "Mat转QImage失败";
        return QPixmap();
    }
    
    // 转换为QPixmap
    return QPixmap::fromImage(qImg);
}

QPixmap ImageProcessor::scaleToFit(const QPixmap &pixmap, const QSize &targetSize, bool keepAspectRatio)
{
    if (pixmap.isNull() || targetSize.isEmpty()) {
        return pixmap;
    }
    
    Qt::AspectRatioMode aspectMode = keepAspectRatio ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio;
    return pixmap.scaled(targetSize, aspectMode, Qt::SmoothTransformation);
}

QPoint ImageProcessor::calculateCenteredPosition(const QSize &imageSize, const QSize &containerSize)
{
    int x = (containerSize.width() - imageSize.width()) / 2;
    int y = (containerSize.height() - imageSize.height()) / 2;
    return QPoint(std::max(0, x), std::max(0, y)); // 确保不出现负值
}

cv::Mat ImageProcessor::applyColorMap(const cv::Mat &grayImage, int colorMap)
{
    if (grayImage.empty()) {
        qWarning() << "尝试对空图像应用颜色映射";
        return cv::Mat();
    }
    
    // 确保输入为单通道灰度图
    cv::Mat normalizedImage;
    if (grayImage.type() != CV_8UC1) {
        // 如果不是8位单通道，转换一下
        if (grayImage.channels() > 1) {
            cv::cvtColor(grayImage, normalizedImage, cv::COLOR_BGR2GRAY);
        } else {
            grayImage.convertTo(normalizedImage, CV_8UC1);
        }
    } else {
        normalizedImage = grayImage;
    }
    
    // 应用颜色映射
    cv::Mat colorMapped;
    cv::applyColorMap(normalizedImage, colorMapped, colorMap);
    return colorMapped;
}

cv::Mat ImageProcessor::rotateImage(const cv::Mat &input, int angle)
{
    if (input.empty()) {
        qWarning() << "尝试旋转空图像";
        return cv::Mat();
    }
    
    // 标准化角度为90的倍数
    int normalizedAngle = ((angle % 360) + 360) % 360;
    int rotationCode;
    
    // 根据不同的旋转角度选择对应的旋转代码
    switch (normalizedAngle) {
        case 90:
            rotationCode = cv::ROTATE_90_CLOCKWISE;
            break;
        case 180:
            rotationCode = cv::ROTATE_180;
            break;
        case 270:
            rotationCode = cv::ROTATE_90_COUNTERCLOCKWISE;
            break;
        default:
            qWarning() << "不支持的旋转角度:" << angle << "，需要是90的倍数";
            return input.clone();
    }
    
    // 执行旋转
    cv::Mat rotated;
    cv::rotate(input, rotated, rotationCode);
    return rotated;
}

} // namespace Image
} // namespace App
} // namespace SmartScope 