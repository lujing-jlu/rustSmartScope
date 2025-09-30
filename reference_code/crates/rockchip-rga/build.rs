use pkg_config;
use std::env;
use std::path::PathBuf;

fn main() {
    // 查找librga库
    pkg_config::probe_library("librga").expect("Failed to find librga");

    // 设置链接库
    println!("cargo:rustc-link-lib=rga");

    // 获取项目根目录
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let include_dir = format!("{}/include", manifest_dir);

    // 生成绑定
    let bindings = bindgen::Builder::default()
        .header(format!("{}/rga/im2d.h", include_dir))
        .header(format!("{}/rga/im2d_common.h", include_dir))
        .header(format!("{}/rga/im2d_type.h", include_dir))
        .header(format!("{}/rga/im2d_buffer.h", include_dir))
        .header(format!("{}/rga/im2d_single.h", include_dir))
        .header(format!("{}/rga/im2d_task.h", include_dir))
        .header(format!("{}/rga/im2d_mpi.h", include_dir))
        .header(format!("{}/rga/RgaApi.h", include_dir))

        .clang_arg(format!("-I{}/rga", include_dir))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
