from typing import Annotated, Literal

from pydantic import BaseModel, Field, model_validator


class LoRaPhyConfig(BaseModel):
    modulation: Literal["lora"]
    bandwidth_hz: Literal[125000, 250000, 500000]
    spreading_factor: int = Field(ge=5, le=12)
    coding_rate: Literal["4/5", "4/6", "4/7", "4/8", "LI4/5", "LI4/6", "LI4/8"]
    header_type: Literal["explicit", "implicit"]
    preamble_length: int = Field(ge=10, le=50)
    sync_word: int = Field(ge=0, le=65535)

    @model_validator(mode="after")
    def validate_short_sf_preamble(self) -> "LoRaPhyConfig":
        if self.spreading_factor in {5, 6} and self.preamble_length != 12:
            raise ValueError("SF5 and SF6 require a preamble length of 12")
        return self


class GfskPhyConfig(BaseModel):
    modulation: Literal["gfsk"]
    receive_bandwidth_hz: Literal[117300, 234300, 467000]
    bit_rate_bps: int = Field(ge=600, le=150000)
    frequency_deviation_hz: int = Field(ge=600, le=300000)
    pulse_shaping: Literal["none", "bt_0_3", "bt_0_5", "bt_0_7", "bt_1_0"]
    preamble_length: int = Field(ge=16, le=255)
    sync_word: int = Field(ge=0, le=65535)

    @model_validator(mode="after")
    def validate_deviation_ratio(self) -> "GfskPhyConfig":
        if 2 * self.frequency_deviation_hz / self.bit_rate_bps < 0.5:
            raise ValueError("frequency deviation to bit rate ratio must be at least 0.5")
        return self


PhyConfig = Annotated[LoRaPhyConfig | GfskPhyConfig, Field(discriminator="modulation")]


class RadioConfig(BaseModel):
    frequency_band_mhz: Literal[433, 915]
    tx_power_dbm: int = Field(ge=0, le=20)
    frequency_offset_hz: int
    rssi_threshold_dbm: int
    heartbeat_interval_ms: int = Field(gt=0)
    heartbeat_loss_threshold: int = Field(ge=1)
    channel_scan_enabled: bool
    group_mode_enabled: bool
    heartbeat_enabled: bool
    wireless_estop_enabled: bool
    phy_crc_enabled: bool
    phy: PhyConfig

    @model_validator(mode="after")
    def validate_band_power(self) -> "RadioConfig":
        if self.frequency_band_mhz == 433 and self.tx_power_dbm > 10:
            raise ValueError("433 MHz TX power cannot exceed 10 dBm")
        return self


DEFAULT_RADIO_CONFIG = RadioConfig(
    frequency_band_mhz=433,
    tx_power_dbm=10,
    frequency_offset_hz=250000,
    rssi_threshold_dbm=-110,
    heartbeat_interval_ms=200,
    heartbeat_loss_threshold=3,
    channel_scan_enabled=True,
    group_mode_enabled=False,
    heartbeat_enabled=True,
    wireless_estop_enabled=True,
    phy_crc_enabled=True,
    phy=LoRaPhyConfig(
        modulation="lora",
        bandwidth_hz=250000,
        spreading_factor=6,
        coding_rate="4/5",
        header_type="explicit",
        preamble_length=12,
        sync_word=5156,
    ),
)
