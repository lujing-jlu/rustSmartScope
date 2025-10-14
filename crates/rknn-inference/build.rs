use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let lib_path = PathBuf::from(&manifest_dir).join("lib");
    let include_path = PathBuf::from(&manifest_dir).join("include");

    // Link RKNN libraries
    println!("cargo:rustc-link-search=native={}", lib_path.display());
    println!("cargo:rustc-link-lib=dylib=rknnrt");
    println!("cargo:rustc-link-lib=dylib=rga");

    // Build C/C++ wrapper code
    let mut build = cc::Build::new();
    build
        .cpp(true)
        .file("csrc/yolov8.cc")
        .file("csrc/postprocess.cc")
        .file("csrc/image_utils.cpp")  // Changed from .c to .cpp
        .file("csrc/file_utils.cpp")    // Changed from .c to .cpp
        .file("csrc/rknn_wrapper.cpp")
        .include(&include_path)
        .include(include_path.join("utils"))
        .include(include_path.join("rga"))
        .flag("-std=c++11")
        .flag("-w"); // Suppress warnings from C++ code

    // Add library paths for linking
    build.flag(&format!("-L{}", lib_path.display()));

    build.compile("yolov8_wrapper");

    // Link C++ standard library
    println!("cargo:rustc-link-lib=dylib=stdc++");

    // Tell cargo to invalidate the built crate whenever the wrapper changes
    println!("cargo:rerun-if-changed=csrc/");
    println!("cargo:rerun-if-changed=include/");
}
