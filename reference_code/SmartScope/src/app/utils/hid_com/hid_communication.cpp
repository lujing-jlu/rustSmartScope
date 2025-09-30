#include "hid_communication.h"
#include <hidapi/hidapi.h>
#include <locale>
#include <codecvt>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <algorithm>

class HIDCommunication::Impl {
public:
    uint16_t vid, pid, usage_page, usage;
    size_t report_size;
    hid_device* device = nullptr;
    std::string manufacturer;
    std::string product;

    Impl(uint16_t v, uint16_t p, uint16_t up, uint16_t u, size_t rs)
        : vid(v), pid(p), usage_page(up), usage(u), report_size(rs) {}
    
    ~Impl() {
        if (device) {
            hid_close(device);
            hid_exit();
        }
    }

    static std::string wstringToString(const wchar_t* wstr) {
        if (!wstr) return "";
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wstr);
    }
};

HIDCommunication::HIDCommunication(uint16_t vid, uint16_t pid, 
                                 uint16_t usage_page, uint16_t usage,
                                 size_t report_size)
    : impl_(new Impl(vid, pid, usage_page, usage, report_size)) {}

HIDCommunication::~HIDCommunication() { delete impl_; }

bool HIDCommunication::open() {
    if (impl_->device) return true;
    if (hid_init() != 0) return false;

    hid_device_info* devices = hid_enumerate(impl_->vid, impl_->pid);
    hid_device_info* current = devices;

    while (current) {
        if (current->usage_page == impl_->usage_page && 
            current->usage == impl_->usage) {
            impl_->device = hid_open_path(current->path);
            if (impl_->device) {
                wchar_t wstr[256];
                if (hid_get_manufacturer_string(impl_->device, wstr, 256) >= 0)
                    impl_->manufacturer = Impl::wstringToString(wstr);
                if (hid_get_product_string(impl_->device, wstr, 256) >= 0)
                    impl_->product = Impl::wstringToString(wstr);
                break;
            }
        }
        current = current->next;
    }

    hid_free_enumeration(devices);
    return impl_->device != nullptr;
}

void HIDCommunication::close() {
    if (impl_->device) {
        hid_close(impl_->device);
        impl_->device = nullptr;
    }
}

bool HIDCommunication::isConnected() const {
    return impl_->device != nullptr;
}

std::vector<uint8_t> HIDCommunication::sendReceive(
    const std::vector<uint8_t>& data, uint32_t timeout_ms) {
    
    if (!impl_->device) throw std::runtime_error("Device not connected");

    std::vector<uint8_t> report(impl_->report_size, 0);
    if (!data.empty()) {
        size_t copy_len = std::min(data.size(), impl_->report_size);
        std::copy(data.begin(), data.begin() + copy_len, report.begin());
    }

    int res = hid_write(impl_->device, report.data(), impl_->report_size);
    if (res < 0) throw std::runtime_error("Write failed");

    std::vector<uint8_t> response(impl_->report_size);
    auto start = std::chrono::steady_clock::now();

    while (true) {
        res = hid_read(impl_->device, response.data(), impl_->report_size);
        if (res > 0) {
            response.resize(res);
            return response;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    throw std::runtime_error("Timeout waiting for response");
}

std::string HIDCommunication::getManufacturer() const {
    return impl_->manufacturer;
}

std::string HIDCommunication::getProduct() const {
    return impl_->product;
}