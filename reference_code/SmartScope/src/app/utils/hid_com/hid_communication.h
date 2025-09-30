#ifndef HID_COMMUNICATION_H
#define HID_COMMUNICATION_H

#include <vector>
#include <string>
#include <cstdint>

class HIDCommunication {
public:
    HIDCommunication(uint16_t vid = 0x0001, 
                    uint16_t pid = 0xEDD1,
                    uint16_t usage_page = 0xFF60,
                    uint16_t usage = 0x61,
                    size_t report_size = 33);
    ~HIDCommunication();

    bool open();
    void close();
    bool isConnected() const;
    
    std::vector<uint8_t> sendReceive(const std::vector<uint8_t>& data, 
                                   uint32_t timeout_ms = 1000);
    
    std::string getManufacturer() const;
    std::string getProduct() const;

private:
    class Impl;
    Impl* impl_;
};

#endif // HID_COMMUNICATION_H