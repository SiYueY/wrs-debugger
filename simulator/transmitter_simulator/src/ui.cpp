#include "ui.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>

#include <imgui.h>

namespace transmitter_simulator {
namespace {
enum class Page : int { Device, Parameter, Pin, Sdo, Protocol, Faults };
constexpr ImVec4 k_accent{0.18F, 0.58F, 0.91F, 1.0F};
constexpr ImVec4 k_good{0.22F, 0.78F, 0.50F, 1.0F};
constexpr ImVec4 k_warn{0.95F, 0.66F, 0.20F, 1.0F};
constexpr ImVec4 k_bad{0.94F, 0.32F, 0.34F, 1.0F};

const char* lifecycle_name(LifecycleState state) noexcept {
    return state == LifecycleState::Running        ? "RUNNING"
           : state == LifecycleState::Disconnected ? "DISCONNECTED"
                                                   : "STOPPED";
}
const char* direction_name(FrameDirection direction) noexcept {
    return direction == FrameDirection::Received      ? "RX"
           : direction == FrameDirection::Transmitted ? "TX"
                                                      : "EVENT";
}
const char* error_name(Error error) noexcept {
    switch (error) {
        case Error::InvalidArgument:
            return "Invalid value";
        case Error::InvalidState:
            return "Simulator is not running";
        case Error::Busy:
            return "Path is locked or not owned by this user";
        case Error::Io:
            return "PTY or filesystem operation failed";
        default:
            return "Operation failed";
    }
}
void badge(const char* id, const char* label, const char* value, const ImVec4& color) {
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImVec4(0.88F + color.x * .12F, 0.88F + color.y * .12F, 0.88F + color.z * .12F, 1.0F));
    ImGui::PushStyleColor(ImGuiCol_Border, color);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0F, 7.0F));
    ImGui::BeginChild(id, ImVec2(145, 56), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextDisabled("%s", label);
    ImGui::TextColored(color, "%s", value);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}
void key_value(const char* key, const char* value) {
    ImGui::TextDisabled("%-19s", key);
    ImGui::SameLine(172);
    ImGui::TextUnformatted(value);
}
void readonly_path_field(const char* label, const char* id, const std::string& value) {
    std::array<char, PATH_MAX> buffer{};
    std::strncpy(buffer.data(), value.c_str(), buffer.size() - 1);
    ImGui::TextDisabled("%s", label);
    ImGui::SetNextItemWidth(-70.0F);
    ImGui::InputText(
        (std::string("##") + id).c_str(), buffer.data(), buffer.size(),
        ImGuiInputTextFlags_ReadOnly);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", value.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton((std::string("Copy##") + id).c_str()))
        ImGui::SetClipboardText(value.c_str());
}
template <typename Value>
void hex_input(const char* id, Value& value, const char* format) {
    ImGui::TextDisabled("0X");
    ImGui::SameLine(0.0F, 2.0F);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputScalar(
        id,
        std::is_same_v<Value, std::uint8_t>    ? ImGuiDataType_U8
        : std::is_same_v<Value, std::uint16_t> ? ImGuiDataType_U16
                                               : ImGuiDataType_U32,
        &value, nullptr, nullptr, format, ImGuiInputTextFlags_CharsHexadecimal);
}
void input_i16(const char* id, std::int16_t& value) {
    auto raw = static_cast<std::uint16_t>(value);
    hex_input(id, raw, "%04X");
    value = static_cast<std::int16_t>(raw);
}
void input_u8(const char* id, std::uint8_t& value) { hex_input(id, value, "%02X"); }
void input_u16(const char* id, std::uint16_t& value) { hex_input(id, value, "%04X"); }
void input_u32(const char* id, std::uint32_t& value) { hex_input(id, value, "%08X"); }
std::string hex_bytes(const Bytes& bytes, std::size_t offset, std::size_t count) {
    std::string text;
    for (std::size_t i = 0; i < count; ++i) {
        char item[4]{};
        std::snprintf(item, sizeof(item), "%02X", bytes[offset + i]);
        if (!text.empty()) text.push_back(' ');
        text += item;
    }
    return text;
}
template <std::size_t Size>
std::array<char, Size + 1> text_from_metadata(const std::array<std::uint8_t, Size>& bytes) {
    std::array<char, Size + 1> text{};
    for (std::size_t i = 0; i < Size && bytes[i] != 0; ++i) text[i] = static_cast<char>(bytes[i]);
    return text;
}
template <std::size_t Size>
std::array<std::uint8_t, Size> metadata_from_text(const std::array<char, Size + 1>& text) {
    std::array<std::uint8_t, Size> bytes{};
    std::copy_n(
        reinterpret_cast<const std::uint8_t*>(text.data()),
        std::min(std::strlen(text.data()), Size), bytes.begin());
    return bytes;
}

