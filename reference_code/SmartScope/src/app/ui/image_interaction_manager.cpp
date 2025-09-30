#include "app/ui/image_interaction_manager.h"
#include "infrastructure/logging/logger.h"
#include "app/image/image_processor.h"

namespace SmartScope {
namespace App {
namespace Ui {

ImageInteractionManager::ImageInteractionManager(QObject *parent)
    : QObject(parent),
      m_imageLabel(nullptr),
      m_stateManager(nullptr),
      m_measurementManager(nullptr),
      m_measurementRenderer(nullptr),
      m_measurementCalculator(nullptr),
      m_correctionManager(nullptr)
{
    LOG_INFO("创建图像交互管理器");
}

ImageInteractionManager::~ImageInteractionManager()
{
    LOG_INFO("销毁图像交互管理器");
    // 这里不需要删除指针，因为它们是由外部持有并传入的
}

bool ImageInteractionManager::initialize(ClickableImageLabel* imageLabel,
                                        MeasurementStateManager* stateManager,
                                        MeasurementManager* measurementManager,
                                        MeasurementRenderer* measurementRenderer,
                                        SmartScope::App::Measurement::MeasurementCalculator* measurementCalculator,
                                        std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager)
{
    LOG_INFO("初始化图像交互管理器");
    
    if (!imageLabel || !stateManager || !measurementManager || 
        !measurementRenderer || !measurementCalculator || !correctionManager) {
        LOG_ERROR("图像交互管理器初始化失败，有一个或多个必要组件为空");
        return false;
    }
    
    m_imageLabel = imageLabel;
    m_stateManager = stateManager;
    m_measurementManager = measurementManager;
    m_measurementRenderer = measurementRenderer;
    m_measurementCalculator = measurementCalculator;
    m_correctionManager = correctionManager;
    
    // 清空测量点列表
    m_originalClickPoints.clear();
    m_measurementPoints.clear();

    // 初始化缺失面积测量专用变量
    m_lineSegmentPoints.clear();
    m_lineSegmentClickPoints.clear();
    m_polygonPoints.clear();
    m_polygonClickPoints.clear();
    m_hasIntersection = false;
    
    LOG_INFO("图像交互管理器初始化完成");
    return true;
}

void ImageInteractionManager::clearCurrentMeasurementPoints()
{
    m_originalClickPoints.clear();
    m_measurementPoints.clear();

    // 清空缺失面积测量专用变量
    m_lineSegmentPoints.clear();
    m_lineSegmentClickPoints.clear();
    m_polygonPoints.clear();
    m_polygonClickPoints.clear();
    m_hasIntersection = false;
    LOG_INFO("已清空当前测量点");
}

void ImageInteractionManager::setDisplayImage(const cv::Mat& displayImage)
{
    if (displayImage.empty()) {
        LOG_WARNING("尝试设置空图像");
        return;
    }
    
    m_displayImage = displayImage.clone();
    LOG_INFO(QString("已设置显示图像，尺寸：%1x%2").arg(m_displayImage.cols).arg(m_displayImage.rows));
}

cv::Mat ImageInteractionManager::redrawMeasurements(const cv::Mat& baseImage, const QSize& originalImageSize)
{
    // 确保基础图像不为空
    if (baseImage.empty()) {
        LOG_WARNING("基础图像为空，无法重绘测量");
        return cv::Mat();
    }
    
    // 确保测量渲染器有效
    if (!m_measurementRenderer || !m_measurementManager) {
        LOG_WARNING("测量渲染器或测量管理器未初始化，无法绘制测量");
        // 返回原始图像的副本
        return baseImage.clone();
    }
    
    // 调用渲染器绘制测量对象
    cv::Mat resultImage = m_measurementRenderer->drawMeasurements(
        baseImage,
        m_measurementManager->getMeasurements(),
        m_correctionManager,
        originalImageSize);
    
    // 存储处理后的图像
    m_displayImage = resultImage.clone();
    
    return resultImage;
}

void ImageInteractionManager::drawTemporaryMeasurement(cv::Mat& image)
{
    // 确保有临时点需要绘制
    if (m_originalClickPoints.isEmpty() || !m_measurementRenderer) {
        LOG_DEBUG("无临时点需要绘制或渲染器未初始化");
        return;
    }
    
    // 确保有效的图像和渲染器
    if (image.empty()) {
        LOG_WARNING("图像为空，无法绘制临时测量");
        return;
    }
    
    // 获取当前测量类型
    MeasurementType currentType = m_stateManager ? m_stateManager->getActiveMeasurementType() : MeasurementType::Length;
    
    // 调用渲染器绘制临时测量
    m_measurementRenderer->drawTemporaryMeasurement(
        image, 
        m_originalClickPoints, 
        m_measurementPoints, 
        currentType
    );
    
    LOG_DEBUG(QString("已绘制临时测量，类型: %1, 点数: %2")
             .arg(static_cast<int>(currentType))
             .arg(m_originalClickPoints.size()));
}

void ImageInteractionManager::handleImageClick(int imageX, int imageY, const QPoint& labelPoint, 
                                            const cv::Mat& depthMap,
                                            const std::vector<cv::Point2i>& pointCloudPixelCoords,
                                            std::function<QVector3D(int, int, int)> findNearestPointFunc)
{
    // 检查是否处于添加模式
    if (!m_stateManager || m_stateManager->getMeasurementMode() != MeasurementMode::Add) {
        LOG_INFO("非添加模式，忽略图像点击");
        return;
    }
    
    // 获取当前测量类型
    MeasurementType currentType = m_stateManager->getActiveMeasurementType();
    LOG_INFO(QString("图像点击 - 测量类型: %1, 坐标: (%2, %3)")
            .arg(static_cast<int>(currentType)).arg(imageX).arg(imageY));
    
    // 检查深度图是否可用
    if (depthMap.empty()) {
        LOG_ERROR("深度图不可用，无法进行测量");
        emit showToastMessage("深度图不可用，请先生成深度图", 2000);
        return;
    }
    
    // 保存当前深度图到测量计算器，以便后续使用
    if (m_measurementCalculator) {
        m_measurementCalculator->setLatestDepthMap(depthMap);
    }
    
    // 保存当前点击的2D坐标
    QPoint currentClickPoint(imageX, imageY);
    
    // 调整点击坐标以匹配深度图尺寸
    int adjustedX = imageX;
    int adjustedY = imageY;

    LOG_DEBUG(QString("坐标转换 - 显示图像尺寸: %1x%2, 深度图尺寸: %3x%4, 原始点击: (%5,%6)")
             .arg(m_displayImage.cols).arg(m_displayImage.rows)
             .arg(depthMap.cols).arg(depthMap.rows)
             .arg(imageX).arg(imageY));

    // 如果显示图像和深度图尺寸不一致，需要进行坐标缩放
    if (m_displayImage.cols != depthMap.cols || m_displayImage.rows != depthMap.rows) {
        float scaleX = (float)depthMap.cols / m_displayImage.cols;
        float scaleY = (float)depthMap.rows / m_displayImage.rows;
        adjustedX = static_cast<int>(imageX * scaleX);
        adjustedY = static_cast<int>(imageY * scaleY);
        adjustedX = qBound(0, adjustedX, depthMap.cols - 1);
        adjustedY = qBound(0, adjustedY, depthMap.rows - 1);

        LOG_DEBUG(QString("坐标缩放 - 缩放比例: %1x%2, 调整后坐标: (%3,%4)")
                 .arg(scaleX, 0, 'f', 3).arg(scaleY, 0, 'f', 3)
                 .arg(adjustedX).arg(adjustedY));
    } else {
        LOG_DEBUG("显示图像与深度图尺寸一致，无需坐标缩放");
    }
    
    // 计算3D点
    QVector3D pcPointMeters;
    bool pointCalculated = false;
    
    // 尝试从点云中找到点
    if (findNearestPointFunc) {
        QVector3D cloudPoint = findNearestPointFunc(adjustedX, adjustedY, 10);
        if (cloudPoint != QVector3D(0, 0, 0)) {
            pcPointMeters = cloudPoint;
            pointCalculated = true;
            LOG_INFO(QString("使用点云中找到的点: (%1, %2, %3)米")
                   .arg(pcPointMeters.x(), 0, 'f', 5)
                   .arg(pcPointMeters.y(), 0, 'f', 5)
                   .arg(pcPointMeters.z(), 0, 'f', 5));
        }
    }
    
    // 如果没找到点，尝试从深度图计算
            if (!pointCalculated && m_measurementCalculator && m_correctionManager) {
            // 若存在中心裁剪ROI、以及rectify阶段的ROI，统一修正相机内参的主点到当前显示坐标系
            // 优先使用校正后的投影矩阵 P1 推导内参参数，其次回退到原始K
            cv::Mat K;
            bool usedP1 = false;
            try {
                auto stereoHelper = m_correctionManager->getStereoCalibrationHelper();
                cv::Mat P1 = stereoHelper ? stereoHelper->getP1() : cv::Mat();
                if (!P1.empty() && P1.rows == 3 && P1.cols == 4) {
                    K = cv::Mat::eye(3, 3, CV_64F);
                    K.at<double>(0, 0) = P1.at<double>(0, 0);
                    K.at<double>(1, 1) = P1.at<double>(1, 1);
                    K.at<double>(0, 2) = P1.at<double>(0, 2);
                    K.at<double>(1, 2) = P1.at<double>(1, 2);
                    usedP1 = true;
                    LOG_INFO(QString("使用P1构造内参: fx=%1, fy=%2, cx=%3, cy=%4")
                             .arg(K.at<double>(0, 0), 0, 'f', 2)
                             .arg(K.at<double>(1, 1), 0, 'f', 2)
                             .arg(K.at<double>(0, 2), 0, 'f', 2)
                             .arg(K.at<double>(1, 2), 0, 'f', 2));
                }
            } catch (...) {
                // 忽略，回退到原始K
            }
            if (!usedP1) {
                auto stereoHelper = m_correctionManager->getStereoCalibrationHelper();
                K = stereoHelper ? stereoHelper->getCameraMatrixLeft().clone() : cv::Mat();
                LOG_WARNING("P1不可用，回退使用原始K");
            }

            // 叠加rectify阶段ROI的主点偏移
            try {
                auto stereoHelper = m_correctionManager->getStereoCalibrationHelper();
                cv::Rect roi1 = stereoHelper ? stereoHelper->getRoi1() : cv::Rect();
                if (roi1.width > 0 && roi1.height > 0) {
                    K.at<double>(0, 2) -= roi1.x;
                    K.at<double>(1, 2) -= roi1.y;
                    LOG_INFO(QString("应用rectify ROI偏移: roi1=(%1,%2), 新cx=%3, cy=%4")
                             .arg(roi1.x).arg(roi1.y)
                             .arg(K.at<double>(0, 2), 0, 'f', 2)
                             .arg(K.at<double>(1, 2), 0, 'f', 2));
                }
            } catch (...) {
                // 忽略ROI偏移失败
            }

            // 叠加3:4裁剪ROI的主点偏移
            if (m_cropROI.width > 0 && m_cropROI.height > 0) {
                K.at<double>(0, 2) -= m_cropROI.x; // cx' = cx - x0
                K.at<double>(1, 2) -= m_cropROI.y; // cy' = cy - y0（通常y0=0）
                LOG_INFO(QString("应用3:4裁剪偏移: crop=(%1,%2), 最终cx=%3, cy=%4")
                         .arg(m_cropROI.x).arg(m_cropROI.y)
                         .arg(K.at<double>(0, 2), 0, 'f', 2)
                         .arg(K.at<double>(1, 2), 0, 'f', 2));
            }


        QSize originalImageSize(m_displayImage.cols, m_displayImage.rows);
        auto stereoHelper = m_correctionManager->getStereoCalibrationHelper();
        cv::Mat fallbackK = stereoHelper ? stereoHelper->getCameraMatrixLeft() : cv::Mat();
        QVector3D pointCloudCoordsMm = m_measurementCalculator->imageToPointCloudCoordinates(
            adjustedX, adjustedY, depthMap, K.empty() ? fallbackK : K, originalImageSize);
        if (pointCloudCoordsMm.z() < 0 && !qIsNaN(pointCloudCoordsMm.z())) {
            pcPointMeters = pointCloudCoordsMm / 1000.0f; // 转为米
            pointCalculated = true;
            LOG_INFO(QString("从深度图计算3D坐标(米): (%1, %2, %3)")
                   .arg(pcPointMeters.x(), 0, 'f', 5)
                   .arg(pcPointMeters.y(), 0, 'f', 5)
                   .arg(pcPointMeters.z(), 0, 'f', 5));
        }
    }
    
    // 如果无法获取有效的3D点，提示错误并返回
    if (!pointCalculated) {
        LOG_ERROR(QString("无法获取有效的3D坐标，忽略点击 (%1, %2)").arg(imageX).arg(imageY));
        emit showToastMessage("点击位置无有效深度数据，请选择其他位置", 2000);
        return;
    }
    
    // 根据测量类型处理点击事件
    switch (currentType) {
        case MeasurementType::Length:
            handleLengthMeasurement(pcPointMeters, currentClickPoint);
            break;
        case MeasurementType::PointToLine:
            handlePointToLineMeasurement(pcPointMeters, currentClickPoint);
            break;
        case MeasurementType::Depth:
            handleDepthMeasurement(pcPointMeters, currentClickPoint);
            break;
        case MeasurementType::Profile:
            handleProfileMeasurement(pcPointMeters, currentClickPoint);
            break;
        case MeasurementType::Area:
            handleAreaMeasurement(pcPointMeters, currentClickPoint);
            break;
        case MeasurementType::Polyline:
            handlePolylineMeasurement(pcPointMeters, currentClickPoint);
            break;
        case MeasurementType::MissingArea:
            handleMissingAreaMeasurement(pcPointMeters, currentClickPoint);
            break;
        default:
            LOG_WARNING(QString("不支持的测量类型: %1").arg(static_cast<int>(currentType)));
            break;
    }
}

void ImageInteractionManager::handleLengthMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint)
{
    // 将有效点添加到临时列表
    m_originalClickPoints.append(currentClickPoint);
    m_measurementPoints.append(pcPointMeters * 1000.0f); // 转为毫米
    
    LOG_INFO(QString("长度测量：添加点 #%1 (%2, %3, %4)mm")
            .arg(m_measurementPoints.size())
            .arg(m_measurementPoints.last().x(), 0, 'f', 2)
            .arg(m_measurementPoints.last().y(), 0, 'f', 2)
            .arg(m_measurementPoints.last().z(), 0, 'f', 2));
    
    // 第一个点，只绘制标记
    if (m_measurementPoints.size() == 1) {
        emit updateUI();  // 触发UI更新，绘制第一个点
    }
    // 第二个点，完成测量
    else if (m_measurementPoints.size() == 2) {
        LOG_INFO("长度测量：完成");
        
        // 计算两点之间的距离
        float distance = (m_measurementPoints[1] - m_measurementPoints[0]).length(); // 毫米
        
        // 创建测量对象
        MeasurementObject* measurement = new MeasurementObject();
        measurement->setType(MeasurementType::Length);
        measurement->setPoints(m_measurementPoints);
        measurement->setOriginalClickPoints(m_originalClickPoints);
        measurement->setResult(QString("%1 mm").arg(distance, 0, 'f', 2));
        
        // 发送测量完成信号
        emit measurementCompleted(measurement);
        
        // 清空临时点
        m_originalClickPoints.clear();
        m_measurementPoints.clear();
        
        // 重置状态
        if (m_stateManager) {
            m_stateManager->setMeasurementMode(MeasurementMode::View);
        }
    }
}

void ImageInteractionManager::handlePointToLineMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint)
{
    // 将有效点添加到临时列表
    m_originalClickPoints.append(currentClickPoint);
    m_measurementPoints.append(pcPointMeters * 1000.0f); // 转为毫米
    
    LOG_INFO(QString("点到线测量：添加点 #%1 (%2, %3, %4)mm")
            .arg(m_measurementPoints.size())
            .arg(m_measurementPoints.last().x(), 0, 'f', 2)
            .arg(m_measurementPoints.last().y(), 0, 'f', 2)
            .arg(m_measurementPoints.last().z(), 0, 'f', 2));
    
    // 根据点数处理
    if (m_measurementPoints.size() == 1) {
        // 第一个点，只绘制标记，提示用户选择第二个点
        emit showToastMessage("请选择线的第二个端点", 2000);
        emit updateUI();
    }
    else if (m_measurementPoints.size() == 2) {
        // 第二个点，绘制线段，提示用户选择测量点
        emit showToastMessage("请选择要测量距离的点", 2000);
        emit updateUI();
    }
    else if (m_measurementPoints.size() == 3) {
        LOG_INFO("点到线测量：完成");
        
        // 提取三个点
        QVector3D p1 = m_measurementPoints[0];
        QVector3D p2 = m_measurementPoints[1];
        QVector3D p3 = m_measurementPoints[2];
        
        // 计算点到线的距离
        QVector3D lineVec = p2 - p1;
        QVector3D pointVec = p3 - p1;
        float lineLenSq = lineVec.lengthSquared();
        float distance = 0.0f;
        
        if (lineLenSq < 1e-6f) {
            // 如果线的两个端点重合，计算点到点的距离
            distance = (p3 - p1).length();
            LOG_WARNING("点到线测量：线的两个端点重合，计算为点到点距离");
        } else {
            // 计算点在线上的投影参数
            float t = QVector3D::dotProduct(pointVec, lineVec) / lineLenSq;
            
            // 根据投影参数计算距离
            if (t < 0.0f) {
                // 投影点在线段外部(P1之前)，计算到P1的距离
                distance = (p3 - p1).length();
            } else if (t > 1.0f) {
                // 投影点在线段外部(P2之后)，计算到P2的距离
                distance = (p3 - p2).length();
            } else {
                // 投影点在线段上，计算到投影点的距离
                QVector3D projectionPoint = p1 + t * lineVec;
                distance = (p3 - projectionPoint).length();
            }
        }
        
        // 创建测量对象
        MeasurementObject* measurement = new MeasurementObject();
        measurement->setType(MeasurementType::PointToLine);
        measurement->setPoints(m_measurementPoints);
        measurement->setOriginalClickPoints(m_originalClickPoints);
        measurement->setResult(QString("%1 mm").arg(distance, 0, 'f', 2));
        
        // 发送测量完成信号
        emit measurementCompleted(measurement);
        
        // 清空临时点
        m_originalClickPoints.clear();
        m_measurementPoints.clear();
        
        // 重置状态
        if (m_stateManager) {
            m_stateManager->setMeasurementMode(MeasurementMode::View);
        }
    }
}

void ImageInteractionManager::handleDepthMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint)
{
    // 将有效点添加到临时列表
    m_originalClickPoints.append(currentClickPoint);
    m_measurementPoints.append(pcPointMeters * 1000.0f); // 转为毫米
    
    LOG_INFO(QString("深度测量：添加点 #%1 (%2, %3, %4)mm")
            .arg(m_measurementPoints.size())
            .arg(m_measurementPoints.last().x(), 0, 'f', 2)
            .arg(m_measurementPoints.last().y(), 0, 'f', 2)
            .arg(m_measurementPoints.last().z(), 0, 'f', 2));
    
    // 根据点数处理
    if (m_measurementPoints.size() == 1) {
        // 第一个点，提示用户选择第二个点
        emit showToastMessage("请选择平面第二个点", 2000);
        emit updateUI();
    }
    else if (m_measurementPoints.size() == 2) {
        // 第二个点，提示用户选择第三个点
        emit showToastMessage("请选择平面第三个点", 2000);
        emit updateUI();
    }
    else if (m_measurementPoints.size() == 3) {
        // 第三个点，提示用户选择测量点
        emit showToastMessage("请选择要测量距离的点", 2000);
        emit updateUI();
    }
    else if (m_measurementPoints.size() == 4) {
        LOG_INFO("深度测量：完成");
        
        // 提取四个点
        QVector3D p1 = m_measurementPoints[0]; // 平面点1
        QVector3D p2 = m_measurementPoints[1]; // 平面点2
        QVector3D p3 = m_measurementPoints[2]; // 平面点3
        QVector3D p4 = m_measurementPoints[3]; // 测量点
        
        // 计算平面法向量
        QVector3D v1 = p2 - p1;
        QVector3D v2 = p3 - p1;
        QVector3D normal = QVector3D::crossProduct(v1, v2);
        float normalLength = normal.length();
        
        // 检查三点是否共线
        if (normalLength < 1e-6f) {
            LOG_ERROR("定义平面的三个点共线，无法计算点到面距离");
            emit showToastMessage("定义的平面点共线，请重新选择", 3000);
            // 清空这次无效的尝试
            m_originalClickPoints.clear();
            m_measurementPoints.clear();
            emit updateUI();
            return;
        }
        
        // 单位化法向量
        normal.normalize();
        float A = normal.x();
        float B = normal.y();
        float C = normal.z();
        // 计算平面方程常数项 D
        float D = -QVector3D::dotProduct(normal, p1);
        
        // 计算点到平面的距离
        float dotProduct = A * p4.x() + B * p4.y() + C * p4.z() + D;
        float distance = std::fabs(dotProduct); // 深度测量始终使用绝对值

        // 深度测量不需要考虑符号，始终显示正值
        // 移除符号判断逻辑，确保深度值始终为正
        
        LOG_INFO(QString("点到面距离计算结果: %1 mm").arg(distance, 0, 'f', 2));
        
        // 创建测量对象
        MeasurementObject* measurement = new MeasurementObject();
        measurement->setType(MeasurementType::Depth);
        measurement->setPoints(m_measurementPoints);
        measurement->setOriginalClickPoints(m_originalClickPoints);
        measurement->setResult(QString("深度: %1 mm").arg(distance, 0, 'f', 2));
        
        // 发送测量完成信号
        emit measurementCompleted(measurement);
        
        // 清空临时点
        m_originalClickPoints.clear();
        m_measurementPoints.clear();
        
        // 重置状态
        if (m_stateManager) {
            m_stateManager->setMeasurementMode(MeasurementMode::View);
        }
    }
}

