#include <transmitter/device.hpp>

#include <new>
#include <transmitter/client.hpp>
#include <serial/tool.hpp>

namespace transmitter {
hardware::Result<std::vector<DeviceInfo>, Error> discover(
    std::chrono::milliseconds response_timeout, std::chrono::milliseconds retry_interval,
    std::uint8_t max_attempts) noexcept {
    auto ports = serial::list_ports();
    if (!ports) {
        return hardware::Result<std::vector<DeviceInfo>, Error>::failure(Error::Io);
    }
    try {
        std::vector<DeviceInfo> devices;
        for (const auto& port : ports.value()) {
            Client client;
            auto opened = client.open(port.path, response_timeout, retry_interval, max_attempts);
            if (!opened) {
                continue;
            }
            DeviceInfo info{};
            info.path = port.path;
            info.description = port.description;
            info.usb.available = port.usb.available;
            info.usb.vendor_id = port.usb.vendor_id;
            info.usb.product_id = port.usb.product_id;
            info.usb.serial_number = port.usb.serial_number;
            info.usb.manufacturer = port.usb.manufacturer;
            info.usb.product = port.usb.product;
            info.identity = client.identity();
            devices.push_back(std::move(info));
        }
        return hardware::Result<std::vector<DeviceInfo>, Error>::success(std::move(devices));
    } catch (const std::bad_alloc&) {
        return hardware::Result<std::vector<DeviceInfo>, Error>::failure(Error::Io);
    }
}
}  // namespace transmitter
