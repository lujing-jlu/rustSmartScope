#include "stereo_calibration_helper.h"
#include "infrastructure/logging/logger.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

namespace SmartScope::App::Calibration {

StereoCalibrationHelper::StereoCalibrationHelper(QObject *parent)
    : QObject(parent)
{
    LOG_INFO("StereoCalibrationHelper instance created.");
}

// --- Getters Implementation ---

cv::Mat StereoCalibrationHelper::getCameraMatrixLeft() const { return m_cameraMatrixLeft; }
cv::Mat StereoCalibrationHelper::getDistCoeffsLeft() const { return m_distCoeffsLeft; }
cv::Mat StereoCalibrationHelper::getCameraMatrixRight() const { return m_cameraMatrixRight; }
cv::Mat StereoCalibrationHelper::getDistCoeffsRight() const { return m_distCoeffsRight; }
cv::Mat StereoCalibrationHelper::getRotationMatrix() const { return m_rotationMatrix; }
cv::Mat StereoCalibrationHelper::getTranslationVector() const { return m_translationVector; }
cv::Mat StereoCalibrationHelper::getR1() const { return m_R1; }
cv::Mat StereoCalibrationHelper::getR2() const { return m_R2; }
cv::Mat StereoCalibrationHelper::getP1() const { return m_P1; }
cv::Mat StereoCalibrationHelper::getP2() const { return m_P2; }
cv::Mat StereoCalibrationHelper::getQMatrix() const { return m_Q; }
cv::Rect StereoCalibrationHelper::getRoi1() const { return m_roi1; }
cv::Rect StereoCalibrationHelper::getRoi2() const { return m_roi2; }
bool StereoCalibrationHelper::isRemapInitialized() const { return m_remapInitialized; }
bool StereoCalibrationHelper::areParametersLoaded() const { return m_parametersLoaded; }
cv::Mat StereoCalibrationHelper::getMap1x() const { return m_map1x; }
cv::Mat StereoCalibrationHelper::getMap1y() const { return m_map1y; }
cv::Mat StereoCalibrationHelper::getMap2x() const { return m_map2x; }
cv::Mat StereoCalibrationHelper::getMap2y() const { return m_map2y; }

// --- Moved Methods Implementation ---

bool StereoCalibrationHelper::loadParameters(const QString& basePath)
{
    LOG_INFO("开始加载相机参数...");
    // 解析基础路径：为空或相对路径时，统一使用应用目录
    QString basePathResolved = basePath;
    if (basePathResolved.isEmpty() || QFileInfo(basePathResolved).isRelative()) {
        basePathResolved = QCoreApplication::applicationDirPath() + "/camera_parameters";
    }
    LOG_INFO(QString("使用基础路径: %1").arg(basePathResolved));
    LOG_INFO(QString("当前工作目录: %1").arg(QDir::currentPath()));

    QString leftCameraPath = basePathResolved + "/camera0_intrinsics.dat";
    QString rightCameraPath = basePathResolved + "/camera1_intrinsics.dat";
    QString rotTransPath = basePathResolved + "/camera1_rot_trans.dat";

    // 检查文件是否存在
    if (!QFile::exists(leftCameraPath)) {
        LOG_ERROR(QString("左相机参数文件不存在: %1").arg(leftCameraPath));
        m_parametersLoaded = false;
        return false;
    }
    if (!QFile::exists(rightCameraPath)) {
        LOG_ERROR(QString("右相机参数文件不存在: %1").arg(rightCameraPath));
        m_parametersLoaded = false;
        return false;
    }
    if (!QFile::exists(rotTransPath)) {
        LOG_ERROR(QString("旋转平移参数文件不存在: %1").arg(rotTransPath));
        m_parametersLoaded = false;
        return false;
    }

    LOG_INFO(QString("使用相机参数文件:\n左相机: %1\n右相机: %2\n旋转平移: %3")
        .arg(leftCameraPath)
        .arg(rightCameraPath)
        .arg(rotTransPath));

    LOG_INFO("开始读取相机参数文件...");
    
    try {
        // 清空旧参数
        m_cameraMatrixLeft = cv::Mat();
        m_distCoeffsLeft = cv::Mat();
        m_cameraMatrixRight = cv::Mat();
        m_distCoeffsRight = cv::Mat();
        m_rotationMatrix = cv::Mat();
        m_translationVector = cv::Mat();

        // 从自定义文本格式解析相机参数
        bool success = parseIntrinsicsFile(leftCameraPath, m_cameraMatrixLeft, m_distCoeffsLeft);
        if (!success) {
            LOG_ERROR("解析左相机内参文件失败");
            m_parametersLoaded = false;
            return false;
        }
        
        success = parseIntrinsicsFile(rightCameraPath, m_cameraMatrixRight, m_distCoeffsRight);
        if (!success) {
            LOG_ERROR("解析右相机内参文件失败");
            m_parametersLoaded = false;
            return false;
        }
        
        success = parseRotTransFile(rotTransPath, m_rotationMatrix, m_translationVector);
        if (!success) {
            LOG_ERROR("解析旋转平移参数文件失败");
            m_parametersLoaded = false;
            return false;
        }
        
        // 打印相机参数信息
        LOG_INFO("相机参数加载成功");
        printMatrixContent(m_cameraMatrixLeft, "左相机内参矩阵");
        printMatrixContent(m_distCoeffsLeft, "左相机畸变系数");
        printMatrixContent(m_cameraMatrixRight, "右相机内参矩阵");
        printMatrixContent(m_distCoeffsRight, "右相机畸变系数");
        printMatrixContent(m_rotationMatrix, "旋转矩阵");
        printMatrixContent(m_translationVector, "平移向量");
        
        m_parametersLoaded = true;
        m_remapInitialized = false; // 参数加载后需要重新初始化校正
        return true;
    } catch (const cv::Exception& e) {
        LOG_ERROR(QString("加载相机参数OpenCV异常: %1").arg(e.what()));
        m_parametersLoaded = false;
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("加载相机参数标准异常: %1").arg(e.what()));
        m_parametersLoaded = false;
        return false;
    } catch (...) {
        LOG_ERROR("加载相机参数未知异常");
        m_parametersLoaded = false;
        return false;
    }
}

