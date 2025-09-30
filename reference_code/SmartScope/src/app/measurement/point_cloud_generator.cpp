#include "app/measurement/point_cloud_generator.h"
#include <limits> // Required for std::numeric_limits

namespace SmartScope::App::Measurement {

PointCloudGenerator::PointCloudGenerator() {
    LOG_INFO("PointCloudGenerator created.");
}

PointCloudGenerator::~PointCloudGenerator() {
    LOG_INFO("PointCloudGenerator destroyed.");
}

bool PointCloudGenerator::generate(
    const cv::Mat& depthMap,
    const cv::Mat& colorImageInput, // Use a different name to avoid shadowing
    std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager,
    std::vector<QVector3D>& outPoints,
    std::vector<QVector3D>& outColors,
    std::vector<cv::Point2i>& outPixelCoords,
    int step,
    float maxDepthMm,
    float gradientThresholdFactor)
{
    LOG_INFO("PointCloudGenerator starting point cloud generation...");

    if (depthMap.empty()) {
        LOG_WARNING("Depth map is empty, cannot generate point cloud.");
        return false;
    }
    if (colorImageInput.empty()) {
        LOG_WARNING("Color image is empty, point cloud will be generated without color or with pseudo-color.");
        // Continue generation, but handle color appropriately later.
    }
    if (!correctionManager) {
        LOG_ERROR("Correction manager is invalid, cannot generate point cloud.");
        return false;
    }
    
    auto stereoHelper = correctionManager->getStereoCalibrationHelper();
    if (!stereoHelper || !stereoHelper->isRemapInitialized()) {
        LOG_ERROR("Stereo calibration helper is invalid or not initialized, cannot generate point cloud.");
        return false;
    }
     if (depthMap.size() != colorImageInput.size() && !colorImageInput.empty()) {
         LOG_ERROR(QString("Depth map size (%1x%2) does not match color image size (%3x%4). Cannot generate point cloud.")
                   .arg(depthMap.cols).arg(depthMap.rows)
                   .arg(colorImageInput.cols).arg(colorImageInput.rows));
         return false;
     }


    try {
        outPoints.clear();
        outColors.clear();
        outPixelCoords.clear();

        // --- Get Calibration Parameters ---
        cv::Mat Q_matrix = stereoHelper->getQMatrix();
        // Camera parameters might be useful for manual calculation or validation, but reprojectImageTo3D mainly uses Q.
        // cv::Mat camMatrixLeft = stereoHelper->getCameraMatrixLeft();
        // cv::Mat transVector = stereoHelper->getTranslationVector();
        // double baseline = std::abs(transVector.at<double>(0));
        // double focal_length = camMatrixLeft.at<double>(0, 0);
        // double cx = camMatrixLeft.at<double>(0, 2);
        // double cy = camMatrixLeft.at<double>(1, 2);

        if (Q_matrix.empty()) {
            LOG_ERROR("Failed to get Q matrix from calibration helper.");
            return false;
        }
        // LOG_INFO(QString("Point cloud generation using Q matrix.")); // Baseline/focal length info removed as Q is used directly.


        // --- Prepare Depth Map and Masks ---
        cv::Mat depthFloat;
        if (depthMap.type() != CV_32F) {
            depthMap.convertTo(depthFloat, CV_32F);
        } else {
            depthFloat = depthMap.clone(); // Use clone to ensure it's modifiable if needed
        }

        // Check for valid depth values
        int initialValidCount = cv::countNonZero(depthFloat > 0);
        if (initialValidCount <= 0) {
            LOG_WARNING("Depth map contains no valid positive depth values.");
            return false; // Nothing to generate
        }

        // Calculate depth gradient for filtering steep changes
        cv::Mat finalValidMask;
        if (gradientThresholdFactor > 0.0f) {
        cv::Mat depthGradX, depthGradY, depthGradMag;
        cv::Sobel(depthFloat, depthGradX, CV_32F, 1, 0, 3);
        cv::Sobel(depthFloat, depthGradY, CV_32F, 0, 1, 3);
        cv::magnitude(depthGradX, depthGradY, depthGradMag);

        // Determine gradient threshold
        double minDepth, maxObservedDepth;
            cv::minMaxLoc(depthFloat, &minDepth, &maxObservedDepth, nullptr, nullptr, depthFloat > 0);
            float gradientThreshold = static_cast<float>(maxObservedDepth) * gradientThresholdFactor;
            LOG_INFO(QString("Depth gradient threshold: %1 (Factor: %2)")
                     .arg(gradientThreshold).arg(gradientThresholdFactor));

        // Create gradient mask (true for smooth areas)
        cv::Mat gradientMask = depthGradMag < gradientThreshold;

        // Optional: Optimize mask with morphological operations
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::morphologyEx(gradientMask, gradientMask, cv::MORPH_OPEN, kernel); // Remove small noise in mask

        // Combine depth validity mask and gradient mask
            finalValidMask = (depthFloat > 0) & gradientMask;
        } else {
            // 禁用梯度与形态学过滤：仅使用深度>0作为有效掩码
            finalValidMask = (depthFloat > 0);
            LOG_INFO("Gradient-based filtering disabled. Using depth>0 mask only.");
        }

        // Log filtering results
        int filteredValidCount = cv::countNonZero(finalValidMask);
        LOG_INFO(QString("Gradient Filtering: Initial valid points=%1, After filtering=%2. Removed %3 points (%4%).")
                 .arg(initialValidCount)
                 .arg(filteredValidCount)
                 .arg(initialValidCount - filteredValidCount)
                 .arg(initialValidCount > 0 ? (float)(initialValidCount - filteredValidCount) * 100.0f / initialValidCount : 0.0f));

        if (filteredValidCount <= 0) {
             LOG_WARNING("No valid points remaining after gradient filtering.");
             return true; // Technically successful (no points generated), but maybe return false? Let's return true for now.
        }


        // --- Prepare Color Image ---
        cv::Mat colorImage;
        bool usePseudoColor = false;
        if (colorImageInput.empty()) {
             LOG_WARNING("No color image provided. Generating pseudo-color based on depth.");
             usePseudoColor = true;
             // Generate pseudo-color map later if needed
        } else {
             if (colorImageInput.channels() == 1) {
                 cv::cvtColor(colorImageInput, colorImage, cv::COLOR_GRAY2BGR);
             } else if (colorImageInput.channels() == 3) {
                 colorImage = colorImageInput; // Assume BGR
             } else {
                 LOG_ERROR(QString("Unsupported color image channel count: %1. Cannot generate colors.").arg(colorImageInput.channels()));
                 return false; // Or proceed without color? Let's fail for now.
             }
             LOG_INFO("Using provided color image for point cloud colors.");
        }

        // Generate pseudo-color if needed (only done once)
        cv::Mat pseudoColorMap;
        if (usePseudoColor) {
            cv::Mat normalizedDepth;
            cv::normalize(depthMap, normalizedDepth, 0, 255, cv::NORM_MINMAX, CV_8U, depthFloat > 0); // Normalize based on valid depth
            cv::applyColorMap(normalizedDepth, pseudoColorMap, cv::COLORMAP_JET);
            colorImage = pseudoColorMap; // Use this for coloring
            LOG_INFO("Generated pseudo-color map based on depth.");
        }

        // --- Generate 3D Points ---
        cv::Mat points3D_mm; // Points in mm, OpenCV coordinate system
        // Use the Q matrix for reprojection
        cv::reprojectImageTo3D(depthFloat, points3D_mm, Q_matrix, true, CV_32F); // Handle disparities = false

        // Pre-allocate memory for output vectors
        outPoints.reserve(filteredValidCount / (step * step) + 1); // Estimate size
        outColors.reserve(filteredValidCount / (step * step) + 1);
        outPixelCoords.reserve(filteredValidCount / (step * step) + 1);

        // --- Extract Points, Colors, and Coordinates ---
        for (int y = 0; y < points3D_mm.rows; y += step) {
            for (int x = 0; x < points3D_mm.cols; x += step) {
                if (finalValidMask.at<uchar>(y, x)) { // Check the final mask
                    cv::Vec3f point_mm = points3D_mm.at<cv::Vec3f>(y, x);

                    // Filter invalid points (infinite, NaN) and points beyond max depth
                    if (std::isfinite(point_mm[0]) && std::isfinite(point_mm[1]) && std::isfinite(point_mm[2])) {
                        if (std::abs(point_mm[2]) < maxDepthMm) { // Check against max depth limit
                            // Coordinate system conversion:
                            // OpenCV: X right, Y down, Z forward
                            // Qt 3D:  X right, Y up,   Z out (towards viewer)
                            // Convert mm to meters and flip Y and Z axes
                            QVector3D qPoint_meters(
                                point_mm[0] / 1000.0f,
                                -point_mm[1] / 1000.0f, // Flip Y
                                -point_mm[2] / 1000.0f  // Flip Z
                            );

                            // Get color (BGR format from OpenCV)
                            cv::Vec3b bgrColor = colorImage.at<cv::Vec3b>(y, x);
                            // Convert BGR to RGB and normalize to 0.0-1.0
                            QVector3D qColor(
                                bgrColor[2] / 255.0f, // R
                                bgrColor[1] / 255.0f, // G
                                bgrColor[0] / 255.0f  // B
                            );

                            // Store the results
                            outPoints.push_back(qPoint_meters);
                            outColors.push_back(qColor);
                            outPixelCoords.push_back(cv::Point2i(x, y));
                        }
                    }
                }
            }
        }

        LOG_INFO(QString("Point cloud generation complete. Generated %1 points (Step=%2).")
                 .arg(outPoints.size()).arg(step));
        return true;

    } catch (const cv::Exception& e) {
        LOG_ERROR(QString("OpenCV exception during point cloud generation: %1").arg(e.what()));
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Standard exception during point cloud generation: %1").arg(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception during point cloud generation.");
        return false;
    }
}

} // namespace SmartScope::App::Measurement 