void device_page(Simulator& simulator, const SimulatorSnapshot& snapshot) {
    static std::array<char, PATH_MAX> pending_path{};
    static bool path_dirty = false;
    static std::string path_error{};
    if (!path_dirty && std::string(pending_path.data()) != snapshot.stable_path) {
        std::strncpy(pending_path.data(), snapshot.stable_path.c_str(), pending_path.size() - 1);
        pending_path.back() = '\0';
    }
    ImGui::TextColored(k_accent, "DEVICE");
    ImGui::TextDisabled(
        "Transport and runtime status only. Configure state in Parameter, PIN, and SDO.");
    if (ImGui::BeginTable("device_columns", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::SeparatorText("TRANSPORT");
        readonly_path_field("PTY slave", "pty-slave", snapshot.slave_path);
        readonly_path_field("Stable path", "stable-path-current", snapshot.stable_path);
        key_value("Lifecycle", lifecycle_name(snapshot.lifecycle));
        key_value("Host peer", snapshot.peer == PeerState::Active ? "Active" : "Detached");
        ImGui::Spacing();
        ImGui::SeparatorText("Stable virtual serial path");
        ImGui::TextDisabled("Change the published link, then recreate transport safely.");
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::InputText("##stable-path", pending_path.data(), pending_path.size())) {
            path_dirty = true;
            path_error.clear();
        }
        const bool path_changed = std::string(pending_path.data()) != snapshot.stable_path;
        const bool absolute = pending_path[0] == '/';
        if (!absolute && pending_path[0] != '\0')
            ImGui::TextColored(k_bad, "Path must be absolute.");
        if (!path_error.empty()) ImGui::TextColored(k_bad, "%s", path_error.c_str());
        if (!path_changed || !absolute) ImGui::BeginDisabled();
        if (ImGui::Button("Apply stable path")) {
            const auto result = simulator.set_transport_path(pending_path.data());
            if (result) {
                path_dirty = false;
                path_error.clear();
            } else
                path_error = error_name(result.error());
        }
        if (!path_changed || !absolute) ImGui::EndDisabled();
        ImGui::SameLine();
        if (snapshot.lifecycle != LifecycleState::Running) ImGui::BeginDisabled();
        if (ImGui::Button("Recreate PTY")) {
            const auto result = simulator.recreate_pty();
            path_error = result ? "" : error_name(result.error());
        }
        if (snapshot.lifecycle != LifecycleState::Running) ImGui::EndDisabled();
        ImGui::TextDisabled("PTY slave is allocated by Linux; recreate it to obtain a new slave.");
        ImGui::TableNextColumn();
        ImGui::SeparatorText("RUNTIME");
        ImGui::Text("RX frames  %zu", snapshot.rx_frames);
        ImGui::Text("TX frames  %zu", snapshot.tx_frames);
        ImGui::Spacing();
        ImGui::TextDisabled("Factory defaults are restored per modulation in Parameter.");
        ImGui::Spacing();
        if (snapshot.lifecycle == LifecycleState::Running) {
            ImGui::PushStyleColor(ImGuiCol_Button, k_bad);
            if (ImGui::Button("Disconnect simulator")) (void)simulator.disconnect();
            ImGui::PopStyleColor();
        } else if (snapshot.lifecycle == LifecycleState::Disconnected) {
            ImGui::PushStyleColor(ImGuiCol_Button, k_good);
            if (ImGui::Button("Reconnect simulator")) (void)simulator.reconnect();
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
}

void parameter_row_start(const char* bytes, const char* field) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", bytes);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(field);
    ImGui::TableSetColumnIndex(2);
}
void parameter_row_end(const char* rule) {
    ImGui::TableSetColumnIndex(3);
    ImGui::TextDisabled("%s", rule);
}
void fixed_parameter_row(
    const char* bytes, const char* field, const Bytes& preview, std::size_t offset,
    std::size_t count, const char* rule) {
    parameter_row_start(bytes, field);
    const auto value = hex_bytes(preview, offset, count);
    ImGui::TextUnformatted(value.c_str());
    parameter_row_end(rule);
}
void flags_editor(ParamFlags& flags, bool gfsk) {
    flags.radio_type = gfsk ? RadioType::Gfsk : RadioType::LoRa;
    int band = flags.band == Band::MHz915 ? 1 : 0;
    const char* bands[] = {"0X0 (433 MHz)", "0X1 (915 MHz)"};
    parameter_row_start("", "Raw parameter flags");
    ImGui::TextDisabled("0X%04X", flags.to_raw());
    parameter_row_end(gfsk ? "GFSK flags" : "LoRa flags");
    parameter_row_start("", "Operation type");
    ImGui::TextDisabled("%s", gfsk ? "0X1 (GFSK)" : "0X0 (LoRa)");
    parameter_row_end("read-only modulation selector");
    parameter_row_start("", "Frequency band");
    ImGui::SetNextItemWidth(120.0F);
    ImGui::Combo("##parameter-band", &band, bands, 2);
    flags.band = band == 0 ? Band::MHz433 : Band::MHz915;
    parameter_row_end("bit 0");
    parameter_row_start("", "PHY CRC");
    ImGui::Checkbox("CRC##parameter-flags", &flags.crc_enabled);
    parameter_row_end("bit 1");
    parameter_row_start("", "Wireless emergency stop");
    ImGui::Checkbox("E-stop##parameter-flags", &flags.estop_enabled);
    parameter_row_end("bit 2");
    parameter_row_start("", "Heartbeat");
    ImGui::Checkbox("Heartbeat##parameter-flags", &flags.heartbeat_enabled);
    parameter_row_end("bit 3");
    parameter_row_start("", "Channel scan");
    ImGui::Checkbox("Scan##parameter-flags", &flags.channel_scan);
    parameter_row_end("bit 5");
    parameter_row_start("", "Group mode");
    const char* group_modes[] = {"0X0 (One-to-one)", "0X1 (One-to-many)"};
    int group_mode = flags.one_to_one ? 0 : 1;
    ImGui::SetNextItemWidth(145.0F);
    ImGui::Combo("##parameter-group-mode", &group_mode, group_modes, 2);
    flags.one_to_one = group_mode == 0;
    parameter_row_end("bit 4");
}
void lora_rows(LoraConfig& config) {
    LoRaParamFrame frame{};
    frame.command = SystemCmd::ParamWriteReq;
    frame.transaction_id = 1;
    frame.config = config;
    const Bytes preview = frame.to_bytes();
    fixed_parameter_row("Byte0", "System command", preview, 0, 1, "0X05 write request");
    fixed_parameter_row("Byte1-4", "Transaction ID", preview, 1, 4, "preview ID 1 (automatic)");
    fixed_parameter_row("Byte5-6", "Object Index", preview, 5, 2, "0X0000 radio configuration");
    fixed_parameter_row(
        "Byte7-10", "Object Data", preview, 7, 4, "0X00000000 for radio configuration");
    parameter_row_start("Byte11-12", "Parameter flags");
    ImGui::TextDisabled("See expanded fields below");
    parameter_row_end("LoRa / band / CRC / E-stop / heartbeat / scan");
    flags_editor(config.flags, false);
    parameter_row_start("Byte13-14", "TX power (dBm)");
    input_i16("##lora-power", config.tx_power);
    parameter_row_end("0-10 @433; 0-20 @915");
    parameter_row_start("Byte15-16", "Frequency offset (kHz)");
    input_u16("##lora-offset", config.frequency_offset);
    parameter_row_end("center frequency offset");
    parameter_row_start("Byte17", "Payload length");
    input_u8("##lora-payload", config.payload_length);
    parameter_row_end("fixed 12 bytes");
    parameter_row_start("Byte18", "RSSI threshold raw");
    input_u8("##lora-rssi", config.rssi_threshold);
    parameter_row_end("10-148; threshold = -raw dBm");
    parameter_row_start("Byte19-20", "Heartbeat interval (ms)");
    input_u16("##lora-heartbeat", config.heartbeat_interval);
    parameter_row_end("200-10000");
    parameter_row_start("Byte21", "Heartbeat loss threshold");
    input_u8("##lora-loss", config.heartbeat_loss);
    parameter_row_end("1-255 packets");
    parameter_row_start("Byte22", "Receive bandwidth");
    input_u8("##lora-bandwidth", config.bandwidth);
    parameter_row_end("0=125, 1=250, 2=500 kHz");
    parameter_row_start("Byte23", "Spreading factor");
    input_u8("##lora-sf", config.spreading_factor);
    parameter_row_end("SF5-SF12");
    parameter_row_start("Byte24", "Coding rate");
    input_u8("##lora-coding", config.coding_rate);
    parameter_row_end("0-6 protocol enumeration");
    parameter_row_start("Byte25", "Header type");
    input_u8("##lora-header", config.header_type);
    parameter_row_end("0=explicit; 1=implicit");
    parameter_row_start("Byte26", "Preamble length");
    input_u8("##lora-preamble", config.preamble_length);
    parameter_row_end("10-50; SF5/SF6 require 12");
    parameter_row_start("Byte27-28", "Sync word");
    input_u16("##lora-sync", config.sync_word);
    parameter_row_end("0xY4X4");
    frame.config = config;
    const Bytes encoded = frame.to_bytes();
    fixed_parameter_row("Byte29-38", "Reserved", encoded, 29, 10, "fixed 0");
    fixed_parameter_row("Byte39", "Result code", encoded, 39, 1, "fixed 0");
    fixed_parameter_row("Byte40-41", "CRC16-XMODEM", encoded, 40, 2, "calculated automatically");
}
void gfsk_rows(GfskConfig& config) {
    GfskParamFrame frame{};
    frame.command = SystemCmd::ParamWriteReq;
    frame.transaction_id = 1;
    frame.config = config;
    const Bytes preview = frame.to_bytes();
    fixed_parameter_row("Byte0", "System command", preview, 0, 1, "0X05 write request");
    fixed_parameter_row("Byte1-4", "Transaction ID", preview, 1, 4, "preview ID 1 (automatic)");
    fixed_parameter_row("Byte5-6", "Object Index", preview, 5, 2, "0X0000 radio configuration");
    fixed_parameter_row(
        "Byte7-10", "Object Data", preview, 7, 4, "0X00000000 for radio configuration");
    parameter_row_start("Byte11-12", "Parameter flags");
    ImGui::TextDisabled("See expanded fields below");
    parameter_row_end("GFSK / band / CRC / E-stop / heartbeat / scan");
    flags_editor(config.flags, true);
    parameter_row_start("Byte13-14", "TX power (dBm)");
    input_i16("##gfsk-power", config.tx_power);
    parameter_row_end("0-10 @433; 0-20 @915");
    parameter_row_start("Byte15-16", "Frequency offset (kHz)");
    input_u16("##gfsk-offset", config.frequency_offset);
    parameter_row_end("center frequency offset");
    parameter_row_start("Byte17", "Payload length");
    input_u8("##gfsk-payload", config.payload_length);
    parameter_row_end("fixed 12 bytes");
    parameter_row_start("Byte18", "RSSI threshold raw");
    input_u8("##gfsk-rssi", config.rssi_threshold);
    parameter_row_end("10-148; threshold = -raw dBm");
    parameter_row_start("Byte19-20", "Heartbeat interval (ms)");
    input_u16("##gfsk-heartbeat", config.heartbeat_interval);
    parameter_row_end("200-10000");
    parameter_row_start("Byte21", "Heartbeat loss threshold");
    input_u8("##gfsk-loss", config.heartbeat_loss);
    parameter_row_end("1-255 packets");
    parameter_row_start("Byte22", "Receive bandwidth");
    input_u8("##gfsk-bandwidth", config.bandwidth);
    parameter_row_end("0=117.3, 1=234.3, 2=467 kHz");
    parameter_row_start("Byte23-26", "Bit rate (bps)");
    input_u32("##gfsk-rate", config.bitrate);
    parameter_row_end("600-150000");
    parameter_row_start("Byte27-30", "Frequency deviation (Hz)");
    input_u32("##gfsk-deviation", config.frequency_deviation);
    parameter_row_end("600-300000; 2*deviation/rate >= 0.5");
    parameter_row_start("Byte31", "Pulse shaping");
    input_u8("##gfsk-pulse", config.pulse_shaping);
    parameter_row_end("0x00, 0x08-0x0B");
    parameter_row_start("Byte32", "Preamble length (bit)");
    input_u8("##gfsk-preamble", config.preamble_length);
    parameter_row_end("16-255");
    parameter_row_start("Byte33-34", "Sync word");
    input_u16("##gfsk-sync", config.sync_word);
    parameter_row_end("16-bit value");
    frame.config = config;
    const Bytes encoded = frame.to_bytes();
    fixed_parameter_row("Byte35-38", "Reserved", encoded, 35, 4, "fixed 0");
    fixed_parameter_row("Byte39", "Result code", encoded, 39, 1, "fixed 0");
    fixed_parameter_row("Byte40-41", "CRC16-XMODEM", encoded, 40, 2, "calculated automatically");
}
template <typename DrawRows>
void parameter_table(const char* id, DrawRows&& draw_rows) {
    if (!ImGui::BeginTable(
            id, 4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
            ImVec2(0, -48)))
        return;
    ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 88.0F);
    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 190.0F);
    ImGui::TableSetupColumn("Value / control", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Protocol rule", ImGuiTableColumnFlags_WidthFixed, 250.0F);
    ImGui::TableHeadersRow();
    draw_rows();
    ImGui::EndTable();
}
void parameter_page(Simulator& simulator, const SimulatorSnapshot& snapshot) {
    static LoraConfig lora{};
    static GfskConfig gfsk{};
    static bool loaded = false;
    if (!loaded) {
        lora = snapshot.device.lora;
        gfsk = snapshot.device.gfsk;
        loaded = true;
    }
    ImGui::TextColored(k_accent, "PARAMETER");
    ImGui::SameLine();
    if (ImGui::SmallButton("Reload from device")) {
        lora = snapshot.device.lora;
        gfsk = snapshot.device.gfsk;
    }
    ImGui::TextDisabled(
        "Radio configuration fields. Manage non-radio SDO objects on the SDO page.");
    if (ImGui::BeginTabBar("parameter_tabs")) {
        if (ImGui::BeginTabItem("LoRa")) {
            parameter_table("lora_frame", [&] { lora_rows(lora); });
            const bool valid = lora.is_valid();
            if (!valid)
                ImGui::TextColored(k_bad, "Configuration violates the LoRa protocol constraints.");
            ImGui::SameLine();
            if (!valid) ImGui::BeginDisabled();
            if (ImGui::Button("Apply LoRa configuration")) (void)simulator.set_lora(lora);
            if (!valid) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Restore LoRa defaults")) {
                lora = LoraConfig::defaults();
                (void)simulator.set_lora(lora);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GFSK")) {
            parameter_table("gfsk_frame", [&] { gfsk_rows(gfsk); });
            const bool valid = gfsk.is_valid();
            if (!valid)
                ImGui::TextColored(k_bad, "Configuration violates the GFSK protocol constraints.");
            ImGui::SameLine();
            if (!valid) ImGui::BeginDisabled();
            if (ImGui::Button("Apply GFSK configuration")) (void)simulator.set_gfsk(gfsk);
            if (!valid) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Restore GFSK defaults")) {
                gfsk = GfskConfig::defaults();
                (void)simulator.set_gfsk(gfsk);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void pin_page(Simulator& simulator, const SimulatorSnapshot& snapshot) {
    static std::array<char, 7> pin{};
    static bool loaded = false;
    static std::string message{};
    if (!loaded) {
        std::copy(
            snapshot.device.pin.digits.begin(), snapshot.device.pin.digits.end(), pin.begin());
        loaded = true;
    }
    ImGui::TextColored(k_accent, "PIN");
    ImGui::TextDisabled(
        "PIN is a six-digit device setting. 000000 remains reserved for serial PIN read requests.");
    ImGui::BeginChild("pin_editor", ImVec2(500, 150), true);
    ImGui::TextDisabled("Current value is shown because this is the firmware simulator.");
    ImGui::SetNextItemWidth(180);
    ImGui::InputText("PIN", pin.data(), pin.size(), ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        std::copy(
            snapshot.device.pin.digits.begin(), snapshot.device.pin.digits.end(), pin.begin());
        pin.back() = '\0';
        message.clear();
    }
    if (ImGui::Button("Apply PIN")) {
        const std::string candidate(pin.data());
        const bool digits = candidate.size() == 6 &&
                            std::all_of(candidate.begin(), candidate.end(), [](char value) {
                                return value >= '0' && value <= '9';
                            });
        if (!digits)
            message = "PIN must contain exactly six decimal digits.";
        else if (candidate == "000000")
            message = "000000 is reserved as the serial PIN read sentinel.";
        else {
            Pin next{};
            std::copy_n(
                reinterpret_cast<const std::uint8_t*>(candidate.data()), next.digits.size(),
                next.digits.begin());
            const auto result = simulator.set_pin(next);
            message = result ? "PIN applied." : error_name(result.error());
        }
    }
    if (!message.empty())
        ImGui::TextColored(message == "PIN applied." ? k_good : k_bad, "%s", message.c_str());
    ImGui::EndChild();
}

struct SdoDraft final {
    DeviceIdentity identity{};
    std::array<char, 41> app_version{};
    std::array<char, 41> boot_version{};
    std::array<char, 41> app_branch{};
    std::array<char, 41> app_tag{};
    std::array<char, 41> boot_branch{};
    std::array<char, 41> boot_tag{};
    int battery{100};
    std::uint32_t upgrade_request{};
};
SdoDraft sdo_draft_from(const DeviceState& state) {
    SdoDraft draft{};
    draft.identity = state.identity;
    draft.app_version = text_from_metadata(state.firmware.app_firmware_version);
    draft.boot_version = text_from_metadata(state.firmware.bootloader_firmware_version);
    draft.app_branch = text_from_metadata(state.firmware.app_branch_name);
    draft.app_tag = text_from_metadata(state.firmware.app_tag_sha1_id);
    draft.boot_branch = text_from_metadata(state.firmware.boot_branch_name);
    draft.boot_tag = text_from_metadata(state.firmware.boot_tag_sha1_id);
    draft.battery = state.battery_percentage;
    draft.upgrade_request = state.upgrade_request;
    return draft;
}
SdoEditorState sdo_editor_state_from(const SdoDraft& draft) {
    SdoEditorState state{};
    state.identity = draft.identity;
    state.firmware.app_firmware_version = metadata_from_text<40>(draft.app_version);
    state.firmware.bootloader_firmware_version = metadata_from_text<40>(draft.boot_version);
    state.firmware.app_branch_name = metadata_from_text<40>(draft.app_branch);
    state.firmware.app_tag_sha1_id = metadata_from_text<40>(draft.app_tag);
    state.firmware.boot_branch_name = metadata_from_text<40>(draft.boot_branch);
    state.firmware.boot_tag_sha1_id = metadata_from_text<40>(draft.boot_tag);
    state.battery_percentage = static_cast<std::uint8_t>(std::clamp(draft.battery, 0, 100));
    state.upgrade_request = draft.upgrade_request;
    return state;
}
void sdo_text_input(const char* index, const char* name, std::array<char, 41>& value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(index);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(name);
    ImGui::TableSetColumnIndex(2);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText((std::string("##") + index).c_str(), value.data(), value.size());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", value.data());
    ImGui::TableSetColumnIndex(3);
    ImGui::TextDisabled("ASCII, max 40 bytes");
}
void sdo_page(Simulator& simulator, const SimulatorSnapshot& snapshot) {
    static SdoDraft draft{};
    static bool loaded = false;
    static std::string message{};
    if (!loaded) {
        draft = sdo_draft_from(snapshot.device);
        loaded = true;
    }
    ImGui::TextColored(k_accent, "SDO");
    ImGui::SameLine();
    if (ImGui::SmallButton("Reload from device")) {
        draft = sdo_draft_from(snapshot.device);
        message.clear();
    }
    ImGui::TextDisabled(
        "Internal firmware-state editor. Serial SDO access permissions remain unchanged.");
    if (ImGui::BeginTable(
            "sdo_table", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
            ImVec2(0, -48))) {
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 112.0F);
        ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 230.0F);
        ImGui::TableSetupColumn("Simulator value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Access / rule", ImGuiTableColumnFlags_WidthFixed, 250.0F);
        ImGui::TableHeadersRow();
        auto sdo_number = [](const char* index, const char* object, std::uint32_t& value,
                             const char* rule) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(index);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(object);
            ImGui::TableSetColumnIndex(2);
            input_u32((std::string("##") + index).c_str(), value);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%s", rule);
        };
        auto sdo_readonly = [](const char* index, const char* object, const char* value,
                               const char* rule) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(index);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(object);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", value);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%s", rule);
        };
        sdo_number(
            "0X001", "Product code", draft.identity.product_code, "serial: RO; GUI: editable");
        sdo_number(
            "0X002", "Version number", draft.identity.version_number, "serial: RO; GUI: editable");
        sdo_number(
            "0X003", "Serial number", draft.identity.serial_number, "serial: RO; GUI: editable");
        sdo_readonly("0X004-0X007", "Identity reserved", "0X00000000", "fixed 0");
        sdo_text_input("0x008-0x011", "App firmware version", draft.app_version);
        sdo_text_input("0x012-0x01B", "Bootloader firmware version", draft.boot_version);
        sdo_text_input("0x01C-0x025", "App branch name", draft.app_branch);
        sdo_text_input("0x026-0x02F", "App tag SHA1 ID", draft.app_tag);
        sdo_text_input("0x030-0x039", "Boot branch name", draft.boot_branch);
        sdo_text_input("0x03A-0x043", "Boot tag SHA1 ID", draft.boot_tag);
        sdo_readonly("0X044-0X0FF", "Reserved", "0X00000000", "fixed 0");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("0X102");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("Battery");
        ImGui::TableSetColumnIndex(2);
        auto battery = static_cast<std::uint8_t>(std::clamp(draft.battery, 0, 100));
        input_u8("##battery", battery);
        draft.battery = battery;
        ImGui::TableSetColumnIndex(3);
        ImGui::TextDisabled("serial: RO; GUI: editable");
        sdo_number(
            "0X202", "Upgrade request", draft.upgrade_request,
            "serial write accepts 0X0000454E only");
        ImGui::EndTable();
    }
    if (ImGui::Button("Apply SDO state")) {
        const auto result = simulator.set_sdo_editor_state(sdo_editor_state_from(draft));
        message = result ? "SDO state applied atomically." : error_name(result.error());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Reserved objects always read as 0x00000000.");
    if (!message.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(
            message == "SDO state applied atomically." ? k_good : k_bad, "%s", message.c_str());
    }
}