void ImageInteractionManager::handleProfileMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint)
{
    // 将有效点添加到临时列表
    m_originalClickPoints.append(currentClickPoint);
    m_measurementPoints.append(pcPointMeters * 1000.0f); // 转为毫米
    
    LOG_INFO(QString("剖面测量：添加点 #%1 (%2, %3, %4)mm")
            .arg(m_measurementPoints.size())
            .arg(m_measurementPoints.last().x(), 0, 'f', 2)
            .arg(m_measurementPoints.last().y(), 0, 'f', 2)
            .arg(m_measurementPoints.last().z(), 0, 'f', 2));
    
    // 第一个点，只绘制标记
    if (m_measurementPoints.size() == 1) {
        emit showToastMessage("请选择剖面线的终点", 2000);
        emit updateUI();
    }
    // 第二个点，完成测量
    else if (m_measurementPoints.size() == 2) {
        LOG_INFO("剖面测量：完成");
        
        // 创建测量对象
        MeasurementObject* measurement = new MeasurementObject();
        measurement->setType(MeasurementType::Profile);
        measurement->setPoints(m_measurementPoints);
        measurement->setOriginalClickPoints(m_originalClickPoints);
        
        // 计算剖面数据
        QVector<QPointF> profileData;
        if (m_measurementCalculator && m_correctionManager) {
            // 获取当前深度图
            cv::Mat depthMap = m_measurementCalculator->getLatestDepthMap();
            
            // 记录深度图基本信息
            if (!depthMap.empty()) {
                LOG_INFO(QString("剖面测量：深度图信息 - 尺寸: %1x%2, 类型: %3")
                        .arg(depthMap.cols).arg(depthMap.rows).arg(depthMap.type()));
                
                // 重要：使用显示图像尺寸作为原始图像尺寸
                // 这应该是校正裁剪后的图像尺寸，与深度图尺寸一致
                QSize originalImageSize;
                if (!m_displayImage.empty()) {
                    originalImageSize = QSize(m_displayImage.cols, m_displayImage.rows);
                    LOG_INFO(QString("剖面测量：使用显示图像尺寸: %1x%2")
                            .arg(originalImageSize.width()).arg(originalImageSize.height()));
                } else {
                    LOG_WARNING("剖面测量：显示图像为空，使用深度图尺寸");
                    originalImageSize = QSize(depthMap.cols, depthMap.rows);
                }

                // 验证尺寸一致性
                if (originalImageSize.width() != depthMap.cols || originalImageSize.height() != depthMap.rows) {
                    LOG_WARNING(QString("剖面测量：图像尺寸不一致 - 显示图像: %1x%2, 深度图: %3x%4")
                               .arg(originalImageSize.width()).arg(originalImageSize.height())
                               .arg(depthMap.cols).arg(depthMap.rows));
                }
                
                // 调用计算器计算剖面数据
                profileData = m_measurementCalculator->calculateProfileData(
                    measurement,
                    depthMap,
                    originalImageSize,
                    m_correctionManager
                );
                
                LOG_INFO(QString("剖面测量：计算完成，获得 %1 个剖面数据点").arg(profileData.size()));

                // 验证剖面数据的有效性
                bool dataValid = true;
                if (profileData.isEmpty()) {
                    dataValid = false;
                    LOG_WARNING("剖面测量：计算结果为空");
                } else {
                    // 检查数据的合理性
                    int validPoints = 0;
                    for (const QPointF& point : profileData) {
                        if (!qIsNaN(point.x()) && !qIsNaN(point.y()) &&
                            !qIsInf(point.x()) && !qIsInf(point.y())) {
                            validPoints++;
                        }
                    }
                    if (validPoints < profileData.size() * 0.5) {
                        dataValid = false;
                        LOG_WARNING(QString("剖面测量：有效数据点过少 %1/%2").arg(validPoints).arg(profileData.size()));
                    }
                }

                // 将剖面数据保存到测量对象中
                if (dataValid) {
                    measurement->setProfileData(profileData);
                    LOG_INFO(QString("剖面测量：成功保存 %1 个有效剖面数据点").arg(profileData.size()));
                } else {
                    LOG_ERROR("剖面测量：数据无效，不保存到测量对象");
                }
                
                // 如果剖面数据为空或点数很少，使用简单的线性插值生成基本剖面
                if (profileData.size() < 2) {
                    LOG_WARNING("剖面测量：计算的剖面数据不足，生成简单的线性插值数据");
                    
                    // 使用起点和终点生成简单的表面起伏数据
                    QVector3D startPoint = m_measurementPoints[0];
                    QVector3D endPoint = m_measurementPoints[1];
                    float totalLength = (endPoint - startPoint).length();
                    
                    // 生成线性基准的起伏数据
                    const int numSamples = 50;
                    QVector<QPointF> interpolatedData;
                    
                    for (int i = 0; i < numSamples; ++i) {
                        float t = i / static_cast<float>(numSamples - 1);
                        
                        // 修复：使用正确的沿剖面线距离计算
                        float distance = t * totalLength;
                        
                        // 线性插值得到当前点
                        QVector3D currentPoint = startPoint + (endPoint - startPoint) * t;
                        
                        // 计算相对于线性基准的高程偏差
                        float baselineZ = startPoint.z() + t * (endPoint.z() - startPoint.z());
                        float elevation = currentPoint.z() - baselineZ; // 线性情况下应该接近0
                        
                        interpolatedData.append(QPointF(distance, elevation));
                    }
                    
                    LOG_INFO(QString("剖面测量：生成了 %1 个线性插值数据点（基于起终点）")
                            .arg(interpolatedData.size()));
                    
                    // 使用插值数据
                    measurement->setProfileData(interpolatedData);
                    profileData = interpolatedData;
                }
                
                // 正确的剖面测量结果：计算表面起伏变化
                double minElevation = std::numeric_limits<double>::max();
                double maxElevation = std::numeric_limits<double>::lowest();
                
                // 如果有剖面数据，分析高程起伏的范围
                if (!profileData.isEmpty()) {
                    for (const QPointF& p : profileData) {
                        double elevation = p.y(); // 现在profileData中的Y值是高程偏差
                        if (elevation < minElevation) minElevation = elevation;
                        if (elevation > maxElevation) maxElevation = elevation;
                    }
                    
                    // 计算表面起伏变化 - 这是剖面测量的正确结果
                    double elevationRange = maxElevation - minElevation;
                    
                    LOG_INFO(QString("剖面测量计算结果(表面起伏): 最小高程=%1mm, 最大高程=%2mm, 起伏范围=%3mm, 数据点数=%4")
                           .arg(minElevation, 0, 'f', 3)
                           .arg(maxElevation, 0, 'f', 3)
                           .arg(elevationRange, 0, 'f', 3)
                           .arg(profileData.size()));
                    
                    // 输出剖面首尾点信息
                    if (profileData.size() >= 2) {
                        LOG_INFO(QString("剖面首点(起伏): (%1, %2), 尾点(起伏): (%3, %4)")
                                .arg(profileData.first().x(), 0, 'f', 3)
                                .arg(profileData.first().y(), 0, 'f', 3)
                                .arg(profileData.last().x(), 0, 'f', 3)
                                .arg(profileData.last().y(), 0, 'f', 3));
                    }
                    
                    // 使用表面起伏范围作为测量结果
                    if (elevationRange > 0.01) {
                        measurement->setResult(QString("起伏: %1 mm").arg(elevationRange, 0, 'f', 2));
                    } else {
                        measurement->setResult("表面平坦，起伏<0.01mm");
                    }
                } else {
                    LOG_WARNING("剖面测量：计算结果为空，无剖面数据");
                    measurement->setResult("无剖面数据");
                }
            } else {
                LOG_ERROR("剖面测量：深度图为空，无法计算剖面数据");
                measurement->setResult("深度图不可用");
                
                // 使用起点和终点生成简单的表面起伏数据
                QVector3D startPoint = m_measurementPoints[0];
                QVector3D endPoint = m_measurementPoints[1];
                float totalLength = (endPoint - startPoint).length();
                
                // 对于无深度图的情况，假设沿线为直线，起伏为0
                const int numSamples = 50;
                QVector<QPointF> interpolatedData;
                
                for (int i = 0; i < numSamples; ++i) {
                    float t = i / static_cast<float>(numSamples - 1);
                    float distance = t * totalLength;
                    
                    // 线性插值得到当前点
                    QVector3D currentPoint = startPoint + (endPoint - startPoint) * t;
                    
                    // 计算相对于线性基准的高程偏差（线性情况下应该为0）
                    float baselineZ = startPoint.z() + t * (endPoint.z() - startPoint.z());
                    float elevation = currentPoint.z() - baselineZ; // 应该接近0
                    
                    interpolatedData.append(QPointF(distance, elevation));
                }
                
                LOG_INFO(QString("剖面测量：深度图为空，生成了 %1 个线性插值数据点（无起伏）")
                        .arg(interpolatedData.size()));
                
                // 使用插值数据
                measurement->setProfileData(interpolatedData);
            }
        } else {
            LOG_ERROR("测量计算器或校准助手为空，无法计算剖面数据");
            measurement->setResult("计算器错误");
        }
        
        // 发送测量完成信号
        emit measurementCompleted(measurement);
        
        // 清空临时点
        m_originalClickPoints.clear();
        m_measurementPoints.clear();
        
        // 重置状态
        if (m_stateManager) {
            m_stateManager->setMeasurementMode(MeasurementMode::View);
        }
    }
}