bool StereoCalibrationHelper::parseIntrinsicsFile(const QString& filePath, cv::Mat& cameraMatrix, cv::Mat& distCoeffs)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(QString("无法打开内参文件: %1").arg(filePath));
        return false;
    }
    
    cameraMatrix = cv::Mat::zeros(3, 3, CV_64F);
    distCoeffs = cv::Mat::zeros(1, 5, CV_64F);
    
    QTextStream in(&file);
    QString line;
    
    // Read intrinsic matrix
    line = in.readLine();
    if (!line.contains("intrinsic:", Qt::CaseInsensitive)) {
        LOG_ERROR(QString("内参文件格式错误，未找到'intrinsic:'标记: %1").arg(filePath));
        file.close(); return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (in.atEnd()) { LOG_ERROR(QString("内参文件格式错误，矩阵数据不完整: %1").arg(filePath)); file.close(); return false; }
        line = in.readLine().trimmed();
        QStringList values = line.split(' ', Qt::SkipEmptyParts);
        if (values.size() != 3) { LOG_ERROR(QString("内参矩阵行%1格式错误，期望3个值: %2").arg(i).arg(line)); file.close(); return false; }
        for (int j = 0; j < 3; ++j) {
            bool ok;
            double val = values[j].toDouble(&ok);
            if (!ok) { LOG_ERROR(QString("内参矩阵数值转换错误: %1").arg(values[j])); file.close(); return false; }
            cameraMatrix.at<double>(i, j) = val;
        }
    }
    
    // Read distortion coefficients
    line = in.readLine();
    if (!line.contains("distortion:", Qt::CaseInsensitive)) {
        LOG_ERROR(QString("内参文件格式错误，未找到'distortion:'标记: %1").arg(filePath));
        file.close(); return false;
    }
    if (in.atEnd()) { LOG_ERROR(QString("内参文件格式错误，畸变系数数据不完整: %1").arg(filePath)); file.close(); return false; }
    line = in.readLine().trimmed();
    QStringList values = line.split(' ', Qt::SkipEmptyParts);
    if (values.size() != 5) { LOG_ERROR(QString("畸变系数格式错误，期望5个值: %1").arg(line)); file.close(); return false; }
    for (int i = 0; i < 5; ++i) {
        bool ok;
        double val = values[i].toDouble(&ok);
        if (!ok) { LOG_ERROR(QString("畸变系数数值转换错误: %1").arg(values[i])); file.close(); return false; }
        distCoeffs.at<double>(0, i) = val;
    }
    
    file.close();
    LOG_INFO(QString("成功解析相机内参文件: %1").arg(filePath));
    return true;
}

