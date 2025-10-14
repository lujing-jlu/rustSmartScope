// C wrapper for RKNN inference functions to ensure C linkage for Rust FFI

#include "../include/yolov8.h"
#include "../include/postprocess.h"

extern "C" {

int init_yolov8_model_wrapper(const char* model_path, rknn_app_context_t* app_ctx) {
    return init_yolov8_model(model_path, app_ctx);
}

int release_yolov8_model_wrapper(rknn_app_context_t* app_ctx) {
    return release_yolov8_model(app_ctx);
}

int inference_yolov8_model_wrapper(rknn_app_context_t* app_ctx, image_buffer_t* img, object_detect_result_list* od_results) {
    return inference_yolov8_model(app_ctx, img, od_results);
}

int init_post_process_wrapper() {
    return init_post_process();
}

void deinit_post_process_wrapper() {
    deinit_post_process();
}

char* coco_cls_to_name_wrapper(int cls_id) {
    return coco_cls_to_name(cls_id);
}

// Separate step wrappers for benchmarking
int yolov8_preprocess_wrapper(rknn_app_context_t* app_ctx, image_buffer_t* img, image_buffer_t* dst_img, letterbox_t* letter_box) {
    return yolov8_preprocess(app_ctx, img, dst_img, letter_box);
}

int yolov8_inference_wrapper(rknn_app_context_t* app_ctx, image_buffer_t* dst_img, rknn_output* outputs) {
    return yolov8_inference(app_ctx, dst_img, outputs);
}

int yolov8_postprocess_wrapper(rknn_app_context_t* app_ctx, rknn_output* outputs, letterbox_t* letter_box, object_detect_result_list* od_results) {
    return yolov8_postprocess(app_ctx, outputs, letter_box, od_results);
}

void yolov8_release_outputs_wrapper(rknn_app_context_t* app_ctx, rknn_output* outputs) {
    yolov8_release_outputs(app_ctx, outputs);
}

} // extern "C"
