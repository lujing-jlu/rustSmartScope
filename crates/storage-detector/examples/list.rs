fn main() -> Result<(), Box<dyn std::error::Error>> {
    let storages = storage_detector::list()?;

    if storages.is_empty() {
        println!("No external storages detected.");
    } else {
        println!("Detected {} external storage(s):", storages.len());
        for s in storages {
            println!(
                "- device: {}\n  label: {}\n  mount: {}\n  fs: {}\n",
                s.device,
                s.label.as_deref().unwrap_or(""),
                s.mount_point,
                s.fs_type
            );
        }
    }

    // Also print as JSON
    println!("JSON:\n{}", storage_detector::list_json());

    Ok(())
}