bool StereoCalibrationHelper::parseRotTransFile(const QString& filePath, cv::Mat& rotationMatrix, cv::Mat& translationVector)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(QString("无法打开旋转平移文件: %1").arg(filePath));
        return false;
    }
    
    rotationMatrix = cv::Mat::eye(3, 3, CV_64F);
    translationVector = cv::Mat::zeros(3, 1, CV_64F);
    
    QTextStream in(&file);
    QString line;
    
    // Read rotation matrix
    line = in.readLine();
    if (!line.contains("R:", Qt::CaseInsensitive)) {
        LOG_ERROR(QString("旋转平移文件格式错误，未找到'R:'标记: %1").arg(filePath));
        file.close(); return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (in.atEnd()) { LOG_ERROR(QString("旋转平移文件格式错误，旋转矩阵数据不完整: %1").arg(filePath)); file.close(); return false; }
        line = in.readLine().trimmed();
        QStringList values = line.split(' ', Qt::SkipEmptyParts);
        if (values.size() != 3) { LOG_ERROR(QString("旋转矩阵行%1格式错误，期望3个值: %2").arg(i).arg(line)); file.close(); return false; }
        for (int j = 0; j < 3; ++j) {
            bool ok;
            double val = values[j].toDouble(&ok);
            if (!ok) { LOG_ERROR(QString("旋转矩阵数值转换错误: %1").arg(values[j])); file.close(); return false; }
            rotationMatrix.at<double>(i, j) = val;
        }
    }
    
    // Read translation vector
    line = in.readLine();
    if (!line.contains("T:", Qt::CaseInsensitive)) {
        LOG_ERROR(QString("旋转平移文件格式错误，未找到'T:'标记: %1").arg(filePath));
        file.close(); return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (in.atEnd()) { LOG_ERROR(QString("旋转平移文件格式错误，平移向量数据不完整: %1").arg(filePath)); file.close(); return false; }
        line = in.readLine().trimmed();
        if (line.isEmpty()) { LOG_ERROR(QString("平移向量行%1为空: %2").arg(i).arg(filePath)); file.close(); return false; }
        bool ok;
        double val = line.toDouble(&ok);
        if (!ok) { LOG_ERROR(QString("平移向量数值转换错误: %1").arg(line)); file.close(); return false; }
        translationVector.at<double>(i, 0) = val;
    }
    
    file.close();
    LOG_INFO(QString("成功解析旋转平移参数文件: %1").arg(filePath));
    return true;
}

