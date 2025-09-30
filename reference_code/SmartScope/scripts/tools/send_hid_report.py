import sys
import hid
import time

vendor_id = 0x0001
product_id = 0xEDD1
usage_page = 0xFF60
usage = 0x61
report_length = 32

def get_raw_hid_interface():
    # 使用 hid.enumerate() 查找设备
    device_list = hid.enumerate(vendor_id, product_id)
    raw_hid_interfaces = [
        d for d in device_list 
        if d['usage_page'] == usage_page and d['usage'] == usage
    ]

    if not raw_hid_interfaces:
        return None

    # 使用 hid.Device() 打开设备
    try:
        device = hid.Device(path=raw_hid_interfaces[0]['path'])
        print(f"Manufacturer: {device.manufacturer}")
        print(f"Product: {device.product}")
        return device
    except Exception as e:
        print(f"Failed to open device: {e}")
        return None

def send_raw_report(data):
    device = get_raw_hid_interface()
    if not device:
        print("No device found")
        sys.exit(1)

    # 构造报告数据（首字节为报告 ID，此处为 0x00）
    request_data = [0x00] + data
    request_data += [0x00] * (report_length - len(data))  # 填充剩余字节

    print("Request:")
    print(bytes(request_data).hex(' '))

    try:
        device.write(bytes(request_data))
        time.sleep(0.1)
        response = device.read(report_length)
        print("Response:")
        print(bytes(response).hex(' '))
    except Exception as e:
        print(f"Error: {e}")
    finally:
        device.close()

if __name__ == '__main__':
    #0-1279
    send_raw_report([0xaa, 0x55, 0x02, 0x00, 0x00, 0x00, 0x00])
    time.sleep(1)
    send_raw_report([0xaa, 0x55, 0x01, 0x00, 0x00, 0x00, 0x00])
    time.sleep(1)
    send_raw_report([0xaa, 0x55, 0x01, 0x00, 0x00, 0xFF, 0x04])
    # send_raw_report([0xaa, 0x55, 0x02, 0x00, 0x00, 0x00, 0x00])
    