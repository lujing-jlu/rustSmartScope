use pkg_config;
use std::env;
use std::path::PathBuf;

fn main() {
    // 查找 MPP 库（rockchip_mpp 或 mpp）
    let lib = pkg_config::probe_library("rockchip_mpp")
        .or_else(|_| pkg_config::probe_library("mpp"))
        .expect("Failed to find rockchip mpp (pkg-config: rockchip_mpp or mpp)");

    // 获取项目根目录
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let include_dir = format!("{}/include", manifest_dir);

    // 生成绑定
    let bindings = bindgen::Builder::default()
        .header(format!("{}/rockchip/rk_mpi.h", include_dir))
        .header(format!("{}/rockchip/mpp_frame.h", include_dir))
        .header(format!("{}/rockchip/mpp_packet.h", include_dir))
        .header(format!("{}/rockchip/mpp_buffer.h", include_dir))
        .header(format!("{}/rockchip/mpp_task.h", include_dir))
        .header(format!("{}/rockchip/mpp_meta.h", include_dir))
        .header(format!("{}/rockchip/mpp_err.h", include_dir))
        .header(format!("{}/rockchip/rk_type.h", include_dir))
        .clang_arg(format!("-I{}/rockchip", include_dir))
        // 同时把系统头文件包含路径也加进去，避免头文件版本不匹配
        .clang_args(
            lib.include_paths
                .iter()
                .map(|p| format!("-I{}", p.display())),
        )
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
