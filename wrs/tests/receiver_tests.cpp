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
    assert(!client.read_info() && client.read_info().error() == Error::NotConnected);
    assert(
        !client.read_lora_parameters() &&
        client.read_lora_parameters().error() == Error::NotConnected);
    assert(!client.connect(233) && client.connect(233).error() == Error::InvalidArgument);
    assert(
        !client.write_lora_parameters(defaults) &&
        client.write_lora_parameters(defaults).error() == Error::NotConnected);
    assert(client.disconnect());
}