bool StereoCalibrationHelper::initializeRectification(const cv::Size& imageSize)
{
    LOG_INFO(QString("初始化立体校正，图像尺寸: %1x%2").arg(imageSize.width).arg(imageSize.height));
    
    if (imageSize.width <= 0 || imageSize.height <= 0) {
        LOG_ERROR(QString("图像尺寸无效: %1x%2").arg(imageSize.width).arg(imageSize.height));
        return false;
    }
    
    if (!m_parametersLoaded) {
        LOG_ERROR("相机参数未加载，无法初始化立体校正");
        return false;
    }
    
    // Validate matrices dimensions (basic check)
    if (m_cameraMatrixLeft.rows != 3 || m_cameraMatrixLeft.cols != 3 ||
        m_cameraMatrixRight.rows != 3 || m_cameraMatrixRight.cols != 3 ||
        m_distCoeffsLeft.empty() || m_distCoeffsRight.empty() ||
        m_rotationMatrix.rows != 3 || m_rotationMatrix.cols != 3 || m_translationVector.empty()) {
        LOG_ERROR("相机参数维度或内容无效");
        return false;
    }
    
    try {
        // Clear previous rectification data
        m_R1.release(); m_R2.release();
        m_P1.release(); m_P2.release();
        m_Q.release();
        m_map1x.release(); m_map1y.release();
        m_map2x.release(); m_map2y.release();
        m_roi1 = cv::Rect(); m_roi2 = cv::Rect();

        LOG_INFO("开始计算立体校正参数...");
        cv::stereoRectify(
            m_cameraMatrixLeft, m_distCoeffsLeft,
            m_cameraMatrixRight, m_distCoeffsRight,
            imageSize, m_rotationMatrix, m_translationVector,
            m_R1, m_R2, m_P1, m_P2, m_Q,
            cv::CALIB_ZERO_DISPARITY, -1.0, // Alpha = -1.0 means default scaling (good compromise)
            imageSize, // Output image size same as input
            &m_roi1, &m_roi2
        );
        LOG_INFO("立体校正参数计算成功");
        LOG_INFO(QString("左相机有效区域ROI: (%1, %2, %3, %4)")
                 .arg(m_roi1.x).arg(m_roi1.y).arg(m_roi1.width).arg(m_roi1.height));
        LOG_INFO(QString("右相机有效区域ROI: (%1, %2, %3, %4)")
                 .arg(m_roi2.x).arg(m_roi2.y).arg(m_roi2.width).arg(m_roi2.height));
        printMatrixContent(m_Q, "重投影矩阵 Q");

        if (m_R1.empty() || m_R2.empty() || m_P1.empty() || m_P2.empty() || m_Q.empty()) {
            LOG_ERROR("立体校正参数生成失败，结果矩阵为空");
            m_remapInitialized = false;
            return false;
        }

        LOG_INFO("开始计算重映射表...");
        cv::initUndistortRectifyMap(
            m_cameraMatrixLeft, m_distCoeffsLeft, m_R1, m_P1,
            imageSize, CV_32FC1, // Use CV_32FC1 for maps
            m_map1x, m_map1y
        );
        cv::initUndistortRectifyMap(
            m_cameraMatrixRight, m_distCoeffsRight, m_R2, m_P2,
            imageSize, CV_32FC1, // Use CV_32FC1 for maps
            m_map2x, m_map2y
        );
        LOG_INFO("重映射表计算成功");

        if (m_map1x.empty() || m_map1y.empty() || m_map2x.empty() || m_map2y.empty()) {
            LOG_ERROR("重映射表生成失败，结果为空");
            m_remapInitialized = false;
            return false;
        }

        if (m_map1x.size() != imageSize || m_map1y.size() != imageSize ||
            m_map2x.size() != imageSize || m_map2y.size() != imageSize) {
            LOG_ERROR(QString("重映射表尺寸与图像不一致，图像: %1x%2, 重映射表: %3x%4")
                     .arg(imageSize.width).arg(imageSize.height)
                     .arg(m_map1x.cols).arg(m_map1x.rows));
             m_remapInitialized = false;
            return false;
        }

        LOG_INFO("立体校正参数和重映射表初始化成功");
        m_remapInitialized = true;
        return true;
    } catch (const cv::Exception& e) {
        LOG_ERROR(QString("初始化立体校正OpenCV异常: %1").arg(e.what()));
        m_remapInitialized = false;
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("初始化立体校正标准异常: %1").arg(e.what()));
        m_remapInitialized = false;
        return false;
    } catch (...) {
        LOG_ERROR("初始化立体校正未知异常");
        m_remapInitialized = false;
        return false;
    }
}

