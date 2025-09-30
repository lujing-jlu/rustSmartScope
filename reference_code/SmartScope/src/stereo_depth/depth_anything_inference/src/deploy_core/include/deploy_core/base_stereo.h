/*
 * @Description:
 * @Author: Teddywesside 18852056629@163.com
 * @Date: 2024-11-25 18:38:34
 * @LastEditTime: 2024-12-02 19:03:30
 * @FilePath: /easy_deploy/deploy_core/include/deploy_core/base_sam.h
 */
#ifndef __EASY_DEPLOY_BASE_STEREO_H
#define __EASY_DEPLOY_BASE_STEREO_H

#include "deploy_core/base_infer_core.h"
#include "deploy_core/common_defination.h"

#include <opencv2/opencv.hpp>

namespace stereo {

struct StereoPipelinePackage : public async_pipeline::IPipelinePackage {
  // the wrapped pipeline image data
  std::shared_ptr<async_pipeline::IPipelineImageData> left_image_data;
  std::shared_ptr<async_pipeline::IPipelineImageData> right_image_data;
  // confidence used in postprocess
  float conf_thresh;
  // record the transform factor during image preprocess
  float transform_scale;

  //
  cv::Mat disp;

  // maintain the blobs buffer instance
  std::shared_ptr<inference_core::IBlobsBuffer> infer_buffer;

  // override from `IPipelinePakcage`, to provide the blobs buffer to inference_core
  std::shared_ptr<inference_core::IBlobsBuffer> GetInferBuffer() override
  {
    if (infer_buffer == nullptr)
    {
      LOG(ERROR) << "[DetectionPipelinePackage] returned nullptr of infer_buffer!!!";
    }
    return infer_buffer;
  }
};

/**
 * @brief A functor to generate sam results from `SamPipelinePackage`. Used in async pipeline.
 *
 */
class StereoGenResultType {
public:
  cv::Mat operator()(const std::shared_ptr<async_pipeline::IPipelinePackage> &package)
  {
    auto stereo_package = std::dynamic_pointer_cast<StereoPipelinePackage>(package);
    if (stereo_package == nullptr)
    {
      LOG(ERROR) << "[StereoGenResultType] Got INVALID package ptr!!!";
      return {};
    }
    return std::move(stereo_package->disp);
  }
};

class BaseStereoMatchingModel : public async_pipeline::BaseAsyncPipeline<cv::Mat, StereoGenResultType> {
protected:
  using ParsingType = std::shared_ptr<async_pipeline::IPipelinePackage>;

  BaseStereoMatchingModel(const std::shared_ptr<inference_core::BaseInferCore> &inference_core);

public:
  bool ComputeDisp(const cv::Mat &left_image, const cv::Mat &right_image, cv::Mat &disp_output);

  [[nodiscard]] std::future<cv::Mat> ComputeDispAsync(const cv::Mat &left_image, const cv::Mat &right_image);

protected:
  virtual bool PreProcess(std::shared_ptr<async_pipeline::IPipelinePackage> pipeline_unit) = 0;

  virtual bool PostProcess(std::shared_ptr<async_pipeline::IPipelinePackage> pipeline_unit) = 0;

private:
  using BaseAsyncPipeline::PushPipeline;

protected:
  std::shared_ptr<inference_core::BaseInferCore> inference_core_;

  static const std::string stereo_pipeline_name_;
};



struct MonoStereoPipelinePackage : public async_pipeline::IPipelinePackage {
  // the wrapped pipeline image data
  std::shared_ptr<async_pipeline::IPipelineImageData> input_image_data;
  // record the transform factor during image preprocess
  float transform_scale;

  //
  cv::Mat depth;

  // maintain the blobs buffer instance
  std::shared_ptr<inference_core::IBlobsBuffer> infer_buffer;

  // override from `IPipelinePakcage`, to provide the blobs buffer to inference_core
  std::shared_ptr<inference_core::IBlobsBuffer> GetInferBuffer() override
  {
    if (infer_buffer == nullptr)
    {
      LOG(ERROR) << "[MonoStereoPipelinePackage] returned nullptr of infer_buffer!!!";
    }
    return infer_buffer;
  }
};

/**
 * @brief A functor to generate sam results from `SamPipelinePackage`. Used in async pipeline.
 *
 */
class MonoStereoGenResultType {
public:
  cv::Mat operator()(const std::shared_ptr<async_pipeline::IPipelinePackage> &package)
  {
    auto stereo_package = std::dynamic_pointer_cast<MonoStereoPipelinePackage>(package);
    if (stereo_package == nullptr)
    {
      LOG(ERROR) << "[MonoStereoGenResultType] Got INVALID package ptr!!!";
      return {};
    }
    return std::move(stereo_package->depth);
  }
};

class BaseMonoStereoModel : public async_pipeline::BaseAsyncPipeline<cv::Mat, MonoStereoGenResultType> {
protected:
  using ParsingType = std::shared_ptr<async_pipeline::IPipelinePackage>;

  BaseMonoStereoModel(const std::shared_ptr<inference_core::BaseInferCore> &inference_core);

public:
  bool ComputeDepth(const cv::Mat &input_image, cv::Mat &depth_output);

  [[nodiscard]] std::future<cv::Mat> ComputeDepthAsync(const cv::Mat &input_image);

protected:
  virtual bool PreProcess(std::shared_ptr<async_pipeline::IPipelinePackage> pipeline_unit) = 0;

  virtual bool PostProcess(std::shared_ptr<async_pipeline::IPipelinePackage> pipeline_unit) = 0;

private:
  using BaseAsyncPipeline::PushPipeline;

protected:
  std::shared_ptr<inference_core::BaseInferCore> inference_core_;

  static const std::string mono_stereo_pipeline_name_;
};

} // namespace sam

#endif