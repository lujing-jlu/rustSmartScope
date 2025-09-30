#include "stereo_depth/stereo_depth_pipeline.hpp"
#include <fstream>
#include <sstream>

namespace stereo_depth {

using cv::Mat; using cv::Size; using std::string;

static bool readMatrix3x3(std::istream &in, Mat &M) {
    double a00,a01,a02,a10,a11,a12,a20,a21,a22;
    if(!(in>>a00>>a01>>a02)) return false;
    if(!(in>>a10>>a11>>a12)) return false;
    if(!(in>>a20>>a21>>a22)) return false;
    M = (cv::Mat_<double>(3,3) << a00,a01,a02,a10,a11,a12,a20,a21,a22);
    return true;
}

StereoDepthPipeline::StereoDepthPipeline(const string &camera_param_dir)
    : param_dir_(camera_param_dir) {}

StereoDepthPipeline::StereoDepthPipeline(const string &camera_param_dir, const Options &opts)
    : param_dir_(camera_param_dir), opts_(opts) {}

bool StereoDepthPipeline::readIntrinsics(const string &path, Mat &K, Mat &D) {
    std::ifstream in(path);
    if(!in.is_open()) return false;
    string tag; std::getline(in, tag); // intrinsic:
    if(!readMatrix3x3(in, K)) return false;
    std::getline(in, tag); // consume endline
    std::getline(in, tag); // distortion:
    std::getline(in, tag);
    std::stringstream ss(tag);
    std::vector<double> coeffs; double t;
    while(ss>>t) coeffs.push_back(t);
    if(coeffs.empty()) { D = Mat::zeros(1,5,CV_64F); }
    else {
        D = Mat(1,(int)coeffs.size(),CV_64F);
        for(int i=0;i<(int)coeffs.size();++i) D.at<double>(0,i)=coeffs[i];
    }
    return true;
}

bool StereoDepthPipeline::readRotTrans(const string &path, Mat &R, Mat &T) {
    std::ifstream in(path);
    if(!in.is_open()) return false;
    string tag; std::getline(in, tag); // R:
    if(!readMatrix3x3(in, R)) return false;
    std::getline(in, tag); // T:
    double tx=0,ty=0,tz=0;
    in >> tx; in >> ty; in >> tz;
    T = (cv::Mat_<double>(3,1) << tx,ty,tz);
    return true;
}

void StereoDepthPipeline::ensureInitialized(const Size &input_size) {
    if (initialized_) return;
    Size rotated = input_size;
    if ((opts_.rotate_input_deg % 180) != 0) rotated = Size(input_size.height, input_size.width);

    if (!readIntrinsics(param_dir_+"/camera0_intrinsics.dat", K0_, D0_))
        throw std::runtime_error("读取 camera0_intrinsics.dat 失败");
    if (!readIntrinsics(param_dir_+"/camera1_intrinsics.dat", K1_, D1_))
        throw std::runtime_error("读取 camera1_intrinsics.dat 失败");
    if (!readRotTrans(param_dir_+"/camera1_rot_trans.dat", R_, T_))
        throw std::runtime_error("读取 camera1_rot_trans.dat 失败");

    cv::stereoRectify(K0_, D0_, K1_, D1_, rotated, R_, T_, R1_, R2_, P1_, P2_, Q_,
                      cv::CALIB_ZERO_DISPARITY, -1.0, rotated, &roi1_, &roi2_);
    cv::initUndistortRectifyMap(K0_, D0_, R1_, P1_, rotated, CV_32FC1, map1x_, map1y_);
    cv::initUndistortRectifyMap(K1_, D1_, R2_, P2_, rotated, CV_32FC1, map2x_, map2y_);

    sgbm_ = cv::StereoSGBM::create(opts_.min_disparity, opts_.num_disparities, opts_.block_size);
    sgbm_->setUniquenessRatio(opts_.uniqueness_ratio);
    sgbm_->setSpeckleWindowSize(opts_.speckle_window);
    sgbm_->setSpeckleRange(opts_.speckle_range);
    sgbm_->setPreFilterCap(opts_.prefilter_cap);
    sgbm_->setDisp12MaxDiff(opts_.disp12_max_diff);
    sgbm_->setMode(cv::StereoSGBM::MODE_SGBM_3WAY);

    image_size_ = rotated;
    initialized_ = true;
}

cv::Mat StereoDepthPipeline::computeDisparity(const Mat &left_raw, const Mat &right_raw) {
    if (left_raw.empty() || right_raw.empty()) return Mat();
    ensureInitialized(left_raw.size());

    Mat left_r, right_r;
    if (opts_.rotate_input_deg % 360 != 0) {
        int deg = (opts_.rotate_input_deg % 360 + 360) % 360;
        if (deg == 90) { cv::rotate(left_raw, left_r, cv::ROTATE_90_CLOCKWISE); cv::rotate(right_raw, right_r, cv::ROTATE_90_CLOCKWISE); }
        else if (deg == 180) { cv::rotate(left_raw, left_r, cv::ROTATE_180); cv::rotate(right_raw, right_r, cv::ROTATE_180); }
        else if (deg == 270) { cv::rotate(left_raw, left_r, cv::ROTATE_90_COUNTERCLOCKWISE); cv::rotate(right_raw, right_r, cv::ROTATE_90_COUNTERCLOCKWISE); }
        else { left_r = left_raw; right_r = right_raw; }
    } else { left_r = left_raw; right_r = right_raw; }

    Mat left_rect, right_rect; 
    cv::remap(left_r, left_rect, map1x_, map1y_, cv::INTER_LINEAR);
    cv::remap(right_r, right_rect, map2x_, map2y_, cv::INTER_LINEAR);
    Mat grayL, grayR; cv::cvtColor(left_rect, grayL, cv::COLOR_BGR2GRAY); cv::cvtColor(right_rect, grayR, cv::COLOR_BGR2GRAY);
    Mat disp16S; sgbm_->compute(grayL, grayR, disp16S);
    Mat disp32F; disp16S.convertTo(disp32F, CV_32F, 1.0/16.0);
    return disp32F;
}

bool StereoDepthPipeline::disparityToDepthMm(const Mat &disparity, Mat &depthMm) const {
    if (disparity.empty() || Q_.empty()) return false;
    CV_Assert(disparity.type() == CV_32F);
    Mat xyz;
    cv::reprojectImageTo3D(disparity, xyz, Q_, /*handleMissingValues=*/true);
    std::vector<Mat> ch; cv::split(xyz, ch);
    if (ch.size() < 3) return false;
    depthMm = ch[2].clone();
    cv::patchNaNs(depthMm, 0.0);
    depthMm.setTo(0, depthMm <= 0);
    return true;
}

bool StereoDepthPipeline::rectifyLeftRight(const Mat &left_raw, const Mat &right_raw,
                                           Mat &left_rect, Mat &right_rect) const {
    if (left_raw.empty() || right_raw.empty() || map1x_.empty() || map2x_.empty()) return false;
    Mat left_r, right_r;
    if (opts_.rotate_input_deg % 360 != 0) {
        int deg = (opts_.rotate_input_deg % 360 + 360) % 360;
        if (deg == 90) { cv::rotate(left_raw, left_r, cv::ROTATE_90_CLOCKWISE); cv::rotate(right_raw, right_r, cv::ROTATE_90_CLOCKWISE); }
        else if (deg == 180) { cv::rotate(left_raw, left_r, cv::ROTATE_180); cv::rotate(right_raw, right_r, cv::ROTATE_180); }
        else if (deg == 270) { cv::rotate(left_raw, left_r, cv::ROTATE_90_COUNTERCLOCKWISE); cv::rotate(right_raw, right_r, cv::ROTATE_90_COUNTERCLOCKWISE); }
        else { left_r = left_raw; right_r = right_raw; }
    } else { left_r = left_raw; right_r = right_raw; }
    cv::remap(left_r, left_rect, map1x_, map1y_, cv::INTER_LINEAR);
    cv::remap(right_r, right_rect, map2x_, map2y_, cv::INTER_LINEAR);
    return true;
}

} // namespace stereo_depth 