void ImageInteractionManager::handleAreaMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint)
{
    // 将有效点添加到临时列表
    m_originalClickPoints.append(currentClickPoint);
    m_measurementPoints.append(pcPointMeters * 1000.0f); // 转为毫米
    
    LOG_INFO(QString("面积测量：添加点 #%1 (%2, %3, %4)mm")
            .arg(m_measurementPoints.size())
            .arg(m_measurementPoints.last().x(), 0, 'f', 2)
            .arg(m_measurementPoints.last().y(), 0, 'f', 2)
            .arg(m_measurementPoints.last().z(), 0, 'f', 2));
    
    // 闭合检测阈值 (毫米)
    const float CLOSING_THRESHOLD_MM = 5.0f;
    
    // 检查是否可以闭合 (至少已有3个点 + 当前点)
    if (m_measurementPoints.size() >= 4) {
        QVector3D firstPoint3D_mm = m_measurementPoints.first();
        QVector3D currentPoint3D_mm = m_measurementPoints.last();
        float distanceToFirst = (currentPoint3D_mm - firstPoint3D_mm).length();
        
        LOG_INFO(QString("面积测量 - 检查闭合：当前点到第一个点的距离 %1 mm (阈值 %2 mm)")
                .arg(distanceToFirst, 0, 'f', 2).arg(CLOSING_THRESHOLD_MM));
        
        // 如果当前点与第一个点足够接近，完成闭合
        if (distanceToFirst < CLOSING_THRESHOLD_MM) {
            LOG_INFO("面积测量：检测到闭合，完成测量");
            
            // 移除最后一个点（即闭合点，不需要加入顶点列表）
            m_measurementPoints.removeLast();
            m_originalClickPoints.removeLast();
            
            // 至少需要3个顶点才能计算面积
            if (m_measurementPoints.size() >= 3) {
                // 创建测量对象
                MeasurementObject* measurement = new MeasurementObject();
                measurement->setType(MeasurementType::Area);
                measurement->setPoints(m_measurementPoints);
                measurement->setOriginalClickPoints(m_originalClickPoints);
                
                // 计算面积
                if (m_measurementCalculator) {
                    m_measurementCalculator->calculateMeasurementResult(measurement);
                    LOG_INFO(QString("面积测量完成，结果：%1").arg(measurement->getResult()));
                } else {
                    measurement->setResult("计算器错误");
                    LOG_ERROR("测量计算器为空，无法计算面积");
                }
                
                // 发送测量完成信号
                emit measurementCompleted(measurement);
                emit showToastMessage(QString("面积测量完成: %1").arg(measurement->getResult()), 3000);
            } else {
                LOG_WARNING("面积测量：闭合时顶点数少于3，无法计算面积");
                emit showToastMessage("闭合时顶点数少于3", 2000);
            }
            
            // 清空临时点
            m_originalClickPoints.clear();
            m_measurementPoints.clear();
            
            // 重置状态
            if (m_stateManager) {
                m_stateManager->setMeasurementMode(MeasurementMode::View);
            }
            
            return;
        }
    }
    
    // 未闭合或点数不足，继续添加点
    emit showToastMessage("选择下一个顶点，或点击第一个点闭合区域", 2000);
    emit updateUI();
}

