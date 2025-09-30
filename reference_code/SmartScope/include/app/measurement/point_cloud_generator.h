#ifndef SMART_SCOPE_POINT_CLOUD_GENERATOR_H
#define SMART_SCOPE_POINT_CLOUD_GENERATOR_H

#include <vector>
#include <QVector3D> // For 3D points and colors
#include <opencv2/opencv.hpp> // For cv::Mat and cv::Point2i
#include "core/camera/camera_correction_manager.h" // For accessing calibration data
#include "infrastructure/logging/logger.h" // For logging

// Forward declaration for Calibration Helper if needed (included above)

namespace SmartScope::App::Measurement {

/**
 * @brief Generates a 3D point cloud from depth and color images using calibration data.
 */
class PointCloudGenerator {
public:
    PointCloudGenerator();
    ~PointCloudGenerator();

    /**
     * @brief Generates the point cloud.
     *
     * @param depthMap The input depth map (CV_32F or other types convertible to CV_32F, values typically in mm).
     * @param colorImageInput The input color image (CV_8UC3 BGR or CV_8UC1 Grayscale) for coloring the points.
     *                        Must have the same dimensions as depthMap. If empty, pseudo-color will be generated.
     * @param correctionManager Pointer to the initialized CameraCorrectionManager for accessing Q matrix etc.
     * @param outPoints Output vector to store the calculated 3D point coordinates (in meters, Qt 3D coordinate system).
     * @param outColors Output vector to store the corresponding point colors (RGB, 0.0-1.0).
     * @param outPixelCoords Output vector to store the original 2D pixel coordinates (x, y) for each generated point.
     * @param step Sampling step for pixels (e.g., 1 for every pixel, 2 for every other pixel).
     * @param maxDepthMm Maximum depth value (in mm) to include in the point cloud. Points further away are discarded.
     * @param gradientThresholdFactor Factor multiplied by the max observed depth to determine the gradient threshold for filtering.
     * @return True if generation was successful (even if 0 points were generated), false otherwise.
     */
    bool generate(
        const cv::Mat& depthMap,
        const cv::Mat& colorImageInput,
        std::shared_ptr<SmartScope::Core::CameraCorrectionManager> correctionManager,
        std::vector<QVector3D>& outPoints,
        std::vector<QVector3D>& outColors,
        std::vector<cv::Point2i>& outPixelCoords,
        int step = 2, // Default sampling step
        float maxDepthMm = 10000.0f, // Default max depth (10 meters)
        float gradientThresholdFactor = 0.05f // Default gradient filter factor (5% of max depth)
    );

private:
    // Private members if needed, e.g., configuration settings
};

} // namespace SmartScope::App::Measurement

#endif // SMART_SCOPE_POINT_CLOUD_GENERATOR_H 