void protocol_page(Simulator& simulator, const SimulatorSnapshot& snapshot) {
    static std::optional<std::size_t> selected_record{};
    if (snapshot.history.empty())
        selected_record.reset();
    else if (!selected_record || *selected_record >= snapshot.history.size())
        selected_record = snapshot.history.size() - 1;

    ImGui::TextColored(k_accent, "PROTOCOL HISTORY");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu records", snapshot.history.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear history")) {
        selected_record.reset();
        (void)simulator.clear_history();
    }
    if (ImGui::BeginTable(
            "history", 7, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
            ImVec2(0, -190))) {
        ImGui::TableSetupColumn("Dir", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("Cmd", ImGuiTableColumnFlags_WidthFixed, 78);
        ImGui::TableSetupColumn("TxID", ImGuiTableColumnFlags_WidthFixed, 115);
        ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 112);
        ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("CRC", ImGuiTableColumnFlags_WidthFixed, 95);
        ImGui::TableSetupColumn("Note", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < snapshot.history.size(); ++index) {
            const auto& record = snapshot.history[index];
            const auto& frame = record.frame;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool selected = selected_record && *selected_record == index;
            if (ImGui::Selectable(
                    (std::string("##history-record-") + std::to_string(index)).c_str(), selected,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                selected_record = index;
            ImGui::SameLine();
            const ImVec4 color = record.direction == FrameDirection::Received      ? k_accent
                                 : record.direction == FrameDirection::Transmitted ? k_good
                                                                                   : k_warn;
            ImGui::TextColored(color, "%s", direction_name(record.direction));
            ImGui::TableNextColumn();
            ImGui::Text("0X%02X", frame[0]);
            ImGui::TableNextColumn();
            ImGui::Text("%u", frame.read_le32(1));
            ImGui::TableNextColumn();
            ImGui::Text("0X%04X", frame.read_le16(5));
            ImGui::TableNextColumn();
            ImGui::Text("0X%02X", frame[39]);
            ImGui::TableNextColumn();
            ImGui::Text("%s", frame.has_valid_crc() ? "valid" : "bad");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(record.note.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Selected frame record");
    if (snapshot.history.empty()) {
        ImGui::TextDisabled("No frames recorded. Connect a Host client to inspect traffic.");
        return;
    }
    const auto& record = snapshot.history[*selected_record];
    const auto& frame = record.frame;
    ImGui::Text(
        "%s  |  Cmd 0X%02X  |  TxID %u  |  Object 0X%04X  |  Result 0X%02X  |  CRC %s",
        direction_name(record.direction), frame[0], frame.read_le32(1), frame.read_le16(5),
        frame[39], frame.has_valid_crc() ? "valid" : "bad");
    if (!record.note.empty()) ImGui::TextDisabled("Note: %s", record.note.c_str());
    const auto raw_frame = hex_bytes(frame, 0, frame.size());
    if (ImGui::SmallButton("Copy complete hex frame")) ImGui::SetClipboardText(raw_frame.c_str());
    for (std::size_t offset = 0; offset < frame.size(); offset += 16) {
        const auto count = std::min<std::size_t>(16, frame.size() - offset);
        ImGui::Text("%02zu: %s", offset, hex_bytes(frame, offset, count).c_str());
    }
}
void faults_page(Simulator& simulator) {
    static FaultConfig fault{};
    ImGui::TextColored(k_accent, "FAULT INJECTION");
    ImGui::TextDisabled("One-shot settings are consumed by the next matching response.");
    ImGui::BeginChild("faults", ImVec2(0, -44), true);
    int delay = static_cast<int>(fault.response_delay.count());
    ImGui::InputInt("Response delay (ms)", &delay);
    fault.response_delay = std::chrono::milliseconds(std::max(delay, 0));
    ImGui::SeparatorText("Response corruption");
    ImGui::Checkbox("Fail next business response", &fault.fail_next_business_response);
    ImGui::Checkbox("Wrong next transaction ID", &fault.wrong_next_transaction);
    ImGui::Checkbox("Wrong next command", &fault.wrong_next_command);
    ImGui::Checkbox("Corrupt next frame CRC", &fault.corrupt_next_crc);
    ImGui::SeparatorText("SDO fault");
    int sdo = fault.next_sdo_fault == SdoFault::None          ? 0
              : fault.next_sdo_fault == SdoFault::NotReceived ? 1
              : fault.next_sdo_fault == SdoFault::InProgress  ? 2
              : fault.next_sdo_fault == SdoFault::Error       ? 3
                                                              : 4;
    const char* sdo_names[] = {"None", "Not received", "In progress", "Error", "Invalid command"};
    ImGui::Combo("Next SDO status", &sdo, sdo_names, 5);
    const SdoFault sdo_values[] = {
        SdoFault::None, SdoFault::NotReceived, SdoFault::InProgress, SdoFault::Error,
        SdoFault::InvalidCommand};
    fault.next_sdo_fault = sdo_values[sdo];
    ImGui::SeparatorText("Delivery fault");
    int delivery = static_cast<int>(fault.next_delivery_fault);
    const char* delivery_names[] = {"None", "Drop", "Split", "Truncate", "Disconnect"};
    ImGui::Combo("Delivery behavior", &delivery, delivery_names, 5);
    fault.next_delivery_fault = static_cast<DeliveryFault>(delivery);
    int split_at = static_cast<int>(fault.split_after_bytes),
        split_delay = static_cast<int>(fault.split_delay.count()),
        truncate_at = static_cast<int>(fault.truncate_after_bytes);
    ImGui::InputInt("Split after bytes", &split_at);
    ImGui::InputInt("Split delay (ms)", &split_delay);
    ImGui::InputInt("Truncate after bytes", &truncate_at);
    fault.split_after_bytes = static_cast<std::size_t>(std::max(split_at, 0));
    fault.split_delay = std::chrono::milliseconds(std::max(split_delay, 0));
    fault.truncate_after_bytes = static_cast<std::size_t>(std::max(truncate_at, 0));
    ImGui::EndChild();
    if (ImGui::Button("Apply fault configuration", ImVec2(220, 0)))
        (void)simulator.set_fault_config(fault);
}
void navigation(Page& page) {
    ImGui::BeginChild("nav", ImVec2(205, 0), true);
    ImGui::TextColored(k_accent, "WRS DEBUGGER");
    ImGui::TextDisabled("TRANSMITTER SIMULATOR");
    ImGui::Spacing();
    const std::array<std::pair<const char*, Page>, 6> pages{
        {{"Device", Page::Device},
         {"Parameter", Page::Parameter},
         {"PIN", Page::Pin},
         {"SDO", Page::Sdo},
         {"Protocol", Page::Protocol},
         {"Fault injection", Page::Faults}}};
    for (const auto& item : pages)
        if (ImGui::Selectable(item.first, page == item.second, 0, ImVec2(0, 34)))
            page = item.second;
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 45);
    ImGui::TextDisabled("v1 · Linux PTY runtime");
    ImGui::EndChild();
}
}  // namespace

void draw_ui(Simulator& simulator) noexcept {
    static Page page = Page::Device;
    const auto snapshot = simulator.snapshot();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                              ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("WRS Transmitter Simulator", nullptr, window_flags);
    ImGui::PopStyleVar(2);
    ImGui::TextColored(k_accent, "WRS TRANSMITTER SIMULATOR");
    ImGui::SameLine();
    ImGui::TextDisabled(" · USB serial device emulator");
    ImGui::SameLine(ImGui::GetWindowWidth() - 305);
    badge(
        "lifecycle", "Lifecycle", lifecycle_name(snapshot.lifecycle),
        snapshot.lifecycle == LifecycleState::Running ? k_good : k_bad);
    ImGui::SameLine();
    badge(
        "peer", "Host peer", snapshot.peer == PeerState::Active ? "ACTIVE" : "DETACHED",
        snapshot.peer == PeerState::Active ? k_good : k_warn);
    ImGui::Separator();
    navigation(page);
    ImGui::SameLine();
    ImGui::BeginChild("workspace", ImVec2(0, 0), false);
    if (page == Page::Device) device_page(simulator, snapshot);
    if (page == Page::Parameter) parameter_page(simulator, snapshot);
    if (page == Page::Pin) pin_page(simulator, snapshot);
    if (page == Page::Sdo) sdo_page(simulator, snapshot);
    if (page == Page::Protocol) protocol_page(simulator, snapshot);
    if (page == Page::Faults) faults_page(simulator);
    ImGui::EndChild();
    ImGui::End();
}
}  // namespace transmitter_simulator