bool StereoCalibrationHelper::rectifyImages(cv::Mat& leftImage, cv::Mat& rightImage)
{
    if (leftImage.empty() && rightImage.empty()) {
        LOG_WARNING("左右图像均为空，无法进行校正");
        return false;
    }
    
    if (!m_remapInitialized) {
        LOG_WARNING("重映射变换表未初始化，无法进行图像校正");
        return false;
    }
    
    try {
        cv::Mat leftRectified, rightRectified;
        
        if (!leftImage.empty()) {
            // Ensure maps match image size before remap
            if (m_map1x.size() != leftImage.size()) {
                 LOG_WARNING(QString("左相机重映射表尺寸(%1x%2)与图像尺寸(%3x%4)不匹配，需要重新初始化校正！")
                           .arg(m_map1x.cols).arg(m_map1x.rows).arg(leftImage.cols).arg(leftImage.rows));
                 // Attempt reinitialization - consider if this is the right place
                 if (!initializeRectification(leftImage.size())) {
                     LOG_ERROR("重新初始化校正失败，无法校正左图像");
                     return false;
                 }
            }
            cv::remap(leftImage, leftRectified, m_map1x, m_map1y, cv::INTER_LINEAR);
            
             // Apply ROI
            if (m_roi1.width > 0 && m_roi1.height > 0 && 
                m_roi1.x >= 0 && m_roi1.y >= 0 && 
                m_roi1.x + m_roi1.width <= leftRectified.cols && 
                m_roi1.y + m_roi1.height <= leftRectified.rows) {
                 leftRectified = leftRectified(m_roi1);
             } else {
                 LOG_WARNING(QString("左图像ROI无效或超出边界: (%1, %2, %3, %4)，使用完整图像")
                          .arg(m_roi1.x).arg(m_roi1.y).arg(m_roi1.width).arg(m_roi1.height));
             }
        }
        
        if (!rightImage.empty()) {
            if (m_map2x.size() != rightImage.size()) {
                 LOG_WARNING(QString("右相机重映射表尺寸(%1x%2)与图像尺寸(%3x%4)不匹配，需要重新初始化校正！")
                           .arg(m_map2x.cols).arg(m_map2x.rows).arg(rightImage.cols).arg(rightImage.rows));
                 if (!initializeRectification(rightImage.size())) { // Assuming left/right size is same
                    LOG_ERROR("重新初始化校正失败，无法校正右图像");
                    return false;
                 }
            }
            cv::remap(rightImage, rightRectified, m_map2x, m_map2y, cv::INTER_LINEAR);
            LOG_INFO(QString("右图像校正后尺寸: %1x%2").arg(rightRectified.cols).arg(rightRectified.rows));
            
            // Apply ROI
             if (m_roi2.width > 0 && m_roi2.height > 0 && 
                 m_roi2.x >= 0 && m_roi2.y >= 0 && 
                 m_roi2.x + m_roi2.width <= rightRectified.cols && 
                 m_roi2.y + m_roi2.height <= rightRectified.rows) {
                 LOG_INFO(QString("裁剪右图像ROI: (%1, %2, %3, %4)")
                       .arg(m_roi2.x).arg(m_roi2.y).arg(m_roi2.width).arg(m_roi2.height));
                 rightRectified = rightRectified(m_roi2);
             } else {
                 LOG_WARNING(QString("右图像ROI无效或超出边界: (%1, %2, %3, %4)，使用完整图像")
                          .arg(m_roi2.x).arg(m_roi2.y).arg(m_roi2.width).arg(m_roi2.height));
             }
        }
        
        // 恢复尺寸一致性调整，确保左右图像尺寸一致
        if (!leftRectified.empty() && !rightRectified.empty()) {
            cv::Size leftSize = leftRectified.size();
            cv::Size rightSize = rightRectified.size();
            if (leftSize != rightSize) {
                LOG_WARNING(QString("左右校正后(含ROI)图像尺寸不一致: 左 %1x%2，右 %3x%4")
                         .arg(leftSize.width).arg(leftSize.height)
                         .arg(rightSize.width).arg(rightSize.height));
                // 使用较小的尺寸作为统一尺寸，避免信息丢失
                cv::Size minSize(std::min(leftSize.width, rightSize.width),
                                 std::min(leftSize.height, rightSize.height));
                LOG_INFO(QString("调整左右图像为共同尺寸: %1x%2").arg(minSize.width).arg(minSize.height));
                if (leftSize != minSize) { 
                    leftRectified = leftRectified(cv::Rect(0, 0, minSize.width, minSize.height)); 
                }
                if (rightSize != minSize) { 
                    rightRectified = rightRectified(cv::Rect(0, 0, minSize.width, minSize.height)); 
                }
            }
        }
        
        // Copy back to references
        if (!leftRectified.empty()) leftRectified.copyTo(leftImage);
        if (!rightRectified.empty()) rightRectified.copyTo(rightImage);
        
        return true;
    } catch (const cv::Exception& e) {
        LOG_ERROR(QString("图像校正OpenCV异常: %1").arg(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("图像校正未知异常");
        return false;
    }
}

void StereoCalibrationHelper::printMatrixContent(const cv::Mat& mat, const QString& name)
{
    if (mat.empty()) {
        LOG_INFO(QString("矩阵 %1 为空").arg(name));
        return;
    }
    
    QString matContent = QString("%1 [%2x%3, type:%4]:\n").arg(name).arg(mat.rows).arg(mat.cols).arg(mat.type());
    int maxRows = std::min(5, mat.rows);
    int maxCols = std::min(10, mat.cols);
    
    for (int i = 0; i < maxRows; ++i) {
        matContent += "[ ";
        for (int j = 0; j < maxCols; ++j) {
            QString valStr;
            if (mat.type() == CV_64F) {
                valStr = QString::number(mat.at<double>(i, j), 'f', 4);
            } else if (mat.type() == CV_32F) {
                valStr = QString::number(mat.at<float>(i, j), 'f', 4);
            } else if (mat.type() == CV_8U) {
                valStr = QString::number(mat.at<uchar>(i, j));
            } else {
                valStr = "?" + QString::number(mat.type());
            }
            matContent += valStr + (j < maxCols - 1 ? ", " : "");
        }
        if (maxCols < mat.cols) matContent += " ...";
        matContent += " ]\n";
    }
    if (maxRows < mat.rows) matContent += "...";
    
    LOG_INFO(matContent);
}

} // namespace SmartScope::App::Calibration 