#include <receiver/client.hpp>

#include <cassert>

int main() {
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
