#include <receiver/client.hpp>

#include <cassert>

void test_param_flags() {
    using receiver::ParamFlags;
    ParamFlags flags{};
    assert(flags.to_flags() == 0);

    flags.band = ParamFlags::Band::MHz915;
    flags.physical_crc = ParamFlags::PhysicalCrcSwitch::Disabled;
    flags.wireless_estop = ParamFlags::WirelessEstopSwitch::Disabled;
    flags.heartbeat = ParamFlags::HeartbeatSwitch::Disabled;
    flags.group_mode = ParamFlags::GroupMode::OneToMany;
    flags.channel_scan_mode = ParamFlags::ChannelScanMode::Hopping;
    flags.modulation = ParamFlags::Modulation::Gfsk;
    assert(flags.to_flags() == 0x403f);
    assert(ParamFlags::valid_flags(0x403f));
    assert(ParamFlags::from_flags(0x403f) == flags);
    assert(!ParamFlags::valid_flags(0x3fc0));
    assert(!ParamFlags::valid_flags(0x8000));
    assert(static_cast<std::uint16_t>(ParamFlags::from_flags(0x8000).modulation) == 2);
}

int main() {
    test_param_flags();
    using namespace receiver;
    const auto defaults = default_lora_parameters();
    assert(valid(defaults));
    assert(same_parameters(defaults, defaults));
    auto changed = defaults;
    changed.heartbeat_interval = 500;
    assert(valid(changed));
    assert(!same_parameters(defaults, changed));
    changed.tx_power = 11;
    assert(!valid(changed));
    assert(valid(default_gfsk_parameters()));

    Client client;
    assert(!client.is_connected());

    const auto info = client.read_info();
    assert(!info && info.error() == Error::NotConnected);

    const auto lora = client.read_lora_parameters();
    assert(!lora && lora.error() == Error::NotConnected);

    const auto connection = client.connect(233);
    assert(!connection && connection.error() == Error::InvalidArgument);

    const auto write = client.write_lora_parameters(defaults);
    assert(!write && write.error() == Error::NotConnected);
    assert(client.disconnect());
}