void ImageInteractionManager::handlePolylineMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint)
{
    // 将有效点添加到临时列表
    m_originalClickPoints.append(currentClickPoint);
    m_measurementPoints.append(pcPointMeters * 1000.0f); // 转为毫米
    
    LOG_INFO(QString("折线测量：添加点 #%1 (%2, %3, %4)mm")
            .arg(m_measurementPoints.size())
            .arg(m_measurementPoints.last().x(), 0, 'f', 2)
            .arg(m_measurementPoints.last().y(), 0, 'f', 2)
            .arg(m_measurementPoints.last().z(), 0, 'f', 2));
    
    // 计算当前的总长度（用于实时显示）
    float currentLength = 0.0f;
            if (m_measurementPoints.size() >= 2) {
                for (int i = 1; i < m_measurementPoints.size(); i++) {
            currentLength += (m_measurementPoints[i] - m_measurementPoints[i-1]).length();
        }
                }
                
    // 显示当前状态和提示
    if (m_measurementPoints.size() == 1) {
        emit showToastMessage("已添加第1个点，请继续选择下一个点", 2000);
    } else if (m_measurementPoints.size() == 2) {
        emit showToastMessage(QString("已添加第2个点，当前长度: %1 mm，继续添加点或点击\"完成\"结束测量")
                            .arg(currentLength, 0, 'f', 2), 3000);
            } else {
        emit showToastMessage(QString("已添加第%1个点，当前总长度: %2 mm，继续添加点或点击\"完成\"结束测量")
                            .arg(m_measurementPoints.size())
                            .arg(currentLength, 0, 'f', 2), 3000);
    }
    
    // 可选：检查是否点击接近第一个点（用于自动闭合，但不强制）
    const float CLOSING_THRESHOLD_MM = 5.0f;
    if (m_measurementPoints.size() >= 3) {
        QVector3D firstPoint3D_mm = m_measurementPoints.first();
        QVector3D currentPoint3D_mm = m_measurementPoints.last();
        float distanceToFirst = (currentPoint3D_mm - firstPoint3D_mm).length();
        
        // 如果接近第一个点，提供闭合选项，但不强制
        if (distanceToFirst < CLOSING_THRESHOLD_MM) {
            LOG_INFO("折线测量：检测到接近第一个点，可选择闭合");
            emit showToastMessage(QString("检测到接近起点（距离%1mm），可点击\"完成\"创建闭合折线")
                                .arg(distanceToFirst, 0, 'f', 1), 3000);
        }
    }
    
    // 触发UI更新以显示新增的点
    emit updateUI();
}

