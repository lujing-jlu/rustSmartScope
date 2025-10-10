//! Example: Camera parameter loader
//!
//! This example demonstrates how to use the camera-correction library
//! to load and validate camera parameters.

use camera_correction::{CameraParameters, StereoParameters};
use clap::Parser;
use log::info;

#[derive(Parser)]
#[command(name = "parameter_loader")]
#[command(about = "Camera parameter loader example")]
struct Args {
    /// Camera parameters directory
    #[arg(short, long, default_value = "./camera_parameters")]
    params_dir: String,

    /// Enable verbose logging
    #[arg(short, long)]
    verbose: bool,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Parse command line arguments
    let args = Args::parse();

    // Initialize logging
    if args.verbose {
        env_logger::init();
    } else {
        env_logger::init_from_env(env_logger::Env::default().default_filter_or("info"));
    }

    info!("Starting camera parameter loader example");

    // Load camera parameters
    info!("Loading camera parameters from: {}", args.params_dir);
    let params = CameraParameters::from_directory(&args.params_dir)?;

    // Get stereo parameters
    let stereo = params.get_stereo_parameters()?;

    // Display loaded parameters
    println!("\n=== Camera Parameters ===");
    
    // Left camera
    println!("\nLeft Camera:");
    println!("  Matrix:");
    println!("    fx: {:.6}", stereo.left_intrinsics.matrix.fx);
    println!("    fy: {:.6}", stereo.left_intrinsics.matrix.fy);
    println!("    cx: {:.6}", stereo.left_intrinsics.matrix.cx);
    println!("    cy: {:.6}", stereo.left_intrinsics.matrix.cy);
    println!("  Distortion:");
    println!("    k1: {:.6}", stereo.left_intrinsics.distortion.k1);
    println!("    k2: {:.6}", stereo.left_intrinsics.distortion.k2);
    println!("    p1: {:.6}", stereo.left_intrinsics.distortion.p1);
    println!("    p2: {:.6}", stereo.left_intrinsics.distortion.p2);
    println!("    k3: {:.6}", stereo.left_intrinsics.distortion.k3);

    // Right camera
    println!("\nRight Camera:");
    println!("  Matrix:");
    println!("    fx: {:.6}", stereo.right_intrinsics.matrix.fx);
    println!("    fy: {:.6}", stereo.right_intrinsics.matrix.fy);
    println!("    cx: {:.6}", stereo.right_intrinsics.matrix.cx);
    println!("    cy: {:.6}", stereo.right_intrinsics.matrix.cy);
    println!("  Distortion:");
    println!("    k1: {:.6}", stereo.right_intrinsics.distortion.k1);
    println!("    k2: {:.6}", stereo.right_intrinsics.distortion.k2);
    println!("    p1: {:.6}", stereo.right_intrinsics.distortion.p1);
    println!("    p2: {:.6}", stereo.right_intrinsics.distortion.p2);
    println!("    k3: {:.6}", stereo.right_intrinsics.distortion.k3);

    // Extrinsics
    println!("\nRight Camera Extrinsics (relative to left):");
    println!("  Rotation Matrix:");
    for (i, row) in stereo.right_extrinsics.rotation.iter().enumerate() {
        println!("    [{:>8.6}, {:>8.6}, {:>8.6}]", row[0], row[1], row[2]);
    }
    println!("  Translation Vector:");
    println!("    tx: {:.6}", stereo.right_extrinsics.translation[0]);
    println!("    ty: {:.6}", stereo.right_extrinsics.translation[1]);
    println!("    tz: {:.6}", stereo.right_extrinsics.translation[2]);

    // Validate parameters
    info!("Validating camera parameters...");
    stereo.validate()?;
    info!("Camera parameters validation passed!");

    // Display matrix representations
    println!("\n=== Matrix Representations ===");
    
    println!("\nLeft Camera Matrix:");
    let left_matrix = stereo.left_intrinsics.matrix.to_matrix();
    for row in &left_matrix {
        println!("  [{:>8.6}, {:>8.6}, {:>8.6}]", row[0], row[1], row[2]);
    }

    println!("\nLeft Camera Distortion Coefficients:");
    let left_dist = stereo.left_intrinsics.distortion.to_vector();
    println!("  [{:>8.6}, {:>8.6}, {:>8.6}, {:>8.6}, {:>8.6}]", 
        left_dist[0], left_dist[1], left_dist[2], left_dist[3], left_dist[4]);

    println!("\n=== Summary ===");
    println!("Successfully loaded camera parameters from: {}", args.params_dir);
    println!("Left camera focal length: {:.2} x {:.2}", 
        stereo.left_intrinsics.matrix.fx, stereo.left_intrinsics.matrix.fy);
    println!("Right camera focal length: {:.2} x {:.2}", 
        stereo.right_intrinsics.matrix.fx, stereo.right_intrinsics.matrix.fy);
    println!("Baseline: {:.2} mm", stereo.right_extrinsics.translation[0].abs());

    info!("Camera parameter loader example completed successfully");
    Ok(())
}
