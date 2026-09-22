#include <catch2/catch.hpp>

#include <transmitter_simulator/device.hpp>

namespace transmitter_simulator {
TEST_CASE("device supplies full SDO dictionary essentials") {
    Device device;
    REQUIRE(device.read_sdo(0x001));
    REQUIRE(device.read_sdo(0x004).value() == 0);
    REQUIRE(device.read_sdo(0x044).value() == 0);
    REQUIRE(device.read_sdo(0x008));
    REQUIRE(device.read_sdo(0x102).value() == 100);
    REQUIRE(device.read_sdo(0x777).error() == SdoAccessError::Unknown);
}

TEST_CASE("device atomically rejects invalid configuration and RO SDO writes") {
    Device device;
    const auto original = device.state().lora;
    auto invalid = original;
    invalid.tx_power = 11;
    REQUIRE_FALSE(device.set_lora(invalid));
    REQUIRE(device.state().lora.tx_power == original.tx_power);
    REQUIRE(device.write_sdo(0x001, 1).error() == SdoAccessError::ReadOnly);
    REQUIRE(device.write_sdo(0x202, 0x454e));
    REQUIRE(device.state().upgrade_request == 0x454e);
}

TEST_CASE("GUI SDO editor state updates all non-reserved dictionary values atomically") {
    Device device;
    SdoEditorState next{};
    next.identity.product_code = 0x11112222;
    next.identity.version_number = 0x33334444;
    next.identity.serial_number = 0x55556666;
    next.firmware.app_firmware_version[0] = 'v';
    next.firmware.app_firmware_version[1] = '2';
    next.battery_percentage = 42;
    next.upgrade_request = 0x12345678;

    REQUIRE(device.set_sdo_editor_state(next));
    REQUIRE(device.read_sdo(0x001).value() == 0x11112222);
    REQUIRE(device.read_sdo(0x002).value() == 0x33334444);
    REQUIRE(device.read_sdo(0x003).value() == 0x55556666);
    REQUIRE(device.read_sdo(0x008).value() == 0x00003276);
    REQUIRE(device.read_sdo(0x102).value() == 42);
    REQUIRE(device.read_sdo(0x202).value() == 0x12345678);
    REQUIRE(device.read_sdo(0x004).value() == 0);
    REQUIRE(device.write_sdo(0x001, 7).error() == SdoAccessError::ReadOnly);
    REQUIRE(device.write_sdo(0x202, 0x12345678).error() == SdoAccessError::InvalidValue);
}

TEST_CASE("restoring one modulation defaults does not change the other modulation") {
    Device device;
    auto lora = device.state().lora;
    auto gfsk = device.state().gfsk;
    lora.frequency_offset = 500;
    gfsk.frequency_deviation = 30000;
    REQUIRE(device.set_lora(lora));
    REQUIRE(device.set_gfsk(gfsk));

    REQUIRE(device.set_lora(LoraConfig::defaults()));
    REQUIRE(device.state().lora.frequency_offset == LoraConfig::defaults().frequency_offset);
    REQUIRE(device.state().gfsk.frequency_deviation == 30000);

    REQUIRE(device.set_gfsk(GfskConfig::defaults()));
    REQUIRE(device.state().gfsk.frequency_deviation == GfskConfig::defaults().frequency_deviation);
    REQUIRE(device.state().lora.frequency_offset == LoraConfig::defaults().frequency_offset);
}
}  // namespace transmitter_simulator
