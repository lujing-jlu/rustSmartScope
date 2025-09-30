#ifndef POINT_CLOUD_UTILS_HPP
#define POINT_CLOUD_UTILS_HPP

#include <vector>
#include <QVector3D> // Using QVector3D as per project convention

// Forward declare PCL types to reduce header dependencies if possible,
// but PointXYZRGB might be needed directly depending on usage.
// For simplicity here, we include the necessary PCL headers directly.
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

namespace PointCloudUtils {

/**
 * @brief 使用PCA优化点云，使其主方向与坐标轴对齐，并将质心移动到原点。
 *        目标是将方差最小的方向 (PC3) 对齐到 Z 轴。
 * @param pointsIn 输入的点云坐标 (Qt 3D 坐标系)。
 * @param colorsIn 输入的点云颜色 (RGB, 0.0-1.0)。尺寸必须与 pointsIn 相同。
 * @param pointsOut 输出优化后的点云坐标。
 * @param colorsOut 输出优化后的点云颜色 (顺序与 pointsOut 对应)。
 * @return true 如果优化成功, false 如果输入无效或 PCA 失败。
 */
bool optimizePointCloud(
    const std::vector<QVector3D>& pointsIn,
    const std::vector<QVector3D>& colorsIn,
    std::vector<QVector3D>& pointsOut,
    std::vector<QVector3D>& colorsOut
);

// 未来可以添加其他点云处理工具函数...

} // namespace PointCloudUtils

#endif // POINT_CLOUD_UTILS_HPP
