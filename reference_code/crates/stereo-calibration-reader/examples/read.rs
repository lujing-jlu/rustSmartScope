use stereo_calibration_reader::load_from_dir;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let dir = args.get(1).cloned().unwrap_or_else(|| "reference_code/SmartScope/camera_parameters".to_string());
    match load_from_dir(&dir) {
        Ok(calib) => {
            println!("left K: {:?}", calib.left.camera_matrix);
            println!("left dist: {:?}", calib.left.dist_coeffs);
            println!("right K: {:?}", calib.right.camera_matrix);
            println!("right dist: {:?}", calib.right.dist_coeffs);
            println!("R: {:?}", calib.extrinsics.rotation);
            println!("T: {:?}", calib.extrinsics.translation);
        }
        Err(e) => {
            eprintln!("error: {}", e);
            std::process::exit(1);
        }
    }
}