void ImageInteractionManager::clearTemporaryPoints()
{
    LOG_INFO("清除图像交互管理器中的临时测量点");
    m_originalClickPoints.clear();
    m_measurementPoints.clear();
    emit updateUI();
}

void ImageInteractionManager::handleMissingAreaMeasurement(const QVector3D& pcPointMeters, const QPoint& currentClickPoint)
{
    QVector3D point3D_mm = pcPointMeters * 1000.0f; // 转为毫米

    if (!m_hasIntersection) {
        // 阶段1：收集前4个点用于形成两条线段
        m_lineSegmentPoints.append(point3D_mm);
        m_lineSegmentClickPoints.append(currentClickPoint);

        LOG_INFO(QString("缺失面积测量：添加线段点 #%1 (%2, %3, %4)mm")
                .arg(m_lineSegmentPoints.size())
                .arg(point3D_mm.x(), 0, 'f', 2)
                .arg(point3D_mm.y(), 0, 'f', 2)
                .arg(point3D_mm.z(), 0, 'f', 2));

        if (m_lineSegmentPoints.size() == 1) {
            emit showToastMessage("已添加第1个点，请选择第2个点", 2000);
        } else if (m_lineSegmentPoints.size() == 2) {
            emit showToastMessage("已添加第2个点，请选择第3个点", 2000);
        } else if (m_lineSegmentPoints.size() == 3) {
            emit showToastMessage("已添加第3个点，请选择第4个点", 2000);
        } else if (m_lineSegmentPoints.size() == 4) {
            // 计算两条线段的交点
            // 第一条线段：p1-p2
            // 第二条线段：p3-p4
            QVector3D p1 = m_lineSegmentPoints[0];
            QVector3D p2 = m_lineSegmentPoints[1];
            QVector3D p3 = m_lineSegmentPoints[2];
            QVector3D p4 = m_lineSegmentPoints[3];

            LOG_INFO(QString("缺失面积测量：计算交点"));
            LOG_INFO(QString("第一条线段: P1(%1,%2,%3) - P2(%4,%5,%6)")
                    .arg(p1.x(), 0, 'f', 2).arg(p1.y(), 0, 'f', 2).arg(p1.z(), 0, 'f', 2)
                    .arg(p2.x(), 0, 'f', 2).arg(p2.y(), 0, 'f', 2).arg(p2.z(), 0, 'f', 2));
            LOG_INFO(QString("第二条线段: P3(%1,%2,%3) - P4(%4,%5,%6)")
                    .arg(p3.x(), 0, 'f', 2).arg(p3.y(), 0, 'f', 2).arg(p3.z(), 0, 'f', 2)
                    .arg(p4.x(), 0, 'f', 2).arg(p4.y(), 0, 'f', 2).arg(p4.z(), 0, 'f', 2));

            QVector3D intersection;
            bool hasIntersection = false;

            if (m_measurementCalculator) {
                hasIntersection = m_measurementCalculator->calculateLinesIntersection(p1, p2, p3, p4, intersection);
            }

            if (hasIntersection) {
                m_intersectionPoint = intersection;
                m_hasIntersection = true;

                // 计算2D交点坐标
                QPoint intersection2D = calculate2DIntersection(
                    m_lineSegmentClickPoints[0], m_lineSegmentClickPoints[1],
                    m_lineSegmentClickPoints[2], m_lineSegmentClickPoints[3]
                );

                // 初始化多边形点列表，交点作为第一个点
                m_polygonPoints.clear();
                m_polygonClickPoints.clear();
                m_polygonPoints.append(intersection);
                m_polygonClickPoints.append(intersection2D); // 使用计算得到的2D交点

                LOG_INFO(QString("缺失面积测量：计算得到3D交点 (%1, %2, %3)，2D交点 (%4, %5)")
                        .arg(intersection.x(), 0, 'f', 3)
                        .arg(intersection.y(), 0, 'f', 3)
                        .arg(intersection.z(), 0, 'f', 3)
                        .arg(intersection2D.x())
                        .arg(intersection2D.y()));

                emit showToastMessage("已计算交点，请继续选择点形成多边形，完成后点击\"完成\"按钮", 3000);
            } else {
                LOG_ERROR("缺失面积测量：无法计算交点，两条线段可能平行或不相交");
                emit showToastMessage("无法计算交点，请重新选择点", 2000);
                // 移除第4个点，让用户重新选择
                m_lineSegmentPoints.removeLast();
                m_lineSegmentClickPoints.removeLast();
                return;
            }
        }

        // 更新m_measurementPoints用于显示（包含线段点）
        m_measurementPoints = m_lineSegmentPoints;
        m_originalClickPoints = m_lineSegmentClickPoints;

    } else {
        // 阶段2：添加多边形点（与交点一起形成多边形）
        m_polygonPoints.append(point3D_mm);
        m_polygonClickPoints.append(currentClickPoint);

        int polygonPointCount = m_polygonPoints.size() - 1; // 减去交点
        LOG_INFO(QString("缺失面积测量：添加多边形点 #%1 (%2, %3, %4)mm")
                .arg(polygonPointCount)
                .arg(point3D_mm.x(), 0, 'f', 2)
                .arg(point3D_mm.y(), 0, 'f', 2)
                .arg(point3D_mm.z(), 0, 'f', 2));

        emit showToastMessage(QString("已添加第%1个多边形点，继续添加点或点击\"完成\"按钮完成测量")
                            .arg(polygonPointCount), 2500);

        // 更新m_measurementPoints用于显示（线段点 + 多边形点）
        m_measurementPoints = m_lineSegmentPoints + m_polygonPoints;
        m_originalClickPoints = m_lineSegmentClickPoints + m_polygonClickPoints;
    }

    // 触发UI更新以显示新增的点
    emit updateUI();
}

