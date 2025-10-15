use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("io error: {0}")]
    Io(#[from] std::io::Error),
    #[error("utf8 error: {0}")]
    Utf8(#[from] std::string::FromUtf8Error),
}

pub type Result<T> = std::result::Result<T, Error>;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ExternalStorage {
    pub device: String,
    pub label: Option<String>,
    pub mount_point: String,
    pub fs_type: String,
}

/// List external storages by parsing /proc/mounts and /dev/disk/by-label
pub fn list() -> Result<Vec<ExternalStorage>> {
    let mounts = fs::File::open("/proc/mounts")?;
    let reader = BufReader::new(mounts);

    let labels = build_label_map("/dev/disk/by-label");

    let mut storages = Vec::new();
    for line in reader.lines() {
        let line = line?;
        if let Some(entry) = parse_mount_line(&line) {
            if is_external_device(&entry.0, &entry.1, &entry.2) {
                let label = labels.get(&entry.0).cloned();
                storages.push(ExternalStorage {
                    device: entry.0,
                    mount_point: entry.1,
                    fs_type: entry.2,
                    label,
                });
            }
        }
    }

    Ok(storages)
}

/// Return JSON array string of ExternalStorage entries.
pub fn list_json() -> String {
    match list() {
        Ok(v) => serde_json::to_string(&v).unwrap_or_else(|_| "[]".to_string()),
        Err(_) => "[]".to_string(),
    }
}

/// Build a map from device path -> label by reading /dev/disk/by-label
fn build_label_map<P: AsRef<Path>>(by_label_dir: P) -> HashMap<String, String> {
    let mut map = HashMap::new();
    if let Ok(entries) = fs::read_dir(by_label_dir) {
        for e in entries.flatten() {
            let label = e.file_name().to_string_lossy().to_string();
            if let Ok(target) = fs::read_link(e.path()) {
                // read_link may return relative path like ../../sda1; normalize to /dev/sda1
                let mut target_path = PathBuf::from("/dev");
                for comp in target.components() {
                    if let std::path::Component::Normal(os) = comp {
                        target_path.push(os);
                    }
                }
                map.insert(target_path.display().to_string(), label);
            }
        }
    }
    map
}

/// Parse a mount line from /proc/mounts
/// Returns (device, mount_point, fs_type)
fn parse_mount_line(line: &str) -> Option<(String, String, String)> {
    // /proc/mounts is space-separated, escape handling is complex; for our
    // simple needs, split by whitespace into at least 3 fields
    let mut parts = line.split_whitespace();
    let device = parts.next()?.to_string();
    let mount_point = parts.next()?.to_string();
    let fs_type = parts.next()?.to_string();
    Some((device, mount_point, fs_type))
}

/// Heuristic to detect external/removable devices
fn is_external_device(device: &str, mount_point: &str, fs_type: &str) -> bool {
    // Likely removable devices
    let dev_ok = device.starts_with("/dev/sd")
        || device.starts_with("/dev/mmcblk")
        || device.starts_with("/dev/hd")
        || device.starts_with("/dev/vd");

    let mp_ok = mount_point.starts_with("/media/")
        || mount_point.starts_with("/run/media/")
        || mount_point.starts_with("/mnt/")
        || mount_point == "/mnt";

    // Common FS for external drives
    let fs_ok = matches!(
        fs_type,
        "vfat" | "exfat" | "ntfs" | "ntfs3" | "ext4" | "ext3" | "ext2" | "f2fs"
    );

    (dev_ok && fs_ok) || (mp_ok && fs_ok)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_mount_line() {
        let line = "/dev/sda1 /media/user/UDISK vfat rw,relatime 0 0";
        let entry = parse_mount_line(line).unwrap();
        assert_eq!(entry.0, "/dev/sda1");
        assert_eq!(entry.1, "/media/user/UDISK");
        assert_eq!(entry.2, "vfat");
    }

    #[test]
    fn test_is_external_device() {
        assert!(is_external_device(
            "/dev/sda1",
            "/media/user/UDISK",
            "vfat"
        ));
        assert!(!is_external_device(
            "/dev/loop0",
            "/",
            "ext4"
        ));
    }

    #[test]
    fn test_build_label_map_mock() {
        // Can't reliably test real /dev/disk/by-label; instead test normalization logic
        // by creating a temp dir with a symlink ../../sda1 → label
        let dir = tempfile::tempdir().unwrap();
        let lbl = dir.path().join("MYDISK");
        // Create a relative symlink to ../../sda1
        #[cfg(unix)]
        std::os::unix::fs::symlink("../../sda1", &lbl).unwrap();
        let map = build_label_map(dir.path());
        assert_eq!(map.get("/dev/sda1"), Some(&"MYDISK".to_string()));
    }

    #[test]
    fn test_parse_from_string() {
        let mounts = r#"
/dev/sda1 /media/user/UDISK vfat rw,relatime 0 0
/dev/mmcblk0p1 /mnt/sdcard exfat rw,relatime 0 0
tmpfs /run tmpfs rw,nosuid,nodev,relatime,size=... 0 0
"#;
        let mut labels = HashMap::new();
        labels.insert("/dev/sda1".to_string(), "UDISK_LABEL".to_string());
        labels.insert("/dev/mmcblk0p1".to_string(), "SDCARD".to_string());

        let parsed = parse_from_str(mounts, &labels);
        assert_eq!(parsed.len(), 2);
        assert_eq!(parsed[0].label.as_deref(), Some("UDISK_LABEL"));
        assert_eq!(parsed[1].label.as_deref(), Some("SDCARD"));
    }

    fn parse_from_str(content: &str, labels: &HashMap<String, String>) -> Vec<ExternalStorage> {
        let mut storages = Vec::new();
        for line in content.lines() {
            if let Some(entry) = parse_mount_line(line) {
                if is_external_device(&entry.0, &entry.1, &entry.2) {
                    let label = labels.get(&entry.0).cloned();
                    storages.push(ExternalStorage {
                        device: entry.0,
                        mount_point: entry.1,
                        fs_type: entry.2,
                        label,
                    });
                }
            }
        }
        storages
    }
}

