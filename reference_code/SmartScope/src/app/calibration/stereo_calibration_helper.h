#pragma once

#include <opencv2/opencv.hpp>
#include <QString>
#include <QSize>
#include <QVector3D> // For potential future use or compatibility
#include <QObject> // Include QObject for logging context if needed

// Forward declaration if needed
// class OtherClass;

namespace SmartScope::App::Calibration {

class StereoCalibrationHelper : public QObject {
    Q_OBJECT // Add Q_OBJECT macro if using signals/slots or properties

public:
    explicit StereoCalibrationHelper(QObject *parent = nullptr);
    ~StereoCalibrationHelper() override = default; // Use default destructor

    // Public interface for loading and initialization
    bool loadParameters(const QString& basePath = "./camera_parameters");
    bool initializeRectification(const cv::Size& imageSize);
    bool rectifyImages(cv::Mat& leftImage, cv::Mat& rightImage);

    // Getters for necessary parameters
    cv::Mat getCameraMatrixLeft() const;
    cv::Mat getDistCoeffsLeft() const;
    cv::Mat getCameraMatrixRight() const;
    cv::Mat getDistCoeffsRight() const;
    cv::Mat getRotationMatrix() const;
    cv::Mat getTranslationVector() const;
    cv::Mat getR1() const;
    cv::Mat getR2() const;
    cv::Mat getP1() const;
    cv::Mat getP2() const;
    cv::Mat getQMatrix() const;
    cv::Rect getRoi1() const;
    cv::Rect getRoi2() const;
    bool isRemapInitialized() const;
    bool areParametersLoaded() const;
    cv::Mat getMap1x() const;
    cv::Mat getMap1y() const;
    cv::Mat getMap2x() const;
    cv::Mat getMap2y() const;

private:
    // Helper methods moved from MeasurementPage
    bool parseIntrinsicsFile(const QString& filePath, cv::Mat& cameraMatrix, cv::Mat& distCoeffs);
    bool parseRotTransFile(const QString& filePath, cv::Mat& rotationMatrix, cv::Mat& translationVector);
    void printMatrixContent(const cv::Mat& mat, const QString& name);

    // Member variables moved from MeasurementPage
    cv::Mat m_cameraMatrixLeft;
    cv::Mat m_distCoeffsLeft;
    cv::Mat m_cameraMatrixRight;
    cv::Mat m_distCoeffsRight;
    cv::Mat m_rotationMatrix;     // R
    cv::Mat m_translationVector;  // T
    cv::Mat m_R1, m_R2;           // Rotation matrices for rectified cameras
    cv::Mat m_P1, m_P2;           // Projection matrices in rectified coordinates
    cv::Mat m_Q;                  // Disparity-to-depth mapping matrix (reprojection matrix)
    cv::Mat m_map1x, m_map1y;     // Rectification maps for the left camera
    cv::Mat m_map2x, m_map2y;     // Rectification maps for the right camera
    cv::Rect m_roi1, m_roi2;      // Regions of interest in rectified images

    // State flags
    bool m_parametersLoaded = false;
    bool m_remapInitialized = false;
};

} // namespace SmartScope::App::Calibration 