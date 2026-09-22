#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <transmitter_simulator/protocol.hpp>

namespace transmitter_simulator {
TEST_CASE("CRC16-XMODEM matches the standard check vector") {
    Bytes source{};
    source.data = {{'1', '2', '3', '4', '5', '6', '7', '8', '9'}};
    REQUIRE(source.crc16_xmodem(9) == 0x31c3);
}

TEST_CASE("little-endian helpers and LoRa wire encoding are stable") {
    LoRaParamFrame frame{};
    frame.command = SystemCmd::ParamWriteReq;
    frame.transaction_id = 0x78563412;
    frame.config = LoraConfig::defaults();
    const Bytes bytes = frame.to_bytes();
    REQUIRE(bytes[1] == 0x12);
    REQUIRE(bytes[4] == 0x78);
    REQUIRE(bytes.read_le16(13) == 10);
    REQUIRE(bytes.read_le16(15) == 250);
    REQUIRE(bytes.has_valid_crc());
    const auto decoded = LoRaParamFrame::from_bytes(bytes);
    REQUIRE(decoded.config.sync_word == 0x1424);
    REQUIRE(decoded.config.is_valid());
}

TEST_CASE("radio validation includes both band power limits") {
    auto lora = LoraConfig::defaults();
    lora.flags.band = Band::MHz433;
    lora.tx_power = 11;
    REQUIRE_FALSE(lora.is_valid());
    lora.flags.band = Band::MHz915;
    lora.tx_power = 20;
    REQUIRE(lora.is_valid());
    lora.tx_power = 21;
    REQUIRE_FALSE(lora.is_valid());
}

TEST_CASE("public typed wire frames preserve protocol fields") {
    LoRaParamFrame lora{};
    lora.command = SystemCmd::ParamWriteReq;
    lora.transaction_id = 0x78563412;
    lora.object_index = 0x6000;
    lora.config = LoraConfig::defaults();
    const Bytes lora_bytes = lora.to_bytes();
    REQUIRE(lora_bytes.size() == kFrameSize);
    REQUIRE(lora_bytes.has_valid_crc());
    const auto lora_round_trip = LoRaParamFrame::from_bytes(lora_bytes);
    REQUIRE(lora_round_trip.command == SystemCmd::ParamWriteReq);
    REQUIRE(lora_round_trip.transaction_id == lora.transaction_id);
    REQUIRE(lora_round_trip.config.tx_power == lora.config.tx_power);

    PinFrame pin{};
    pin.command = SystemCmd::PinCfgRsp;
    pin.pin = {{'1', '2', '3', '4', '5', '6'}};
    pin.transaction_id = 9;
    const auto pin_round_trip = PinFrame::from_bytes(pin.to_bytes());
    REQUIRE(pin_round_trip.pin == pin.pin);
    REQUIRE(pin_round_trip.transaction_id == 9);
    REQUIRE(pin_round_trip.is_ascii_pin());
    REQUIRE_FALSE(pin_round_trip.is_read_request());

    SdoFrame sdo{};
    sdo.command = SystemCmd::ParamReadRsp;
    sdo.transaction_id = 7;
    sdo.object_index = static_cast<std::uint16_t>(0x4000U | sdo::ProductCode);
    sdo.object_data = 0x57525301;
    const auto sdo_round_trip = SdoFrame::from_bytes(sdo.to_bytes());
    REQUIRE(sdo_round_trip.object_index == sdo.object_index);
    REQUIRE(sdo_round_trip.object_data == sdo.object_data);
}

TEST_CASE("Bytes and parameter flags expose bounded protocol primitives") {
    Bytes bytes{};
    bytes.write_le16(0, 0x3412);
    bytes.write_le32(2, 0x78563412);
    REQUIRE(bytes.read_le16(0) == 0x3412);
    REQUIRE(bytes.read_le32(2) == 0x78563412);
    REQUIRE(bytes.is_zero(6, kFrameBodySize - 6));
    bytes[6] = 1;
    REQUIRE_FALSE(bytes.is_zero(6, 1));

    const auto defaults = LoraConfig::defaults();
    ParamFlags decoded{};
    REQUIRE(ParamFlags::from_raw(defaults.flags.to_raw(), decoded));
    REQUIRE(decoded.radio_type == RadioType::LoRa);
    REQUIRE(decoded.band == Band::MHz433);
}
}  // namespace transmitter_simulator
