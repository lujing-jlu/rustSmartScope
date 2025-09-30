#include "point_cloud_utils.hpp"

#include <iostream> // For std::cout, std::cerr
#include <pcl/common/common.h>
#include <pcl/common/transforms.h>
#include <pcl/features/normal_3d.h> // Might be needed indirectly or for future extensions
#include <pcl/filters/filter.h>
#include <pcl/filters/voxel_grid.h> // Example filter
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h> // Example IO
#include <pcl/common/pca.h> // For PCA
#include <Eigen/Core> // For Eigen matrices
#include <pcl/console/time.h> // For TicToc

namespace PointCloudUtils {

bool optimizePointCloud(
    const std::vector<QVector3D>& pointsIn,
    const std::vector<QVector3D>& colorsIn,
    std::vector<QVector3D>& pointsOut,
    std::vector<QVector3D>& colorsOut)
{
    pcl::console::TicToc timer;
    timer.tic();

    pointsOut.clear();
    colorsOut.clear();

    if (pointsIn.empty() || pointsIn.size() != colorsIn.size()) {
        std::cerr << "[optimizePointCloud] Error: Input points empty or points/colors size mismatch." << std::endl;
        return false;
    }

    // 1. Convert std::vector<QVector3D> to pcl::PointCloud<pcl::PointXYZRGB>
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    cloud->points.resize(pointsIn.size());

    for (size_t i = 0; i < pointsIn.size(); ++i) {
        cloud->points[i].x = pointsIn[i].x();
        cloud->points[i].y = pointsIn[i].y();
        cloud->points[i].z = pointsIn[i].z();
        // Convert color from 0.0-1.0 float (QVector3D) to 0-255 uint8_t (PCL RGB)
        cloud->points[i].r = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, colorsIn[i].x() * 255.0f)));
        cloud->points[i].g = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, colorsIn[i].y() * 255.0f)));
        cloud->points[i].b = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, colorsIn[i].z() * 255.0f)));
    }
    cloud->width = static_cast<uint32_t>(cloud->points.size());
    cloud->height = 1;
    cloud->is_dense = false; // Assume could have NaN/inf, though input format might guarantee density

    std::cout << "[optimizePointCloud] Converted to PCL cloud: " << cloud->points.size() << " points." << std::endl;

    // --- Start PCA Optimization Logic (copied and adapted from StereoInference) ---

    if (cloud->points.size() < 3) {
        std::cerr << "[optimizePointCloud] Error: Point cloud has less than 3 points, cannot perform PCA." << std::endl;
        // Still return true but with original data? Or false? Let's return false.
        return false;
    }

    // 2. Calculate PCA
    pcl::PCA<pcl::PointXYZRGB> pca(true); // Use true to compute eigenvectors/values directly
    pca.setInputCloud(cloud);

    // 3. Get Centroid (Mean)
    Eigen::Vector4f centroid4f;
    centroid4f = pca.getMean(); // Correct: Assign the returned reference
    Eigen::Vector3f centroid3f = centroid4f.head<3>();
    std::cout << "[optimizePointCloud] Point cloud centroid: (" << centroid3f[0] << ", " << centroid3f[1] << ", " << centroid3f[2] << ")" << std::endl;

    // 4. Get Eigenvectors and Eigenvalues
    Eigen::Matrix3f eigenvectors = pca.getEigenVectors();
    Eigen::Vector3f eigenvalues = pca.getEigenValues();

    std::cout << "[optimizePointCloud] PCA Eigenvalues: (" << eigenvalues[0] << ", " << eigenvalues[1] << ", " << eigenvalues[2] << ")" << std::endl;
    std::cout << "[optimizePointCloud] PC1 (X-axis target): (" << eigenvectors(0,0) << ", " << eigenvectors(1,0) << ", " << eigenvectors(2,0) << ")" << std::endl;
    std::cout << "[optimizePointCloud] PC2 (Y-axis target): (" << eigenvectors(0,1) << ", " << eigenvectors(1,1) << ", " << eigenvectors(2,1) << ")" << std::endl;
    std::cout << "[optimizePointCloud] PC3 (Z-axis target): (" << eigenvectors(0,2) << ", " << eigenvectors(1,2) << ", " << eigenvectors(2,2) << ")" << std::endl;

    // 5. Build rotation matrix to align PCA axes with coordinate axes (PC1->X, PC2->Y, PC3->Z)
    Eigen::Matrix3f rotation = eigenvectors.transpose();
    std::cout << "[optimizePointCloud] Rotation matrix (PC1->X, PC2->Y, PC3->Z, before det check):\\n" << rotation << std::endl;

    // Determinant check (currently commented out as per previous step)
    /*
    if (rotation.determinant() < 0) {
        std::cout << "[optimizePointCloud] PCA result is left-handed, flipping Z axis (PC3) for right-handed alignment..." << std::endl;
        rotation.row(2) *= -1;
    }
    std::cout << "[optimizePointCloud] Rotation matrix (after determinant check):\\n" << rotation << std::endl;
    */
    std::cout << "[optimizePointCloud] Rotation matrix (using eigenvectors.transpose(), det check skipped):\\n" << rotation << std::endl;

    // 6. Build the 4x4 transformation matrix: T = Translation(-centroid) * Rotation
    Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
    transform.block<3,3>(0,0) = rotation;
    transform.block<3,1>(0,3) = - (rotation * centroid3f);
    std::cout << "[optimizePointCloud] Final 4x4 Transform matrix:\\n" << transform << std::endl;

    // 7. Apply the transformation
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr transformed_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::transformPointCloud(*cloud, *transformed_cloud, transform);

    // --- End PCA Optimization Logic ---

    // 8. Convert transformed PCL cloud back to std::vector<QVector3D>
    pointsOut.reserve(transformed_cloud->points.size());
    colorsOut.reserve(transformed_cloud->points.size());

    for (const auto& point : transformed_cloud->points) {
        pointsOut.emplace_back(point.x, point.y, point.z);
        // Convert color back from 0-255 uint8_t to 0.0-1.0 float
        colorsOut.emplace_back(
            static_cast<float>(point.r) / 255.0f,
            static_cast<float>(point.g) / 255.0f,
            static_cast<float>(point.b) / 255.0f
        );
    }

    std::cout << "[optimizePointCloud] Optimization finished. Output points: " << pointsOut.size() << ". Time: " << timer.toc() << " ms" << std::endl;
    return true;
}

} // namespace PointCloudUtils
