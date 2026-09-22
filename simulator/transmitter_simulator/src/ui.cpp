#include "ui.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

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
void input_i16(const char* label, std::int16_t& value) {
    int input = value;
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputInt(label, &input);
    value = static_cast<std::int16_t>(std::clamp(input, -32768, 32767));
}
void input_u8(const char* label, std::uint8_t& value) {
    int input = value;
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputInt(label, &input);
    value = static_cast<std::uint8_t>(std::clamp(input, 0, 255));
}
void input_u16(const char* label, std::uint16_t& value) {
    int input = value;
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputInt(label, &input);
    value = static_cast<std::uint16_t>(std::clamp(input, 0, 65535));
}
void input_u32(const char* label, std::uint32_t& value, bool hexadecimal = false) {
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputScalar(
        label, ImGuiDataType_U32, &value, nullptr, nullptr, hexadecimal ? "%08X" : "%u",
        hexadecimal ? ImGuiInputTextFlags_CharsHexadecimal : 0);
}
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
    static std::array<char, 512> pending_path{};
    static bool path_dirty = false;
    static std::string path_error{};
    if (!path_dirty && std::string(pending_path.data()) != snapshot.stable_path) {
        std::strncpy(pending_path.data(), snapshot.stable_path.c_str(), pending_path.size() - 1);
        pending_path.back() = '\0';
    }
    ImGui::TextColored(k_accent, "DEVICE");
    ImGui::TextDisabled(
        "Transport and runtime status only. Configure state in Parameter, PIN, and SDO.");
    if (ImGui::BeginTable("device_cards", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::BeginChild("transport", ImVec2(0, 212), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextColored(k_accent, "TRANSPORT");
        ImGui::Separator();
        key_value("PTY slave", snapshot.slave_path.c_str());
        key_value("Stable path", snapshot.stable_path.c_str());
        key_value("Lifecycle", lifecycle_name(snapshot.lifecycle));
        key_value("Host peer", snapshot.peer == PeerState::Active ? "Active" : "Detached");
        ImGui::Spacing();
        ImGui::TextDisabled("Stable virtual serial path");
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
        if (ImGui::Button("Apply & Reconnect")) {
            const auto result = simulator.set_transport_path(pending_path.data());
            if (result) {
                path_dirty = false;
                path_error.clear();
            } else
                path_error = error_name(result.error());
        }
        if (!path_changed || !absolute) ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("runtime", ImVec2(0, 212), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextColored(k_accent, "RUNTIME");
        ImGui::Separator();
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
        ImGui::EndChild();
        ImGui::EndTable();
    }
}

void table_row_start(const char* bytes, const char* field) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", bytes);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(field);
    ImGui::TableSetColumnIndex(2);
}
void table_row_end(const char* rule) {
    ImGui::TableSetColumnIndex(3);
    ImGui::TextDisabled("%s", rule);
}
void fixed_row(
    const char* bytes, const char* field, const Bytes& preview, std::size_t offset,
    std::size_t count, const char* rule) {
    table_row_start(bytes, field);
    const auto value = hex_bytes(preview, offset, count);
    ImGui::TextUnformatted(value.c_str());
    table_row_end(rule);
}
void flags_editor(ParamFlags& flags, bool gfsk) {
    ImGui::TextDisabled("0x%04X", flags.to_raw());
    ImGui::SameLine();
    int band = flags.band == Band::MHz915 ? 1 : 0;
    const char* bands[] = {"433 MHz", "915 MHz"};
    ImGui::SetNextItemWidth(95);
    ImGui::Combo("##band", &band, bands, 2);
    flags.band = band == 0 ? Band::MHz433 : Band::MHz915;
    ImGui::SameLine();
    ImGui::Checkbox("CRC##flags", &flags.crc_enabled);
    ImGui::SameLine();
    ImGui::Checkbox("E-stop##flags", &flags.estop_enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Heartbeat##flags", &flags.heartbeat_enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Scan##flags", &flags.channel_scan);
    flags.radio_type = gfsk ? RadioType::Gfsk : RadioType::LoRa;
    flags.one_to_one = true;
}
void lora_rows(LoraConfig& config) {
    LoRaParamFrame frame{};
    frame.command = SystemCmd::ParamWriteReq;
    frame.transaction_id = 1;
    frame.config = config;
    const Bytes preview = frame.to_bytes();
    fixed_row("Byte0", "System command", preview, 0, 1, "0x05 write request");
    fixed_row("Byte1-4", "Transaction ID", preview, 1, 4, "preview ID 1 (automatic)");
    fixed_row("Byte5-6", "Object Index", preview, 5, 2, "0x0000 radio config");
    fixed_row("Byte7-10", "Object Data", preview, 7, 4, "0 for radio config");
    table_row_start("Byte11-12", "Parameter flags");
    flags_editor(config.flags, false);
    table_row_end("LoRa / band / CRC / E-stop / heartbeat / scan");
    table_row_start("Byte13-14", "TX power (dBm)");
    input_i16("##lora-power", config.tx_power);
    table_row_end("0-10 @433; 0-20 @915");
    table_row_start("Byte15-16", "Frequency offset (kHz)");
    input_u16("##lora-offset", config.frequency_offset);
    table_row_end("center frequency offset");
    table_row_start("Byte17", "Payload length");
    input_u8("##lora-payload", config.payload_length);
    table_row_end("fixed 12 bytes");
    table_row_start("Byte18", "RSSI threshold raw");
    input_u8("##lora-rssi", config.rssi_threshold);
    table_row_end("10-148; threshold = -raw dBm");
    table_row_start("Byte19-20", "Heartbeat interval (ms)");
    input_u16("##lora-heartbeat", config.heartbeat_interval);
    table_row_end("200-10000");
    table_row_start("Byte21", "Heartbeat loss threshold");
    input_u8("##lora-loss", config.heartbeat_loss);
    table_row_end("1-255 packets");
    table_row_start("Byte22", "Receive bandwidth");
    input_u8("##lora-bandwidth", config.bandwidth);
    table_row_end("0=125, 1=250, 2=500 kHz");
    table_row_start("Byte23", "Spreading factor");
    input_u8("##lora-sf", config.spreading_factor);
    table_row_end("SF5-SF12");
    table_row_start("Byte24", "Coding rate");
    input_u8("##lora-coding", config.coding_rate);
    table_row_end("0-6 protocol enumeration");
    table_row_start("Byte25", "Header type");
    input_u8("##lora-header", config.header_type);
    table_row_end("0=explicit; 1=implicit");
    table_row_start("Byte26", "Preamble length");
    input_u8("##lora-preamble", config.preamble_length);
    table_row_end("10-50; SF5/SF6 require 12");
    table_row_start("Byte27-28", "Sync word");
    input_u16("##lora-sync", config.sync_word);
    table_row_end("0xY4X4");
    fixed_row("Byte29-38", "Reserved", preview, 29, 10, "fixed 0");
    fixed_row("Byte39", "Result code", preview, 39, 1, "fixed 0");
    fixed_row("Byte40-41", "CRC16-XMODEM", preview, 40, 2, "calculated automatically");
}
void gfsk_rows(GfskConfig& config) {
    GfskParamFrame frame{};
    frame.command = SystemCmd::ParamWriteReq;
    frame.transaction_id = 1;
    frame.config = config;
    const Bytes preview = frame.to_bytes();
    fixed_row("Byte0", "System command", preview, 0, 1, "0x05 write request");
    fixed_row("Byte1-4", "Transaction ID", preview, 1, 4, "preview ID 1 (automatic)");
    fixed_row("Byte5-6", "Object Index", preview, 5, 2, "0x0000 radio config");
    fixed_row("Byte7-10", "Object Data", preview, 7, 4, "0 for radio config");
    table_row_start("Byte11-12", "Parameter flags");
    flags_editor(config.flags, true);
    table_row_end("GFSK / band / CRC / E-stop / heartbeat / scan");
    table_row_start("Byte13-14", "TX power (dBm)");
    input_i16("##gfsk-power", config.tx_power);
    table_row_end("0-10 @433; 0-20 @915");
    table_row_start("Byte15-16", "Frequency offset (kHz)");
    input_u16("##gfsk-offset", config.frequency_offset);
    table_row_end("center frequency offset");
    table_row_start("Byte17", "Payload length");
    input_u8("##gfsk-payload", config.payload_length);
    table_row_end("fixed 12 bytes");
    table_row_start("Byte18", "RSSI threshold raw");
    input_u8("##gfsk-rssi", config.rssi_threshold);
    table_row_end("10-148; threshold = -raw dBm");
    table_row_start("Byte19-20", "Heartbeat interval (ms)");
    input_u16("##gfsk-heartbeat", config.heartbeat_interval);
    table_row_end("200-10000");
    table_row_start("Byte21", "Heartbeat loss threshold");
    input_u8("##gfsk-loss", config.heartbeat_loss);
    table_row_end("1-255 packets");
    table_row_start("Byte22", "Receive bandwidth");
    input_u8("##gfsk-bandwidth", config.bandwidth);
    table_row_end("0=117.3, 1=234.3, 2=467 kHz");
    table_row_start("Byte23-26", "Bit rate (bps)");
    input_u32("##gfsk-rate", config.bitrate);
    table_row_end("600-150000");
    table_row_start("Byte27-30", "Frequency deviation (Hz)");
    input_u32("##gfsk-deviation", config.frequency_deviation);
    table_row_end("600-300000; 2*deviation/rate >= 0.5");
    table_row_start("Byte31", "Pulse shaping");
    input_u8("##gfsk-pulse", config.pulse_shaping);
    table_row_end("0x00, 0x08-0x0B");
    table_row_start("Byte32", "Preamble length (bit)");
    input_u8("##gfsk-preamble", config.preamble_length);
    table_row_end("16-255");
    table_row_start("Byte33-34", "Sync word");
    input_u16("##gfsk-sync", config.sync_word);
    table_row_end("16-bit value");
    fixed_row("Byte35-38", "Reserved", preview, 35, 4, "fixed 0");
    fixed_row("Byte39", "Result code", preview, 39, 1, "fixed 0");
    fixed_row("Byte40-41", "CRC16-XMODEM", preview, 40, 2, "calculated automatically");
}
template <typename DrawRows>
void parameter_table(const char* id, DrawRows&& draw_rows) {
    if (!ImGui::BeginTable(
            id, 4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
            ImVec2(0, -48)))
        return;
    ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 88);
    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 190);
    ImGui::TableSetupColumn("Value / control", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Protocol rule", ImGuiTableColumnFlags_WidthFixed, 250);
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
        "Complete 42-byte configuration write-frame preview. Only device parameters are editable.");
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
    ImGui::TextDisabled("%s", index);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(name);
    ImGui::TableSetColumnIndex(2);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText((std::string("##") + index).c_str(), value.data(), value.size());
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
            "sdo_table", 4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_Resizable,
            ImVec2(0, -48))) {
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 220);
        ImGui::TableSetupColumn("Simulator value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Access / rule", ImGuiTableColumnFlags_WidthFixed, 185);
        ImGui::TableHeadersRow();
        table_row_start("0x001", "Product code");
        input_u32("##product", draft.identity.product_code, true);
        table_row_end("serial: RO; GUI: editable");
        table_row_start("0x002", "Version number");
        input_u32("##version", draft.identity.version_number, true);
        table_row_end("serial: RO; GUI: editable");
        table_row_start("0x003", "Serial number");
        input_u32("##serial", draft.identity.serial_number, true);
        table_row_end("serial: RO; GUI: editable");
        fixed_row("0x004-0x007", "Identity reserved", Bytes{}, 0, 4, "fixed 0");
        sdo_text_input("0x008-0x011", "App firmware version", draft.app_version);
        sdo_text_input("0x012-0x01B", "Bootloader firmware version", draft.boot_version);
        sdo_text_input("0x01C-0x025", "App branch name", draft.app_branch);
        sdo_text_input("0x026-0x02F", "App tag SHA1 ID", draft.app_tag);
        sdo_text_input("0x030-0x039", "Boot branch name", draft.boot_branch);
        sdo_text_input("0x03A-0x043", "Boot tag SHA1 ID", draft.boot_tag);
        fixed_row("0x044-0x0FF", "Reserved", Bytes{}, 0, 4, "fixed 0");
        table_row_start("0x102", "Battery (%)");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::SliderInt("##battery", &draft.battery, 0, 100);
        table_row_end("serial: RO; GUI: editable");
        table_row_start("0x202", "Upgrade request");
        input_u32("##upgrade", draft.upgrade_request, true);
        table_row_end("serial write accepts 0x0000454E only");
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
    ImGui::TextColored(k_accent, "PROTOCOL HISTORY");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu records", snapshot.history.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear history")) (void)simulator.clear_history();
    if (ImGui::BeginTable(
            "history", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY,
            ImVec2(0, -130))) {
        for (const char* name : {"Dir", "Cmd", "TxID", "Object", "Result", "CRC", "Note"})
            ImGui::TableSetupColumn(name);
        ImGui::TableHeadersRow();
        for (const auto& record : snapshot.history) {
            const auto& frame = record.frame;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const ImVec4 color = record.direction == FrameDirection::Received      ? k_accent
                                 : record.direction == FrameDirection::Transmitted ? k_good
                                                                                   : k_warn;
            ImGui::TextColored(color, "%s", direction_name(record.direction));
            ImGui::TableNextColumn();
            ImGui::Text("0x%02X", frame[0]);
            ImGui::TableNextColumn();
            ImGui::Text("%u", frame.read_le32(1));
            ImGui::TableNextColumn();
            ImGui::Text("0x%04X", frame.read_le16(5));
            ImGui::TableNextColumn();
            ImGui::Text("0x%02X", frame[39]);
            ImGui::TableNextColumn();
            ImGui::Text("%s", frame.has_valid_crc() ? "valid" : "bad");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(record.note.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Latest raw frame");
    if (snapshot.history.empty()) {
        ImGui::TextDisabled("No frames recorded. Connect a Host client to inspect traffic.");
        return;
    }
    const auto& frame = snapshot.history.back().frame;
    for (std::size_t i = 0; i < frame.size(); ++i) {
        ImGui::Text("%02X", frame[i]);
        if ((i + 1) % 14 != 0 && i + 1 != frame.size()) ImGui::SameLine();
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
