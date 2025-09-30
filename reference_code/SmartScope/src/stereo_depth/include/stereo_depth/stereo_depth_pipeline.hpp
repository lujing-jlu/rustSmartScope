#pragma once

#include <opencv2/opencv.hpp>
#include <string>

namespace stereo_depth {

class StereoDepthPipeline {
public:
    struct Options {
        // 输入图像在校正前需要顺时针旋转的角度（与标定一致）
        int rotate_input_deg = 90;
        // SGBM 参数
        int min_disparity = 0;
        int num_disparities = 16 * 8; // 必须是16倍数
        int block_size = 5;           // 奇数
        int uniqueness_ratio = 10;
        int speckle_window = 100;
        int speckle_range = 32;
        int prefilter_cap = 63;
        int disp12_max_diff = 1;
    };

    explicit StereoDepthPipeline(const std::string &camera_param_dir);
    StereoDepthPipeline(const std::string &camera_param_dir, const Options &opts);

    // 处理一帧，返回 CV_32F 视差（像素）
    // 注意：输入为原始左右图（未旋转/未校正），内部会按标定旋转与校正
    cv::Mat computeDisparity(const cv::Mat &left_raw, const cv::Mat &right_raw);

    // 获取 Q 矩阵（在首帧初始化后可用）
    cv::Mat getQ() const { return Q_.clone(); }

    // 新增：根据Q矩阵将视差转换为深度(mm)。返回true表示成功。
    bool disparityToDepthMm(const cv::Mat &disparity, cv::Mat &depthMm) const;

    // 新增：输出校正后的左右图（BGR），内部包含旋转
    bool rectifyLeftRight(const cv::Mat &left_raw, const cv::Mat &right_raw,
                          cv::Mat &left_rect, cv::Mat &right_rect) const;

private:
    // 相机参数读取
    bool readIntrinsics(const std::string &path, cv::Mat &K, cv::Mat &D);
    bool readRotTrans(const std::string &path, cv::Mat &R, cv::Mat &T);

    // 初始化（在获得第一帧尺寸后）
    void ensureInitialized(const cv::Size &input_size);

    std::string param_dir_;
    Options opts_{};

    // 标定参数与映射
    cv::Mat K0_, D0_, K1_, D1_, R_, T_, R1_, R2_, P1_, P2_, Q_;
    cv::Rect roi1_, roi2_;
    cv::Mat map1x_, map1y_, map2x_, map2y_;
    cv::Ptr<cv::StereoSGBM> sgbm_;

    bool initialized_ = false;
    cv::Size image_size_;
};

} // namespace stereo_depth 