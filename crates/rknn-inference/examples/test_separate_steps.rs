use rknn_inference::{ImageBuffer, ImageFormat};
use std::ffi::CString;

// Import FFI types
use rknn_inference::ffi::{
    FfiImageBuffer, LetterBox, ObjectDetectResultList, RknnAppContext, RknnOutput,
};

fn main() {
    println!("Testing separate step functions...\n");

    let model_path = "models/yolov8m.rknn";
    let image_path = "tests/test.jpg";

    println!("1. Loading model...");
    let model_path_c = CString::new(model_path).unwrap();
    let mut app_ctx = RknnAppContext::default();

    unsafe {
        let ret = rknn_inference::ffi::init_yolov8_model(model_path_c.as_ptr(), &mut app_ctx);
        if ret < 0 {
            panic!("Failed to load model: {}", ret);
        }
        let ret = rknn_inference::ffi::init_post_process();
        if ret < 0 {
            panic!("Failed to init post process: {}", ret);
        }
    }
    println!("✓ Model loaded: {}x{}\n", app_ctx.model_width, app_ctx.model_height);

    println!("2. Loading image...");
    let img = image::open(image_path).expect("Failed to load image");
    let img = img.to_rgb8();
    let image_buffer = ImageBuffer {
        width: img.width() as i32,
        height: img.height() as i32,
        format: ImageFormat::Rgb888,
        data: img.into_raw(),
    };
    println!("✓ Image loaded: {}x{}\n", image_buffer.width, image_buffer.height);

    println!("3. Testing preprocess...");
    let mut ffi_img = FfiImageBuffer::from_image_buffer(&image_buffer);
    let mut dst_img = rknn_inference::ffi::CImageBuffer {
        width: 0,
        height: 0,
        width_stride: 0,
        height_stride: 0,
        format: 0,
        virt_addr: std::ptr::null_mut(),
        size: 0,
        fd: -1,
    };
    let mut letter_box = LetterBox::default();

    unsafe {
        let ret = rknn_inference::ffi::yolov8_preprocess(
            &mut app_ctx,
            ffi_img.as_ptr(),
            &mut dst_img,
            &mut letter_box,
        );
        if ret < 0 {
            panic!("Preprocess failed: {}", ret);
        }
    }
    println!("✓ Preprocess done");
    println!("  dst_img: {}x{}, size={}", dst_img.width, dst_img.height, dst_img.size);
    println!("  letter_box: x_pad={}, y_pad={}, scale={}\n", letter_box.x_pad, letter_box.y_pad, letter_box.scale);

    println!("4. Testing inference...");
    let mut outputs: Vec<RknnOutput> = (0..app_ctx.io_num.n_output)
        .map(|_| RknnOutput::default())
        .collect();

    println!("  Allocated {} output buffers", outputs.len());

    unsafe {
        let ret = rknn_inference::ffi::yolov8_inference(
            &mut app_ctx,
            &mut dst_img,
            outputs.as_mut_ptr(),
        );
        if ret < 0 {
            panic!("Inference failed: {}", ret);
        }
    }
    println!("✓ Inference done\n");

    println!("5. Testing postprocess...");
    let mut od_results = ObjectDetectResultList::default();

    unsafe {
        let ret = rknn_inference::ffi::yolov8_postprocess(
            &mut app_ctx,
            outputs.as_mut_ptr(),
            &mut letter_box,
            &mut od_results,
        );
        if ret < 0 {
            panic!("Postprocess failed: {}", ret);
        }
    }
    println!("✓ Postprocess done");
    println!("  Detected {} objects\n", od_results.count);

    // Cleanup
    unsafe {
        rknn_inference::ffi::yolov8_release_outputs(&mut app_ctx, outputs.as_mut_ptr());
        if !dst_img.virt_addr.is_null() {
            libc::free(dst_img.virt_addr as *mut libc::c_void);
        }
        rknn_inference::ffi::release_yolov8_model(&mut app_ctx);
        rknn_inference::ffi::deinit_post_process();
    }

    println!("✓ All steps completed successfully!");
}