QPoint ImageInteractionManager::calculate2DIntersection(const QPoint& line1P1, const QPoint& line1P2,
                                                       const QPoint& line2P1, const QPoint& line2P2)
{
    // 将QPoint转换为浮点数进行计算
    float x1 = line1P1.x(), y1 = line1P1.y();
    float x2 = line1P2.x(), y2 = line1P2.y();
    float x3 = line2P1.x(), y3 = line2P1.y();
    float x4 = line2P2.x(), y4 = line2P2.y();

    // 计算方向向量
    float dx1 = x2 - x1, dy1 = y2 - y1;
    float dx2 = x4 - x3, dy2 = y4 - y3;

    // 计算叉积（2D中的"叉积"实际是标量）
    float cross = dx1 * dy2 - dy1 * dx2;

    // 检查两条直线是否平行
    if (std::abs(cross) < 1e-6f) {
        LOG_WARNING("2D线段平行，无法计算交点，返回第一条线段中点");
        return QPoint((line1P1.x() + line1P2.x()) / 2, (line1P1.y() + line1P2.y()) / 2);
    }

    // 计算参数t1
    float w_x = x1 - x3, w_y = y1 - y3;
    float t1 = (dx2 * w_y - dy2 * w_x) / cross;

    // 计算交点
    float intersectionX = x1 + t1 * dx1;
    float intersectionY = y1 + t1 * dy1;

    QPoint result(static_cast<int>(intersectionX), static_cast<int>(intersectionY));

    LOG_INFO(QString("计算2D交点: 线段1(%1,%2)-(%3,%4), 线段2(%5,%6)-(%7,%8) => 交点(%9,%10)")
             .arg(line1P1.x()).arg(line1P1.y()).arg(line1P2.x()).arg(line1P2.y())
             .arg(line2P1.x()).arg(line2P1.y()).arg(line2P2.x()).arg(line2P2.y())
             .arg(result.x()).arg(result.y()));

    return result;
}

} // namespace Ui
} // namespace App
} // namespace SmartScope