#include "app/ui/measurement_renderer.h"
#include "infrastructure/logging/logger.h"
#include "app/ui/point_cloud_gl_widget.h"
#include <QColor>
#include <QtGlobal> // for qBound
#include <cmath> // for fabs
#include <QString> // 添加 QString
#include <QPoint>  // 添加 QPoint
#include <QSize>   // 添加 QSize
#include <QVector> // 添加 QVector
#include <QVector3D> // 添加 QVector3D
#include <opencv2/freetype.hpp>
#include <QCoreApplication>
#include <QDir>

using namespace SmartScope::App::Calibration;

namespace SmartScope {
namespace App {
namespace Ui {

// --- Initialize Static Constants ---
const cv::Scalar MeasurementRenderer::DEFAULT_TEXT_COLOR = cv::Scalar(255, 255, 255); // White

MeasurementRenderer::MeasurementRenderer()
{
    LOG_INFO("MeasurementRenderer 已创建");
    try {
        ft2 = cv::freetype::createFreeType2();
        // --- 查找字体文件 ---
        QString fontPath;
        QString fontFileName = "wqy-zenhei.ttc"; // 或者你的字体文件名 e.g., NotoSansCJKsc-Regular.otf
        QStringList possibleBasePaths = {
            QCoreApplication::applicationDirPath() + "/../resources/fonts/", // 优先使用打包的字体
             ".", // 检查构建目录的 resources/fonts
             QCoreApplication::applicationDirPath() + "/resources/fonts/", // 检查可执行文件同级的 resources/fonts
             "./resources/fonts/", // 相对当前工作目录
             "/usr/share/fonts/truetype/wqy/", // 常见系统路径 (Linux)
             "/usr/share/fonts/wenquanyi/wqy-zenhei/" // 另一种常见系统路径
            // 添加其他可能的系统路径
        };

        for (const QString& basePath : possibleBasePaths) {
            QString currentPath = QDir(basePath).filePath(fontFileName);
             if (QFile::exists(currentPath)) {
                fontPath = QDir(currentPath).canonicalPath(); // 获取绝对路径
                LOG_INFO(QString("找到字体文件: %1").arg(fontPath));
                break;
             }
        }

        if (!fontPath.isEmpty()) {
             ft2->loadFontData(fontPath.toStdString(), 0);
             LOG_INFO("FreeType 字体加载成功");
        } else {
             LOG_ERROR(QString("未找到字体文件 '%1'，文本渲染可能不正确。检查路径: %2").arg(fontFileName).arg(possibleBasePaths.join(", ")));
             ft2.release(); // 如果找不到字体，释放指针，后面会用 fallback
        }
        // --- 字体查找结束 ---

    } catch (const cv::Exception& e) {
        LOG_ERROR(QString("初始化 FreeType 失败: %1").arg(e.what()));
        ft2.release(); // 确保指针为空
    }
}

MeasurementRenderer::~MeasurementRenderer()
{
    LOG_INFO("MeasurementRenderer 已销毁");
}

cv::Mat MeasurementRenderer::drawMeasurements(
    const cv::Mat& baseImage,
    const QVector<MeasurementObject*>& measurements,
    std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager,
    const QSize& originalImageSize)
{
    // 创建一个干净的图像副本进行绘制
    cv::Mat displayImage = baseImage.clone();

    // 计算缩放比例因子 (原始图像到显示图像)
    float scaleX = 1.0f, scaleY = 1.0f;
    if (originalImageSize.width() > 0 && originalImageSize.height() > 0 &&
        displayImage.cols > 0 && displayImage.rows > 0) {
        scaleX = static_cast<float>(displayImage.cols) / originalImageSize.width();
        scaleY = static_cast<float>(displayImage.rows) / originalImageSize.height();
        LOG_INFO(QString("MeasurementRenderer - 图像绘制比例因子: scaleX=%1, scaleY=%2").arg(scaleX, 0, 'f', 3).arg(scaleY, 0, 'f', 3));
    } else {
        LOG_WARNING(QString("MeasurementRenderer - 无法计算绘制比例因子 - 原始尺寸:%1x%2, 显示尺寸:%3x%4")
                    .arg(originalImageSize.width()).arg(originalImageSize.height())
                    .arg(displayImage.cols).arg(displayImage.rows));
        // 如果无法计算比例，默认使用 1.0，但这可能导致绘制不准确
    }

    // 获取相机参数 (用于3D投影) - // REMOVED: No longer needed for primary drawing
    /*
    cv::Mat cameraMatrix;
    bool paramsOk = false;
    if (calibrationHelper && calibrationHelper->areParametersLoaded()) {
        cameraMatrix = calibrationHelper->getCameraMatrixLeft();
        if (!cameraMatrix.empty()) {
            paramsOk = true;
        }
    }
    */

    LOG_INFO(QString("MeasurementRenderer - 开始绘制 %1 个测量对象").arg(measurements.size()));

    // 将变量声明移到循环外面，避免case标签问题
    auto stereoHelper = correctionManager ? correctionManager->getStereoCalibrationHelper() : nullptr;

    // 绘制每个测量对象
    for (MeasurementObject* measurement : measurements) {
        if (measurement && measurement->isVisible()) {
            MeasurementType type = measurement->getType();
            const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
            const QVector<QVector3D>& points3D = measurement->getPoints(); // 毫米单位

            // --- 统一基于 ClickPoints 绘制, 但保留 points3D 用于 Depth 计算 ---
            switch (type) {
                case MeasurementType::Length:
                    if (clickPoints.size() >= 2) {
                        LOG_DEBUG("绘制长度测量 (基于ClickPoints)");
                        drawLengthFromClickPoints(displayImage, measurement); // 使用明确的函数名
                    } else {
                        LOG_DEBUG(QString("跳过绘制长度测量: 点击点=%1").arg(clickPoints.size()));
                    }
                    break;

                case MeasurementType::PointToLine:
                    if (clickPoints.size() == 3) {
                        LOG_DEBUG("绘制点到线测量 (基于ClickPoints)");
                        drawPointToLineFromClickPoints(displayImage, measurement);
                    } else {
                        LOG_DEBUG(QString("跳过绘制点到线测量: 点击点=%1").arg(clickPoints.size()));
                    }
                    break;

                case MeasurementType::Depth:
                    if (clickPoints.size() == 4 && points3D.size() == 4 && stereoHelper && stereoHelper->isRemapInitialized()) {
                        LOG_DEBUG("计算并绘制深度(点到面)测量 (基于ClickPoints)");
                        
                        // 1. 获取 3D 点 (毫米)
                        QVector3D p1_3d = points3D[0];
                        QVector3D p2_3d = points3D[1];
                        QVector3D p3_3d = points3D[2];
                        QVector3D p4_3d = points3D[3]; // 目标点

                        // 2. 计算平面法向量和投影点 (3D)
                        QVector3D v1 = p2_3d - p1_3d;
                        QVector3D v2 = p3_3d - p1_3d;
                        QVector3D normal = QVector3D::crossProduct(v1, v2);
                        QVector3D projectionPoint3D = p4_3d; // 默认值以防出错
                        if (normal.lengthSquared() > 1e-6f) { // 避免三点共线
                            normal.normalize();
                            float dist = QVector3D::dotProduct(p4_3d - p1_3d, normal);
                            projectionPoint3D = p4_3d - dist * normal;
                        } else {
                            LOG_WARNING("Depth测量：平面三点共线，投影点计算可能不准确");
                            // 可以选择不绘制或使用 p1 作为投影点
                            projectionPoint3D = p1_3d; // 简化处理
                        }

                        // --- 修改：使用投影矩阵 P1 进行 3D 到 2D 投影 ---
                        QPoint projectionPoint2D(-1, -1); // 默认无效值
                        // 尝试获取左相机投影矩阵 P1 (3x4)
                        cv::Mat P1 = stereoHelper->getP1();
                        
                        if (!P1.empty() && P1.rows == 3 && P1.cols == 4) {
                            // 将 3D 点转换为齐次坐标 (4x1 Mat, double)
                            cv::Mat point3D_h = (cv::Mat_<double>(4,1) << 
                                projectionPoint3D.x(), 
                                projectionPoint3D.y(), 
                                projectionPoint3D.z(), 
                                1.0);
                            
                            // 执行投影: projected_h (3x1) = P1 (3x4) * point3D_h (4x1)
                            cv::Mat projected_h = P1 * point3D_h;
                            
                            // 转换为非齐次坐标 (u = x/w, v = y/w)
                            double w = projected_h.at<double>(2, 0);
                            if (std::abs(w) > 1e-6) { // 避免除以零
                                double u = projected_h.at<double>(0, 0) / w;
                                double v = projected_h.at<double>(1, 0) / w;
                                
                                // 边界检查并设置 QPoint
                                projectionPoint2D.setX(qBound(0, static_cast<int>(u), displayImage.cols - 1));
                                projectionPoint2D.setY(qBound(0, static_cast<int>(v), displayImage.rows - 1));
                                LOG_INFO(QString("投影点计算 (使用P1): 3D(%1,%2,%3) -> 2D(%4,%5)")
                                         .arg(projectionPoint3D.x(), 0, 'f', 1).arg(projectionPoint3D.y(), 0, 'f', 1).arg(projectionPoint3D.z(), 0, 'f', 1)
                                         .arg(projectionPoint2D.x()).arg(projectionPoint2D.y()));
                            } else {
                                QString errorMsg = QString("投影计算失败：齐次坐标 w 过小");
                                LOG_ERROR(errorMsg);
                            }
                        } else {
                            QString errorMsg = QString("无法获取有效的左相机投影矩阵 P1 或 P1 尺寸不正确 (%1 x %2)").arg(P1.rows).arg(P1.cols);
                            LOG_ERROR(errorMsg);
                            // 在这里可以回退到 cv::projectPoints (如果需要)
                        }
                        // --- 结束修改 ---
                        
                        // 4. 调用绘制函数
                        if (projectionPoint2D.x() >= 0 && projectionPoint2D.y() >= 0) { // 确保投影有效
                            drawDepthMeasurementVisuals(displayImage, measurement, projectionPoint2D);
                        } else {
                             LOG_ERROR("投影点无效，无法绘制深度测量细节");
                             // 可以选择只绘制点击的点
                             drawDepthMeasurementVisuals(displayImage, measurement, clickPoints[0]); // 或者不画细节
                        }

            } else {
                        LOG_DEBUG(QString("跳过绘制深度(点到面)测量: 数据不足 (点击点=%1, 3D点=%2) 或 Helper无效")
                                  .arg(clickPoints.size()).arg(points3D.size()));
                    }
                    break;

                case MeasurementType::Area:
                    if (clickPoints.size() >= 3) {
                        LOG_DEBUG("绘制面积测量 (基于ClickPoints)");
                        drawAreaFromClickPoints(displayImage, measurement);
                    } else {
                         LOG_DEBUG(QString("跳过绘制面积测量: 点击点=%1 (需要 >= 3)").arg(clickPoints.size()));
                    }
                    break;

                case MeasurementType::Polyline:
                    if (clickPoints.size() >= 2) {
                        LOG_DEBUG("绘制折线测量 (基于ClickPoints)");
                        drawPolylineFromClickPoints(displayImage, measurement);
                    } else {
                         LOG_DEBUG(QString("跳过绘制折线测量: 点击点=%1 (需要 >= 2)").arg(clickPoints.size()));
                    }
                    break;
                
                case MeasurementType::Profile:
                     if (clickPoints.size() == 2) { // 轮廓线需要两个点
                        LOG_DEBUG("绘制轮廓测量线 (基于ClickPoints)");
                        drawProfileFromClickPoints(displayImage, measurement);
                    } else {
                         LOG_DEBUG(QString("跳过绘制轮廓测量: 点击点=%1 (需要 2)").arg(clickPoints.size()));
                    }
                    break;

                case MeasurementType::RegionProfile:
                default:
                    LOG_DEBUG(QString("跳过绘制测量对象: 类型=%1 (暂不支持或数据不足)")
                              .arg(static_cast<int>(type)));
                    break;

                case MeasurementType::MissingArea:
                    if (clickPoints.size() >= 5) {
                        LOG_DEBUG("绘制补缺测量 (基于ClickPoints)");
                        drawMissingAreaFromClickPoints(displayImage, measurement);
                    } else {
                        LOG_DEBUG(QString("跳过绘制补缺测量: 点击点=%1").arg(clickPoints.size()));
                    }
                    break;
            }
        }
    }
    LOG_INFO("MeasurementRenderer - 测量对象绘制完成");
    // 添加调试日志：检查返回的图像
    LOG_DEBUG(QString("MeasurementRenderer - 返回绘制后的图像: %1x%2, 类型: %3, 是否为空: %4")
             .arg(displayImage.cols).arg(displayImage.rows).arg(displayImage.type()).arg(displayImage.empty()));
    #ifdef QT_DEBUG
        // 可以在Debug模式下保存图像查看
        // cv::imwrite("debug_rendered_image.png", displayImage);
    #endif
    return displayImage;
}

void MeasurementRenderer::drawTemporaryMeasurement(
    cv::Mat& image,
    const QVector<QPoint>& originalClickPoints,
    const QVector<QVector3D>& measurementPoints,
    MeasurementType type)
{
    if (image.empty() || originalClickPoints.isEmpty()) {
        return;
    }

    LOG_DEBUG(QString("绘制临时测量，点数: %1, 类型: %2").arg(originalClickPoints.size()).arg(static_cast<int>(type)));

    // 特殊处理MissingArea类型
    if (type == MeasurementType::MissingArea) {
        // 定义颜色
        cv::Scalar lineColor(255, 165, 0);   // 橙色 - 实际线段
        cv::Scalar rayColor(0, 255, 255);    // 青色 - 射线和虚线
        cv::Scalar intersectionColor(0, 255, 0); // 绿色 - 交点
        cv::Scalar pointColor(255, 0, 0);    // 红色 - 普通点
        cv::Scalar polygonColor(0, 255, 255);  // 黄色 - 多边形边缘
        
        // 如果点数少于5，则需要绘制初始线段以便用户选择
        if (originalClickPoints.size() < 5) {
            // 绘制用户添加的点
    for (int i = 0; i < originalClickPoints.size(); ++i) {
        QPoint point = originalClickPoints[i];
        // 检查坐标有效性
        if (point.x() >= 0 && point.x() < image.cols &&
            point.y() >= 0 && point.y() < image.rows) {
                    
            cv::Point cvPoint(point.x(), point.y());
            // 绘制点
                    cv::circle(image, cvPoint, 10, pointColor, -1);
                    cv::circle(image, cvPoint, 12, cv::Scalar(0, 0, 0), 2);
                }
            }
            
            // 当点数大于等于2时，绘制第一条线段
            if (originalClickPoints.size() >= 2) {
                QPoint p1 = originalClickPoints[0];
                QPoint p2 = originalClickPoints[1];
                
                // 只有当点在图像内时才绘制
                if (p1.x() >= 0 && p1.x() < image.cols && p1.y() >= 0 && p1.y() < image.rows &&
                    p2.x() >= 0 && p2.x() < image.cols && p2.y() >= 0 && p2.y() < image.rows) {
                    
                    cv::Point cvP1(p1.x(), p1.y());
                    cv::Point cvP2(p2.x(), p2.y());
                    cv::line(image, cvP1, cvP2, lineColor, 3); // 线宽增加到3
                }
                
                // 计算线段并绘制可见部分的射线
                cv::Point2f p1f, p2f;
                bool p1Valid = false, p2Valid = false;
                
                if (p1.x() >= 0 && p1.x() < image.cols && p1.y() >= 0 && p1.y() < image.rows) {
                    p1f = cv::Point2f(p1.x(), p1.y());
                    p1Valid = true;
                }
                
                if (p2.x() >= 0 && p2.x() < image.cols && p2.y() >= 0 && p2.y() < image.rows) {
                    p2f = cv::Point2f(p2.x(), p2.y());
                    p2Valid = true;
                }
                
                // 如果至少一个点在图像内，计算射线
                if (p1Valid || p2Valid) {
                    // 如果两点都不在图像内，我们不画射线
                    if (!p1Valid && !p2Valid) {
                        // 跳过绘制
                    } 
                    else {
                        // 确保有足够的信息来计算方向
                        cv::Point2f direction;
                        cv::Point2f startPoint;
                        
                        if (p1Valid && p2Valid) {
                            // 两点都在图像内，正常计算
                            direction = cv::Point2f(p2f.x - p1f.x, p2f.y - p1f.y);
                            startPoint = p2f;
                        } else if (p2Valid) {
                            // 只有P2在图像内，使用原始方向
                            direction = cv::Point2f(p2.x() - p1.x(), p2.y() - p1.y());
                            startPoint = p2f;
                        } else {
                            // 只有P1在图像内，使用原始方向的相反方向
                            direction = cv::Point2f(p1.x() - p2.x(), p1.y() - p2.y());
                            startPoint = p1f;
                        }
                        
                        float length = sqrt(direction.x * direction.x + direction.y * direction.y);
                        if (length > 0) {
                            direction.x /= length;
                            direction.y /= length;
                            
                            // 延长射线 (长度为图像对角线的2倍)
                            float rayLength = 2.0f * sqrt(image.cols * image.cols + image.rows * image.rows);
                            cv::Point2f rayEnd(startPoint.x + direction.x * rayLength, startPoint.y + direction.y * rayLength);
                            
                            // 计算射线与图像边界的交点
                            cv::Point rayEndInImage;
                            bool rayIntersectsImage = clipLineToImage(startPoint, rayEnd, image.cols, image.rows, rayEndInImage);
                            
                            // 只有当射线与图像相交时才绘制
                            if (rayIntersectsImage) {
                                // 绘制射线延长部分 (虚线)
                                drawDashedLine(image, cv::Point(startPoint.x, startPoint.y), rayEndInImage, rayColor, 2, 10, 6);
                            }
                        }
                    }
                }
            }
            
            // 当点数大于等于4时，绘制第二条线段和射线
            if (originalClickPoints.size() >= 4) {
                QPoint p3 = originalClickPoints[2];
                QPoint p4 = originalClickPoints[3];
                
                // 只有当点在图像内时才绘制线段
                if (p3.x() >= 0 && p3.x() < image.cols && p3.y() >= 0 && p3.y() < image.rows &&
                    p4.x() >= 0 && p4.x() < image.cols && p4.y() >= 0 && p4.y() < image.rows) {
                    
                    cv::Point cvP3(p3.x(), p3.y());
                    cv::Point cvP4(p4.x(), p4.y());
                    cv::line(image, cvP3, cvP4, lineColor, 3); // 线宽增加到3
                }
                
                // 计算线段并绘制可见部分的射线
                cv::Point2f p3f, p4f;
                bool p3Valid = false, p4Valid = false;
                
                if (p3.x() >= 0 && p3.x() < image.cols && p3.y() >= 0 && p3.y() < image.rows) {
                    p3f = cv::Point2f(p3.x(), p3.y());
                    p3Valid = true;
                }
                
                if (p4.x() >= 0 && p4.x() < image.cols && p4.y() >= 0 && p4.y() < image.rows) {
                    p4f = cv::Point2f(p4.x(), p4.y());
                    p4Valid = true;
                }
                
                // 如果至少一个点在图像内，计算射线
                if (p3Valid || p4Valid) {
                    // 如果两点都不在图像内，我们不画射线
                    if (!p3Valid && !p4Valid) {
                        // 跳过绘制
                    } 
                    else {
                        // 确保有足够的信息来计算方向
                        cv::Point2f direction;
                        cv::Point2f startPoint;
                        
                        if (p3Valid && p4Valid) {
                            // 两点都在图像内，正常计算
                            direction = cv::Point2f(p4f.x - p3f.x, p4f.y - p3f.y);
                            startPoint = p4f;
                        } else if (p4Valid) {
                            // 只有P4在图像内，使用原始方向
                            direction = cv::Point2f(p4.x() - p3.x(), p4.y() - p3.y());
                            startPoint = p4f;
                        } else {
                            // 只有P3在图像内，使用原始方向的相反方向
                            direction = cv::Point2f(p3.x() - p4.x(), p3.y() - p4.y());
                            startPoint = p3f;
                        }
                        
                        float length = sqrt(direction.x * direction.x + direction.y * direction.y);
                        if (length > 0) {
                            direction.x /= length;
                            direction.y /= length;
                            
                            // 延长射线
                            float rayLength = 2.0f * sqrt(image.cols * image.cols + image.rows * image.rows);
                            cv::Point2f rayEnd(startPoint.x + direction.x * rayLength, startPoint.y + direction.y * rayLength);
                            
                            // 计算射线与图像边界的交点
                            cv::Point rayEndInImage;
                            bool rayIntersectsImage = clipLineToImage(startPoint, rayEnd, image.cols, image.rows, rayEndInImage);
                            
                            // 只有当射线与图像相交时才绘制
                            if (rayIntersectsImage) {
                                // 绘制射线延长部分 (虚线)
                                drawDashedLine(image, cv::Point(startPoint.x, startPoint.y), rayEndInImage, rayColor, 2, 10, 6);
                            }
                        }
                    }
                }
            }
        }
        // 当有5个或更多点时（已有交点），处理交点和后续的多边形
        else if (originalClickPoints.size() >= 5) {
            // 获取交点 - 保持交点的原始位置，无论是否在图像内
            QPoint intersectionPoint = originalClickPoints[4];
            bool intersectionInImage = (intersectionPoint.x() >= 0 && intersectionPoint.x() < image.cols &&
                                      intersectionPoint.y() >= 0 && intersectionPoint.y() < image.rows);
            
            // 绘制原始四个点
            for (int i = 0; i < 4; ++i) {
                QPoint p = originalClickPoints[i];
                if (p.x() >= 0 && p.x() < image.cols && p.y() >= 0 && p.y() < image.rows) {
                    cv::Point cvPoint(p.x(), p.y());
                    cv::circle(image, cvPoint, 6, cv::Scalar(0, 255, 0), -1); // 绿色点
                    cv::circle(image, cvPoint, 8, cv::Scalar(0, 0, 0), 1); // 黑色边框
                }
            }
            
            // 绘制两条原始线段
            if (originalClickPoints.size() >= 4) {
                for (int i = 0; i < 4; i += 2) {
                    QPoint p1 = originalClickPoints[i];
                    QPoint p2 = originalClickPoints[i+1];
                    
                    // 只有当点在图像内时才绘制线段
                    if (p1.x() >= 0 && p1.x() < image.cols && p1.y() >= 0 && p1.y() < image.rows &&
                        p2.x() >= 0 && p2.x() < image.cols && p2.y() >= 0 && p2.y() < image.rows) {
                        cv::Point cvP1(p1.x(), p1.y());
                        cv::Point cvP2(p2.x(), p2.y());
                        cv::line(image, cvP1, cvP2, lineColor, 2);
                    }
                    
                    // 计算射线并绘制可见部分
                    cv::Point2f p1f, p2f;
                    bool p1Valid = false, p2Valid = false;
                    
                    if (p1.x() >= 0 && p1.x() < image.cols && p1.y() >= 0 && p1.y() < image.rows) {
                        p1f = cv::Point2f(p1.x(), p1.y());
                        p1Valid = true;
                    }
                    
                    if (p2.x() >= 0 && p2.x() < image.cols && p2.y() >= 0 && p2.y() < image.rows) {
                        p2f = cv::Point2f(p2.x(), p2.y());
                        p2Valid = true;
                    }
                    
                    // 如果至少一个点在图像内，计算射线
                    if (p1Valid || p2Valid) {
                        // 如果两点都不在图像内，我们不画射线
                        if (!p1Valid && !p2Valid) {
                            // 跳过绘制
                        } 
                        else {
                            // 确保有足够的信息来计算方向
                            cv::Point2f direction;
                            cv::Point2f startPoint;
                            
                            if (p1Valid && p2Valid) {
                                // 两点都在图像内，正常计算
                                direction = cv::Point2f(p2f.x - p1f.x, p2f.y - p1f.y);
                                startPoint = p2f;
                            } else if (p2Valid) {
                                // 只有P2在图像内，使用原始方向
                                direction = cv::Point2f(p2.x() - p1.x(), p2.y() - p1.y());
                                startPoint = p2f;
                            } else {
                                // 只有P1在图像内，使用原始方向的相反方向
                                direction = cv::Point2f(p1.x() - p2.x(), p1.y() - p2.y());
                                startPoint = p1f;
                            }
                            
                            float length = sqrt(direction.x * direction.x + direction.y * direction.y);
                            if (length > 0) {
                                direction.x /= length;
                                direction.y /= length;
                                
                                // 延长射线
                                float rayLength = 2.0f * sqrt(image.cols * image.cols + image.rows * image.rows);
                                cv::Point2f rayEnd(startPoint.x + direction.x * rayLength, startPoint.y + direction.y * rayLength);
                                
                                // 计算射线与图像边界的交点
                                cv::Point rayEndInImage;
                                bool rayIntersectsImage = clipLineToImage(startPoint, rayEnd, image.cols, image.rows, rayEndInImage);
                                
                                // 只有当射线与图像相交时才绘制
                                if (rayIntersectsImage) {
                                    // 绘制射线延长部分 (虚线)
                                    drawDashedLine(image, cv::Point(startPoint.x, startPoint.y), rayEndInImage, rayColor, 2, 10, 6);
                                }
                            }
                        }
                    }
                }
            }
            
            // 如果交点在图像内，绘制交点
            if (intersectionInImage) {
                cv::Point cvIntersection(intersectionPoint.x(), intersectionPoint.y());
                cv::circle(image, cvIntersection, 8, intersectionColor, -1);
                cv::circle(image, cvIntersection, 10, cv::Scalar(0, 0, 0), 1); // 黑色边框
                
                // 如果交点在图像内，绘制交点到原始点的虚线连接
                for (int i = 0; i < 4; ++i) {
                    QPoint p = originalClickPoints[i];
                    if (p.x() >= 0 && p.x() < image.cols && p.y() >= 0 && p.y() < image.rows) {
                        cv::Point cvPoint(p.x(), p.y());
                        drawDashedLine(image, cvIntersection, cvPoint, rayColor, 1, 10, 6);
                    }
                }
            }
            
            // 如果有额外点，处理多边形
            if (originalClickPoints.size() >= 6) {
                std::vector<cv::Point> polygonPoints;
                
                // 无论交点是否在图像内，都添加到多边形绘制中
                polygonPoints.push_back(cv::Point(intersectionPoint.x(), intersectionPoint.y()));
                
                // 添加所有额外点到多边形，无论它们是否在图像内
                for (int i = 5; i < originalClickPoints.size(); ++i) {
                    QPoint extraPoint = originalClickPoints[i];
                    cv::Point cvExtraPoint(extraPoint.x(), extraPoint.y());
                    
                    // 无条件添加点到多边形
                    polygonPoints.push_back(cvExtraPoint);
                    
                    // 仅当点在图像内时才绘制点标记
                    if (extraPoint.x() >= 0 && extraPoint.x() < image.cols &&
                        extraPoint.y() >= 0 && extraPoint.y() < image.rows) {
                        cv::circle(image, cvExtraPoint, 6, pointColor, -1);
                        cv::circle(image, cvExtraPoint, 8, cv::Scalar(0, 0, 0), 1); // 黑色边框
                    }
                }
                
                // 绘制多边形边缘，仅绘制图像内的部分
                // 这里需要绘制所有边，即使某些点在图像外
                if (polygonPoints.size() >= 2) { // 至少要有两个点才能画线
                    for (size_t i = 0; i < polygonPoints.size(); i++) {
                        cv::Point current = polygonPoints[i];
                        cv::Point next = polygonPoints[(i + 1) % polygonPoints.size()]; // 循环回到第一个点
                        
                        // 计算线段与图像边界的交点
                        bool shouldDraw = false;
                        cv::Point lineStart, lineEnd;
                        
                        // 两点都在图像内，直接绘制
                        if ((current.x >= 0 && current.x < image.cols && current.y >= 0 && current.y < image.rows) &&
                            (next.x >= 0 && next.x < image.cols && next.y >= 0 && next.y < image.rows)) {
                            lineStart = current;
                            lineEnd = next;
                            shouldDraw = true;
                        }
                        // 一个点在图像内，一个点在图像外，裁剪线段
                        else if ((current.x >= 0 && current.x < image.cols && current.y >= 0 && current.y < image.rows) ||
                                (next.x >= 0 && next.x < image.cols && next.y >= 0 && next.y < image.rows)) {
                            cv::Point insidePoint, outsidePoint;
                            
                            if (current.x >= 0 && current.x < image.cols && current.y >= 0 && current.y < image.rows) {
                                insidePoint = current;
                                outsidePoint = next;
                            } else {
                                insidePoint = next;
                                outsidePoint = current;
                            }
                            
                            cv::Point2f direction(outsidePoint.x - insidePoint.x, outsidePoint.y - insidePoint.y);
                            float length = sqrt(direction.x * direction.x + direction.y * direction.y);
                            if (length > 0) {
                                direction.x /= length;
                                direction.y /= length;
                                
                                float rayLength = 2.0f * sqrt(image.cols * image.cols + image.rows * image.rows);
                                cv::Point2f rayEnd(insidePoint.x + direction.x * rayLength, insidePoint.y + direction.y * rayLength);
                                
                                cv::Point intersectionPoint;
                                if (clipLineToImage(cv::Point2f(insidePoint.x, insidePoint.y), rayEnd, image.cols, image.rows, intersectionPoint)) {
                                    lineStart = insidePoint;
                                    lineEnd = intersectionPoint;
                                    shouldDraw = true;
                                }
                            }
                        }
                        // 两点都在图像外，但线段可能穿过图像
                        else {
                            // 计算当前点到下一个点的线段是否与图像相交
                            cv::Point p1, p2;
                            if (clipLineToImage(cv::Point2f(current.x, current.y), cv::Point2f(next.x, next.y), image.cols, image.rows, p1) &&
                                clipLineToImage(cv::Point2f(next.x, next.y), cv::Point2f(current.x, current.y), image.cols, image.rows, p2)) {
                                lineStart = p1;
                                lineEnd = p2;
                                shouldDraw = true;
                            }
                        }
                        
                        if (shouldDraw) {
                            cv::line(image, lineStart, lineEnd, polygonColor, 2);
                        }
                    }
                }
            }
        }
        
        return; // 已完成MissingArea的绘制，直接返回
    }
    
    // 特殊处理Profile类型
    if (type == MeasurementType::Profile) {
        cv::Scalar color = cv::Scalar(255, 0, 255); // 洋红色
        
        // 绘制点和线
        for (int i = 0; i < originalClickPoints.size(); ++i) {
            QPoint point = originalClickPoints[i];
            // 检查坐标有效性并转换为图像坐标
            cv::Point cvPoint = toCvPoint(point, image.cols, image.rows);
            
            // 绘制点
            cv::circle(image, cvPoint, MARKER_RADIUS, color, -1, cv::LINE_AA);
            cv::circle(image, cvPoint, MARKER_RADIUS + 2, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
        }
        
        // 如果有两个点，绘制连接线
        if (originalClickPoints.size() >= 2) {
            cv::Point p1 = toCvPoint(originalClickPoints[0], image.cols, image.rows);
            cv::Point p2 = toCvPoint(originalClickPoints[1], image.cols, image.rows);
            cv::line(image, p1, p2, color, 2, cv::LINE_AA);
        }
        
        return; // 完成Profile的绘制，直接返回
    }
    
    // 特殊处理Polyline类型
    if (type == MeasurementType::Polyline) {
        cv::Scalar color = cv::Scalar(0, 255, 0); // 绿色
        int thickness = 2;
        
        LOG_DEBUG(QString("绘制临时折线测量，点数: %1").arg(originalClickPoints.size()));
        
        // 绘制所有已添加的点
        for (int i = 0; i < originalClickPoints.size(); ++i) {
            QPoint point = originalClickPoints[i];
            // 检查坐标有效性并转换为图像坐标
            cv::Point cvPoint = toCvPoint(point, image.cols, image.rows);
            
            // 绘制点（绿色）
            cv::circle(image, cvPoint, MARKER_RADIUS, color, -1, cv::LINE_AA);
            cv::circle(image, cvPoint, MARKER_RADIUS + 2, cv::Scalar(0, 0, 0), 1, cv::LINE_AA); // 黑色边框
            
            LOG_DEBUG(QString("绘制折线点 #%1: (%2, %3)").arg(i+1).arg(cvPoint.x).arg(cvPoint.y));
        }
        
        // 绘制连接线（绿色）
        if (originalClickPoints.size() >= 2) {
            for (int i = 0; i < originalClickPoints.size() - 1; ++i) {
                cv::Point p1 = toCvPoint(originalClickPoints[i], image.cols, image.rows);
                cv::Point p2 = toCvPoint(originalClickPoints[i+1], image.cols, image.rows);
                cv::line(image, p1, p2, color, thickness, cv::LINE_AA);
                
                LOG_DEBUG(QString("绘制折线段 %1->%2: (%3,%4)->(%5,%6)")
                         .arg(i+1).arg(i+2)
                         .arg(p1.x).arg(p1.y)
                         .arg(p2.x).arg(p2.y));
            }
        }
        
        // 如果有点的话，计算并显示当前总长度
        if (measurementPoints.size() >= 2) {
            float currentLength = 0.0f;
            for (int i = 1; i < measurementPoints.size(); i++) {
                currentLength += (measurementPoints[i] - measurementPoints[i-1]).length();
            }
            
            // 在最后一个点旁边显示当前长度
            if (!originalClickPoints.isEmpty()) {
                cv::Point lastPoint = toCvPoint(originalClickPoints.last(), image.cols, image.rows);
                cv::Point textPos(lastPoint.x + 15, lastPoint.y - 15);
                
                QString lengthText = QString("当前: %1 mm").arg(currentLength, 0, 'f', 2);
                drawTextWithBackground(image, lengthText, textPos, cv::Scalar(255, 255, 255), 0.5, 1, cv::Scalar(0, 100, 0, 200));
                
                LOG_DEBUG(QString("显示临时折线长度: %1 mm").arg(currentLength, 0, 'f', 2));
            }
        }
        
        return; // 完成Polyline的绘制，直接返回
    }
    
    // 深度测量类型的临时绘制
    if (type == MeasurementType::Depth) {
        cv::Scalar pointColor(255, 0, 255); // 洋红色点

        // 绘制已选择的点
        for (int i = 0; i < originalClickPoints.size(); ++i) {
            QPoint point = originalClickPoints[i];
            // 检查坐标有效性
            if (point.x() >= 0 && point.x() < image.cols &&
                point.y() >= 0 && point.y() < image.rows) {
                cv::Point cvPoint(point.x(), point.y());
                // 绘制点
                cv::circle(image, cvPoint, 10, pointColor, -1); // 点本身 (洋红色)
                cv::circle(image, cvPoint, 12, cv::Scalar(0, 0, 0), 2);    // 黑色边框

                // 添加点标签
                std::string label;
                if (i < 3) {
                    label = "P" + std::to_string(i + 1); // 平面点 P1, P2, P3
                } else {
                    label = "目标"; // 目标点
                }
                cv::putText(image, label,
                           cv::Point(cvPoint.x + 15, cvPoint.y - 10),
                           cv::FONT_HERSHEY_SIMPLEX, 0.5,
                           cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
            }
        }

        // 如果有3个或更多点，绘制平面的边界线
        if (originalClickPoints.size() >= 3) {
            for (int i = 0; i < 3; ++i) {
                QPoint p1 = originalClickPoints[i];
                QPoint p2 = originalClickPoints[(i + 1) % 3];

                if (p1.x() >= 0 && p1.x() < image.cols && p1.y() >= 0 && p1.y() < image.rows &&
                    p2.x() >= 0 && p2.x() < image.cols && p2.y() >= 0 && p2.y() < image.rows) {
                    cv::line(image, cv::Point(p1.x(), p1.y()), cv::Point(p2.x(), p2.y()),
                            cv::Scalar(255, 255, 0), 1, cv::LINE_AA); // 黄色虚线表示平面
                }
            }
        }

        // 注意：在临时绘制阶段不显示深度值，避免重复显示
        return; // 完成深度测量的临时绘制，直接返回
    }

    // 其他测量类型的绘制逻辑

    // 绘制点和标签
    for (int i = 0; i < originalClickPoints.size(); ++i) {
        QPoint point = originalClickPoints[i];
        // 检查坐标有效性
        if (point.x() >= 0 && point.x() < image.cols &&
            point.y() >= 0 && point.y() < image.rows) {
            cv::Point cvPoint(point.x(), point.y());
            // 绘制点
            cv::circle(image, cvPoint, 10, cv::Scalar(0, 255, 0), -1); // 点本身 (绿色)
            cv::circle(image, cvPoint, 12, cv::Scalar(0, 0, 0), 2);    // 黑色边框
        }
    }

    // ... 其他类型的绘制逻辑保持不变 ...
}

void MeasurementRenderer::drawLengthFromClickPoints(cv::Mat& image, const MeasurementObject* measurement)
{
    const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
    if (clickPoints.size() < 2) return;

    // 绘制点
    for (int i = 0; i < clickPoints.size() && i < 2; i++) { // 确保只画前两个点
        QPoint point = clickPoints[i];
        if (point.x() >= 0 && point.x() < image.cols &&
            point.y() >= 0 && point.y() < image.rows) {
            cv::circle(image, cv::Point(point.x(), point.y()), 10, cv::Scalar(0, 255, 0), -1);
            cv::circle(image, cv::Point(point.x(), point.y()), 12, cv::Scalar(0, 0, 0), 2);
            // cv::putText(image, "P" + std::to_string(i + 1),
            //             cv::Point(point.x() + 15, point.y() + 15), cv::FONT_HERSHEY_SIMPLEX,
            //             1.2, cv::Scalar(0, 255, 0), 3); // <-- Remove this part
        }
    }

    // 绘制线和距离标签
    QPoint p1_q = clickPoints[0];
    QPoint p2_q = clickPoints[1];
    // 边界检查 (使用 OpenCV Point)
    cv::Point p1(qBound(0, p1_q.x(), image.cols - 1), qBound(0, p1_q.y(), image.rows - 1));
    cv::Point p2(qBound(0, p2_q.x(), image.cols - 1), qBound(0, p2_q.y(), image.rows - 1));

    cv::line(image, p1, p2, cv::Scalar(0, 255, 0), 2); // 实线

    cv::Point midPoint((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
        drawDistanceLabel(image, midPoint, measurement->getResult());
}

void MeasurementRenderer::drawPointToLineFromClickPoints(cv::Mat& image, const MeasurementObject* measurement)
{
    const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
    if (clickPoints.size() != 3) return;

    // 获取 OpenCV Point 并进行边界检查
    cv::Point p1 = toCvPoint(clickPoints[0], image.cols, image.rows);
    cv::Point p2 = toCvPoint(clickPoints[1], image.cols, image.rows);
    cv::Point p3 = toCvPoint(clickPoints[2], image.cols, image.rows);

    // 定义颜色
    cv::Scalar lineColor(0, 255, 255); // 黄色线段和延长线
    cv::Scalar pointColor = lineColor; // 点也用黄色
    cv::Scalar perpColor(255, 0, 255); // 紫色垂线

    // 1. 绘制点 P1, P2, P3
    drawPoint(image, p1, "P1");
    drawPoint(image, p2, "P2");
    drawPoint(image, p3, "P3");

    // 2. 计算 P3 到 无限直线 P1P2 的投影点 H_true
    cv::Point2f p1f(p1.x, p1.y);
    cv::Point2f p2f(p2.x, p2.y);
    cv::Point2f p3f(p3.x, p3.y);
    cv::Point2f lineVec = p2f - p1f;
    cv::Point2f pointVec = p3f - p1f;
    float lineLenSq = lineVec.dot(lineVec); // 使用 dot 计算平方长度
    cv::Point H_true; // 真实垂足 (整数坐标)
    float t = 0.0f;

    if (lineLenSq < 1e-6f) { // P1 和 P2 重合
        H_true = p1;
        t = 0.0f; // 或者标记为特殊情况
    } else {
        // 计算投影参数 t，不限制范围
        t = pointVec.dot(lineVec) / lineLenSq;
        // 计算真实垂足 H_true
        H_true = cv::Point(static_cast<int>(p1.x + t * lineVec.x),
                           static_cast<int>(p1.y + t * lineVec.y));
    }

    // --- 重新组织绘制顺序 --- 
    
    // A. 绘制垂线 P3-H_true (紫色实线)
    cv::line(image, p3, H_true, perpColor, 2, cv::LINE_AA); 

    // B. 绘制基准线 P1-P2 (黄色实线)
    cv::line(image, p1, p2, lineColor, 2);

    // C. 如果垂足在线段外，绘制从 H_true 到最近端点的虚线 (黄色)
    if (t < 0.0f) {
        // 垂足在 P1 外侧，虚线连接 H_true 到 P1
        // cv::line(image, H_true, p1, lineColor, 2); 
        drawDashedLine(image, H_true, p1, lineColor, 1, 8, 4);
    } else if (t > 1.0f) {
        // 垂足在 P2 外侧，虚线连接 H_true 到 P2
        // cv::line(image, H_true, p2, lineColor, 2);
        drawDashedLine(image, H_true, p2, lineColor, 1, 8, 4);
    }
    // 注意：移除了之前的 drawDashedLine 调用和 else 分支下的 cv::line(p1,p2,...)
    // 因为基准线总是在步骤 B 中绘制

    // D. 绘制距离标签 (放在 P3 和 H_true 连线的中点)
    cv::Point labelPos((p3.x + H_true.x) / 2, (p3.y + H_true.y) / 2);
    drawDistanceLabel(image, labelPos, measurement->getResult());
}

void MeasurementRenderer::drawDepthMeasurementVisuals(
    cv::Mat& image,
    const MeasurementObject* measurement,
    const QPoint& projectionPoint2D)
{
    const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
    if (clickPoints.size() != 4) return;

    cv::Point p1 = toCvPoint(clickPoints[0], image.cols, image.rows);
    cv::Point p2 = toCvPoint(clickPoints[1], image.cols, image.rows);
    cv::Point p3 = toCvPoint(clickPoints[2], image.cols, image.rows);
    cv::Point p4 = toCvPoint(clickPoints[3], image.cols, image.rows); // 目标点
    cv::Point projP = toCvPoint(projectionPoint2D, image.cols, image.rows); // 投影点

    // 绘制四个点
    drawPoint(image, p1, "P1");
    drawPoint(image, p2, "P2");
    drawPoint(image, p3, "P3");
    drawPoint(image, p4, "P4");
    // 可选：绘制投影点
    // cv::circle(image, projP, 8, cv::Scalar(0, 0, 255), -1); // 小红点

    // 绘制平面三角形 (绿色虚线)
    cv::Scalar triangleColor(0, 255, 0); // 绿色
    // cv::line(image, p1, p2, cv::Scalar(150, 150, 150), 1);
    // cv::line(image, p2, p3, cv::Scalar(150, 150, 150), 1);
    // cv::line(image, p3, p1, cv::Scalar(150, 150, 150), 1);
    drawDashedLine(image, p1, p2, triangleColor, 1, 8, 4);
    drawDashedLine(image, p2, p3, triangleColor, 1, 8, 4);
    drawDashedLine(image, p3, p1, triangleColor, 1, 8, 4);

    // --- 移除 P4 到投影点的垂线 ---
    // cv::line(image, p4, projP, cv::Scalar(0, 255, 0), 2); // 绿色实线 (P4 到垂足)
    // --- 结束移除 ---

    // --- 确认移除：从 垂足 projP 到 P1, P2, P3 的虚线 ---
    // drawDashedLine(image, projP, p1, cv::Scalar(0, 255, 255), 1, 8, 4); // 黄色细虚线
    // drawDashedLine(image, projP, p2, cv::Scalar(0, 255, 255), 1, 8, 4);
    // drawDashedLine(image, projP, p3, cv::Scalar(0, 255, 255), 1, 8, 4);
    // --- 结束确认移除 ---

    // 绘制深度标签（只显示一次，放在P4附近）
    cv::Point labelPos = p4 + cv::Point(15, 30); // 调整标签位置到 P4 右下方
    QString resultText = measurement->getResult();
    // 若为深度测量，强制取绝对值（只处理数值部分）
    QRegExp rx("(-?\\d+\\.?\\d*)");
    if (resultText.contains("深度")) {
        int pos = rx.indexIn(resultText);
        if (pos >= 0) {
            double val = rx.cap(1).toDouble();
            val = std::fabs(val);
            resultText.replace(rx, QString::number(val, 'f', 2));
        }
    }
    drawDistanceLabel(image, labelPos, resultText);
}

void MeasurementRenderer::drawPoint(cv::Mat& image, const cv::Point& point, const std::string& label)
{
    cv::circle(image, point, 10, cv::Scalar(0, 255, 0), -1);
    cv::circle(image, point, 12, cv::Scalar(0, 0, 0), 2);
    cv::putText(image, label,
                cv::Point(point.x + 15, point.y + 15), cv::FONT_HERSHEY_SIMPLEX,
                1.2, cv::Scalar(0, 255, 0), 3);
}

void MeasurementRenderer::drawDistanceLabel(cv::Mat& image, const cv::Point& position, const QString& text)
{
    if (text.isEmpty()) return;

    int fontHeight = 30; // 增大字体大小 to 30
    cv::Scalar color(255, 255, 255); // 白色文字 (BGR)
    cv::Scalar bgColor(0, 0, 0, 180); // 半透明黑色背景 (RGBA, 但OpenCV通常只用前3个)
    int thickness = -1; // 文字轮廓粗细 -> 改为 -1 请求填充 (FreeType)
    int lineType = cv::LINE_AA;
    int baseline = 0;
    int padding = 10; // 增加内边距，从5改为10

    // 使用 FreeType 绘制 (如果初始化成功)
    if (ft2) {
        try {
            // 1. 计算文本尺寸
            cv::Size textSize = ft2->getTextSize(text.toStdString(), fontHeight, thickness /* 使用-1 */, &baseline);

            // 2. 计算背景矩形位置和大小
            // 尝试将标签放在原始 position 点的上方
            cv::Point textOrg = position + cv::Point(-textSize.width / 2, -padding - baseline); // 居中于 position 上方
            // 确保背景和文本在图像内
            textOrg.x = std::max(0, textOrg.x);
            textOrg.y = std::max(0, textOrg.y);
            int rectWidth = textSize.width + 4 * padding; // 增加水平内边距，从2*padding改为4*padding
            int rectHeight = textSize.height + 2 * padding;
            int rectX = textOrg.x - padding; // 左边额外留出padding空间
            int rectY = textOrg.y;
            if (rectX + rectWidth > image.cols) rectX = image.cols - rectWidth;
            if (rectY + rectHeight > image.rows) rectY = image.rows - rectHeight;
            rectX = std::max(0, rectX); // 再次确保大于0
            rectY = std::max(0, rectY);

            cv::Rect bgRect(rectX, rectY, rectWidth, rectHeight);

            // 3. 绘制半透明背景
            if (bgRect.width > 0 && bgRect.height > 0) { // 检查矩形有效性
                cv::Mat roi = image(bgRect);
                cv::Mat blackRect = cv::Mat(roi.size(), roi.type(), cv::Scalar(0, 0, 0)); // 全黑矩形
                double alpha = 0.6; // 背景不透明度 (60%)
                cv::addWeighted(roi, 1.0 - alpha, blackRect, alpha, 0.0, roi); // 混合 roi 和 blackRect
            }

            // 4. 绘制白色文本 (不透明)
            cv::Point textPos(bgRect.x + 2 * padding, bgRect.y + textSize.height + padding); // 增加文本左边距
            ft2->putText(image, text.toStdString(), textPos, fontHeight, color, thickness /* 使用-1 */, lineType, true); // color 是白色
            LOG_DEBUG(QString("使用 FreeType 绘制标签: '%1' at (%2,%3) with Semi-Transparent BG").arg(text).arg(textPos.x).arg(textPos.y));

        } catch (const cv::Exception& e) {
            LOG_ERROR(QString("FreeType putText 失败: %1. Text: '%2'").arg(e.what()).arg(text));
            cv::putText(image, text.toStdString(), position, cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255,255,255), 2 /* fallback thickness */);
        } catch (const std::exception& e) {
            LOG_ERROR(QString("绘制标签时发生非 OpenCV 异常: %1. Text: '%2'").arg(e.what()).arg(text));
            cv::putText(image, text.toStdString(), position, cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255,255,255), 2 /* fallback thickness */);
        } catch (...) {
            LOG_ERROR(QString("绘制标签时发生未知异常. Text: '%1'").arg(text));
            cv::putText(image, text.toStdString(), position, cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255,255,255), 2 /* fallback thickness */);
        }
    }
    // Fallback: 如果 FreeType 未初始化或失败
    else {
         LOG_WARNING("FreeType 未初始化或字体加载失败，回退到 cv::putText");
         // Fallback 使用 cv::putText，同样增大字体并添加背景
         double fallbackFontScale = 1.0;
         int fallbackThickness = 2; // Fallback 保持为 2 (通常是实心)
         cv::Size textSize = cv::getTextSize(text.toStdString(), cv::FONT_HERSHEY_SIMPLEX, fallbackFontScale, fallbackThickness, &baseline);

         // --- Add calculation for rectX, rectY etc. in else block --- 
         cv::Point textOrg = position + cv::Point(-textSize.width / 2, -padding - baseline - textSize.height);
         textOrg.x = std::max(0, textOrg.x);
         textOrg.y = std::max(0, textOrg.y);
         int rectWidth = textSize.width + 4 * padding; // 增加水平内边距
         int rectHeight = textSize.height + baseline + 2 * padding;
         int rectX = textOrg.x - padding; // 左边额外留出padding空间
         int rectY = textOrg.y;
         if(rectX + rectWidth > image.cols) rectX = image.cols - rectWidth;
         if(rectY + rectHeight > image.rows) rectY = image.rows - rectHeight;
         rectX = std::max(0, rectX);
         rectY = std::max(0, rectY);

         cv::Rect bgRect(rectX, rectY, rectWidth, rectHeight);

         // 绘制半透明背景
         if (bgRect.width > 0 && bgRect.height > 0) {
             cv::Mat roi = image(bgRect);
             cv::Mat blackRect = cv::Mat(roi.size(), roi.type(), cv::Scalar(0, 0, 0));
             double alpha = 0.6;
             cv::addWeighted(roi, 1.0 - alpha, blackRect, alpha, 0.0, roi);
         }

         // 绘制白色文本
         cv::putText(image, text.toStdString(), cv::Point(bgRect.x + 2 * padding, bgRect.y + textSize.height + padding),
                     cv::FONT_HERSHEY_SIMPLEX, fallbackFontScale, cv::Scalar(255,255,255), fallbackThickness);
    }
}

cv::Point MeasurementRenderer::toCvPoint(const QPoint& qPoint, int maxWidth, int maxHeight)
{
    return cv::Point(qBound(0, qPoint.x(), maxWidth - 1),
                     qBound(0, qPoint.y(), maxHeight - 1));
}

void MeasurementRenderer::drawDashedLine(cv::Mat& image, const cv::Point& p1, const cv::Point& p2,
                                       const cv::Scalar& color, int thickness, int dashLength, int gapLength)
{
    double dist = cv::norm(p1 - p2);
    if (dist < 1e-6 || dashLength <= 0 || gapLength < 0) {
        cv::line(image, p1, p2, color, thickness);
        return;
    }

    double dx = (p2.x - p1.x) / dist;
    double dy = (p2.y - p1.y) / dist;
    double currentDist = 0;
    // cv::Point start = p1; // 不再需要迭代的 start
    bool drawDash = true;

    while (currentDist < dist) {
        double segmentStartDist = currentDist;
        double segmentLength = drawDash ? dashLength : gapLength;
        double segmentEndDist = currentDist + segmentLength;

        // 确保最后一段不会超出 p2
        if (segmentEndDist > dist) {
            segmentEndDist = dist;
            segmentLength = dist - segmentStartDist;
        }

        if (drawDash && segmentLength > 0) {
            // 计算当前虚线段的精确起点和终点 (double)
            double startX = p1.x + dx * segmentStartDist;
            double startY = p1.y + dy * segmentStartDist;
            double endX = p1.x + dx * segmentEndDist;
            double endY = p1.y + dy * segmentEndDist;

            // 使用精确计算的点绘制线段
            cv::line(image, 
                     cv::Point(static_cast<int>(startX), static_cast<int>(startY)), 
                     cv::Point(static_cast<int>(endX), static_cast<int>(endY)), 
                     color, thickness);
        }

        // start = end; // 不再需要
        currentDist = segmentEndDist; // 直接更新 currentDist
        drawDash = !drawDash;
    }
}

// 新添加的函数：计算线段与图像边界的交点
bool MeasurementRenderer::clipLineToImage(const cv::Point2f& start, const cv::Point2f& end, int width, int height, cv::Point& outPoint)
{
    // Cohen-Sutherland线段裁剪算法
    const int INSIDE = 0; // 0000
    const int LEFT = 1;   // 0001
    const int RIGHT = 2;  // 0010
    const int BOTTOM = 4; // 0100
    const int TOP = 8;    // 1000
    
    // 计算点的区域码
    auto computeCode = [width, height](float x, float y) -> int {
        int code = INSIDE;
        if (x < 0) code |= LEFT;
        else if (x >= width) code |= RIGHT;
        if (y < 0) code |= BOTTOM;
        else if (y >= height) code |= TOP;
        return code;
    };
    
    // 初始点的区域码
    int codeStart = computeCode(start.x, start.y);
    int codeEnd = computeCode(end.x, end.y);
    
    // 如果两点都在区域外的同一侧，则直接返回false
    if ((codeStart & codeEnd) != 0) {
        return false;
    }
    
    // 使用本地变量而不是交换原始const参数
    cv::Point2f p0, p1;
    int code0, code1;
    
    // 如果起点在图像内，终点在图像外，调整本地变量
    if (codeStart == INSIDE) {
        p0 = end;
        p1 = start;
        code0 = codeEnd;
        code1 = codeStart;
    } else {
        p0 = start;
        p1 = end;
        code0 = codeStart;
        code1 = codeEnd;
    }
    
    // 线段的参数方程: P(t) = p0 + t * (p1 - p0), t in [0,1]
    float x0 = p0.x;
    float y0 = p0.y;
    float x1 = p1.x;
    float y1 = p1.y;
    
    // 找到与图像边界的交点
    float t = 1.0f; // 初始化为终点
    
    // 检查线段与各边界的交点
    if (code0 & LEFT) { // 左边界
        float t_edge = (0 - x0) / (x1 - x0);
        if (t_edge >= 0 && t_edge < t) {
            float y_intersect = y0 + t_edge * (y1 - y0);
            if (y_intersect >= 0 && y_intersect < height) {
                t = t_edge;
                outPoint = cv::Point(0, static_cast<int>(y_intersect));
            }
        }
    }
    else if (code0 & RIGHT) { // 右边界
        float t_edge = (width - 1 - x0) / (x1 - x0);
        if (t_edge >= 0 && t_edge < t) {
            float y_intersect = y0 + t_edge * (y1 - y0);
            if (y_intersect >= 0 && y_intersect < height) {
                t = t_edge;
                outPoint = cv::Point(width - 1, static_cast<int>(y_intersect));
            }
        }
    }
    
    if (code0 & BOTTOM) { // 下边界
        float t_edge = (0 - y0) / (y1 - y0);
        if (t_edge >= 0 && t_edge < t) {
            float x_intersect = x0 + t_edge * (x1 - x0);
            if (x_intersect >= 0 && x_intersect < width) {
                t = t_edge;
                outPoint = cv::Point(static_cast<int>(x_intersect), 0);
            }
        }
    }
    else if (code0 & TOP) { // 上边界
        float t_edge = (height - 1 - y0) / (y1 - y0);
        if (t_edge >= 0 && t_edge < t) {
            float x_intersect = x0 + t_edge * (x1 - x0);
            if (x_intersect >= 0 && x_intersect < width) {
                t = t_edge;
                outPoint = cv::Point(static_cast<int>(x_intersect), height - 1);
            }
        }
    }
    
    // 如果没有找到交点（t仍然是1.0）且终点不在图像内
    if (t == 1.0f && code1 != INSIDE) {
        return false;
    }
    
    return true;
}

void MeasurementRenderer::drawAreaFromClickPoints(cv::Mat& image, const MeasurementObject* measurement)
{
    const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
    if (clickPoints.size() < 3) return; // Need at least 3 points for an area

    cv::Scalar areaColor(255, 0, 0); // Blue color for area measurements

    // 1. Draw the polygon edges
    for (int i = 0; i < clickPoints.size(); ++i) {
        QPoint p1_q = clickPoints[i];
        QPoint p2_q = clickPoints[(i + 1) % clickPoints.size()]; // Connect last point to first

        // Check bounds and convert
        if (p1_q.x() >= 0 && p1_q.x() < image.cols && p1_q.y() >= 0 && p1_q.y() < image.rows &&
            p2_q.x() >= 0 && p2_q.x() < image.cols && p2_q.y() >= 0 && p2_q.y() < image.rows)
        {
            cv::Point p1 = toCvPoint(p1_q, image.cols, image.rows);
            cv::Point p2 = toCvPoint(p2_q, image.cols, image.rows);
            cv::line(image, p1, p2, areaColor, 2); // Draw blue line segment
        }
    }

    // 2. Draw the points
    for (int i = 0; i < clickPoints.size(); ++i) {
        QPoint point_q = clickPoints[i];
        if (point_q.x() >= 0 && point_q.x() < image.cols && point_q.y() >= 0 && point_q.y() < image.rows) {
             cv::Point point = toCvPoint(point_q, image.cols, image.rows);
             // Draw blue point with black border
             cv::circle(image, point, 10, areaColor, -1);
             cv::circle(image, point, 12, cv::Scalar(0, 0, 0), 2);
        }
    }

    // 3. Draw the area label (place near the first point P1 for simplicity)
    if (!clickPoints.isEmpty()) {
        cv::Point labelPos = toCvPoint(clickPoints[0], image.cols, image.rows);
        labelPos += cv::Point(15, 30); // Offset below and right of P1
        drawDistanceLabel(image, labelPos, measurement->getResult()); // Reuse existing label function
    }
}

// --- 新增函数：绘制折线测量 --- 
void MeasurementRenderer::drawPolylineFromClickPoints(
    cv::Mat& image,
    MeasurementObject* measurement)
{
    if (!measurement || image.empty()) return;

    const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
    if (clickPoints.size() < 2) return; // 折线至少需要两个点

    // 折线测量固定使用绿色
    cv::Scalar color = cv::Scalar(0, 255, 0); // 绿色
    int thickness = measurement->isSelected() ? SELECTED_THICKNESS : DEFAULT_THICKNESS;

    // 绘制连接线
    for (int i = 0; i < clickPoints.size() - 1; ++i) {
        cv::Point p1 = toCvPoint(clickPoints[i], image.cols, image.rows);
        cv::Point p2 = toCvPoint(clickPoints[i+1], image.cols, image.rows);
        cv::line(image, p1, p2, color, thickness, cv::LINE_AA);
    }

    // 绘制标记点
    for (const QPoint& p : clickPoints) {
        cv::Point center = toCvPoint(p, image.cols, image.rows);
        cv::circle(image, center, MARKER_RADIUS, color, -1, cv::LINE_AA);
    }

    // 绘制结果文本 (总长度)
    QString resultText = measurement->getResult();
    if (!resultText.isEmpty()) {
        // 将文本放在最后一个点的旁边
        QPoint lastPoint = clickPoints.last();
        cv::Point textPos = toCvPoint(lastPoint, image.cols, image.rows);
        textPos.x += 10; // 稍微偏移
        textPos.y -= 10;
        
        // 边界检查，防止文本画到图像外
        cv::Size textSize = cv::getTextSize(resultText.toStdString(), cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, nullptr);
        if (textPos.x + textSize.width > image.cols) {
            textPos.x = image.cols - textSize.width - 5;
        }
        if (textPos.y < textSize.height + 5) {
            textPos.y = textSize.height + 5;
        }
        if (textPos.x < 5) {
            textPos.x = 5;
        }

        drawTextWithBackground(image, resultText, textPos, DEFAULT_TEXT_COLOR, TEXT_FONT_SCALE, TEXT_THICKNESS, cv::Scalar(30, 30, 30, 180));
    }
}
// --- 结束新增函数 ---

// --- 新增：绘制轮廓测量线 --- 
void MeasurementRenderer::drawProfileFromClickPoints(
    cv::Mat& image,
    const MeasurementObject* measurement)
{
    if (!measurement || image.empty() || measurement->getOriginalClickPoints().size() != 2) return;

    const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
    QPoint p1_q = clickPoints[0];
    QPoint p2_q = clickPoints[1];

    cv::Scalar color = cv::Scalar(255, 0, 255); // 选择一种颜色，例如洋红
    int thickness = measurement->isSelected() ? SELECTED_THICKNESS : DEFAULT_THICKNESS;

    // 绘制连接线 - 使用toCvPoint确保点在图像范围内
    cv::Point p1 = toCvPoint(p1_q, image.cols, image.rows);
    cv::Point p2 = toCvPoint(p2_q, image.cols, image.rows);
    cv::line(image, p1, p2, color, thickness, cv::LINE_AA);

    // 绘制标记点 - 使用更明显的标记
    cv::circle(image, p1, MARKER_RADIUS, color, -1, cv::LINE_AA);
    cv::circle(image, p2, MARKER_RADIUS, color, -1, cv::LINE_AA);
    // 添加黑色边框使标记更明显
    cv::circle(image, p1, MARKER_RADIUS + 2, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
    cv::circle(image, p2, MARKER_RADIUS + 2, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);

    // 可以选择性地绘制一个简单的标签，比如 "Profile"
    QString resultText = measurement->getResult(); // 例如 "轮廓线已定义"
    if (!resultText.isEmpty()) {
        cv::Point midPoint((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
        midPoint.y -= 10; // 向上偏移一点
        drawTextWithBackground(image, resultText, midPoint, DEFAULT_TEXT_COLOR, TEXT_FONT_SCALE, TEXT_THICKNESS, cv::Scalar(30, 30, 30, 180));
    }
}

// --- 辅助函数：绘制文本带背景 ---
void MeasurementRenderer::drawTextWithBackground(
    cv::Mat& image,
    const QString& text,
    const cv::Point& position,
    const cv::Scalar& textColor,
    double fontScale,
    int thickness,
    const cv::Scalar& bgColor)
{
    if (text.isEmpty()) return;

    int baseline = 0;
    int padding = 5; // 背景矩形的内边距
    cv::Size textSize;
    int fontHeightPixels = 20; // FreeType 需要像素大小，设定一个默认值

    // 将QString转换为UTF-8编码的std::string，这是FreeType通常需要的
    std::string utf8Text = text.toStdString(); 

    // 优先使用 FreeType 计算尺寸和绘制
    if (ft2 && !ft2.empty()) { // 检查 ft2 是否有效
        fontHeightPixels = static_cast<int>(TEXT_FONT_SCALE * 30); // 基于 OpenCV scale 估算像素大小
        textSize = ft2->getTextSize(utf8Text, fontHeightPixels, -1, &baseline);
    } else { // 回退到 OpenCV 标准绘制
        textSize = cv::getTextSize(utf8Text, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
    }

    // 2. 计算背景矩形位置和大小 (逻辑不变)
    cv::Point textOrg = position + cv::Point(-textSize.width / 2, -padding - baseline); // 居中于 position 上方
    textOrg.x = std::max(0, textOrg.x);
    textOrg.y = std::max(0, textOrg.y);
    int rectWidth = textSize.width + 2 * padding;
    int rectHeight = textSize.height + 2 * padding;
    int rectX = textOrg.x;
    int rectY = textOrg.y;
    if (rectX + rectWidth > image.cols) rectX = image.cols - rectWidth;
    if (rectY + rectHeight > image.rows) rectY = image.rows - rectHeight;
    rectX = std::max(0, rectX);
    rectY = std::max(0, rectY);
    cv::Rect bgRect(rectX, rectY, rectWidth, rectHeight);

    // 3. 绘制半透明背景 (逻辑不变)
    if (bgRect.width > 0 && bgRect.height > 0) { 
        cv::Mat roi = image(bgRect);
        cv::Mat blackRect = cv::Mat(roi.size(), roi.type(), bgColor);
        double alpha = 0.6; 
        cv::addWeighted(roi, 1.0 - alpha, blackRect, alpha, 0.0, roi); 
    }

    // 4. 绘制文本 (优先 FreeType)
    cv::Point textPos(bgRect.x + padding, bgRect.y + textSize.height + padding); // 文本左下角
    if (ft2 && !ft2.empty()) { // 检查 ft2 是否有效
        // FreeType putText: 文本颜色在前，需要字体像素大小
        ft2->putText(image, utf8Text, textPos, fontHeightPixels, textColor, -1, cv::LINE_AA, true);
    } else { // 回退到 OpenCV 标准绘制
        cv::putText(image, utf8Text, textPos, cv::FONT_HERSHEY_SIMPLEX, fontScale, textColor, thickness, cv::LINE_AA);
    }
}

// --- Helper Function Implementation ---
/**
 * @brief 将 QColor 转换为 cv::Scalar (BGR 顺序)
 */
cv::Scalar MeasurementRenderer::toCvColor(const QColor& color)
{
    // QColor methods return values in the range 0-255
    return cv::Scalar(color.blue(), color.green(), color.red(), color.alpha()); 
}

// --- 新增函数：绘制补缺测量 --- 
void MeasurementRenderer::drawMissingAreaFromClickPoints(
    cv::Mat& image,
    const MeasurementObject* measurement)
{
    if (!measurement || image.empty()) return;

    const QVector<QPoint>& clickPoints = measurement->getOriginalClickPoints();
    if (clickPoints.size() < 3) return; // 多边形至少需要3个点

    // 定义颜色方案
    cv::Scalar intersectionColor(0, 255, 0); // 绿色 - 交点
    cv::Scalar pointColor(255, 0, 0);        // 红色 - 其他点
    cv::Scalar polygonColor(0, 255, 255);    // 黄色 - 多边形边缘

    // 现在传入的clickPoints包含正确的2D坐标
    // 第一个点是计算得到的2D交点，其余是用户添加的点

    std::vector<cv::Point> polygonPoints;

    // 处理所有多边形点
    for (int i = 0; i < clickPoints.size(); i++) {
        QPoint qPoint = clickPoints[i];
        cv::Point cvPoint(qPoint.x(), qPoint.y());

        // 添加到多边形点集合中
        polygonPoints.push_back(cvPoint);

        // 仅当点在图像内时绘制点
        if (qPoint.x() >= 0 && qPoint.x() < image.cols &&
            qPoint.y() >= 0 && qPoint.y() < image.rows) {

            if (i == 0) {
                // 第一个点是交点，用绿色绘制
                cv::circle(image, cvPoint, 8, intersectionColor, -1);
                cv::circle(image, cvPoint, 10, cv::Scalar(0, 0, 0), 1); // 黑色边框
            } else {
                // 其他点用红色绘制
                cv::circle(image, cvPoint, 6, pointColor, -1);
                cv::circle(image, cvPoint, 8, cv::Scalar(0, 0, 0), 1); // 黑色边框
            }
        }
    }
    
    // 绘制多边形边缘，仅绘制图像内的部分
    // 这里需要绘制所有边，即使某些点在图像外
    if (polygonPoints.size() >= 2) { // 至少要有两个点才能画线
        for (size_t i = 0; i < polygonPoints.size(); i++) {
            cv::Point current = polygonPoints[i];
            cv::Point next = polygonPoints[(i + 1) % polygonPoints.size()]; // 循环回到第一个点
            
            // 计算线段与图像边界的交点
            bool shouldDraw = false;
            cv::Point lineStart, lineEnd;
            
            // 两点都在图像内，直接绘制
            if ((current.x >= 0 && current.x < image.cols && current.y >= 0 && current.y < image.rows) &&
                (next.x >= 0 && next.x < image.cols && next.y >= 0 && next.y < image.rows)) {
                lineStart = current;
                lineEnd = next;
                shouldDraw = true;
            }
            // 一个点在图像内，一个点在图像外，裁剪线段
            else if ((current.x >= 0 && current.x < image.cols && current.y >= 0 && current.y < image.rows) ||
                     (next.x >= 0 && next.x < image.cols && next.y >= 0 && next.y < image.rows)) {
                cv::Point insidePoint, outsidePoint;
                
                if (current.x >= 0 && current.x < image.cols && current.y >= 0 && current.y < image.rows) {
                    insidePoint = current;
                    outsidePoint = next;
                } else {
                    insidePoint = next;
                    outsidePoint = current;
                }
                
                cv::Point2f direction(outsidePoint.x - insidePoint.x, outsidePoint.y - insidePoint.y);
                float length = sqrt(direction.x * direction.x + direction.y * direction.y);
                if (length > 0) {
                    direction.x /= length;
                    direction.y /= length;
                    
                    float rayLength = 2.0f * sqrt(image.cols * image.cols + image.rows * image.rows);
                    cv::Point2f rayEnd(insidePoint.x + direction.x * rayLength, insidePoint.y + direction.y * rayLength);
                    
                    cv::Point intersectionPoint;
                    if (clipLineToImage(cv::Point2f(insidePoint.x, insidePoint.y), rayEnd, image.cols, image.rows, intersectionPoint)) {
                        lineStart = insidePoint;
                        lineEnd = intersectionPoint;
                        shouldDraw = true;
                    }
                }
            }
            // 两点都在图像外，但线段可能穿过图像
            else {
                // 计算当前点到下一个点的线段是否与图像相交
                cv::Point p1, p2;
                if (clipLineToImage(cv::Point2f(current.x, current.y), cv::Point2f(next.x, next.y), image.cols, image.rows, p1) &&
                    clipLineToImage(cv::Point2f(next.x, next.y), cv::Point2f(current.x, current.y), image.cols, image.rows, p2)) {
                    lineStart = p1;
                    lineEnd = p2;
                    shouldDraw = true;
                }
            }
            
            if (shouldDraw) {
                cv::line(image, lineStart, lineEnd, polygonColor, 3);
            }
        }
    }

    // 绘制面积标签
    QString resultText = measurement->getResult();
    if (!resultText.isEmpty()) {
        // 将标签放在多边形中心附近
        cv::Point labelPos;

        if (polygonPoints.size() > 0) {
            // 计算多边形中心点
            cv::Point2f center(0, 0);
            for (const auto& point : polygonPoints) {
                center.x += point.x;
                center.y += point.y;
            }
            center.x /= polygonPoints.size();
            center.y /= polygonPoints.size();

            labelPos = cv::Point(static_cast<int>(center.x), static_cast<int>(center.y));
        } else {
            // 如果没有多边形点，放在图像中心
            labelPos = cv::Point(image.cols / 2, image.rows / 2);
        }

        // 确保标签在图像范围内
        cv::Size textSize = cv::getTextSize(resultText.toStdString(), cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, nullptr);
        if (labelPos.x + textSize.width > image.cols) {
            labelPos.x = image.cols - textSize.width - 5;
        }
        if (labelPos.y < textSize.height + 5) {
            labelPos.y = textSize.height + 5;
        }

        drawTextWithBackground(image, resultText, labelPos, cv::Scalar(255, 255, 255), 0.7, 2, cv::Scalar(0, 0, 0, 200));
    }
}
// --- 结束新增函数 ---

} // namespace Ui
} // namespace App
} // namespace SmartScope 