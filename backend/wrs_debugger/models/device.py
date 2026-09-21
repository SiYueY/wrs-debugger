from pydantic import Field

from wrs_debugger.models.base import ApiModel


class SerialPortInfo(ApiModel):
    device: str = Field(min_length=1)
    description: str | None = None
    manufacturer: str | None = None
    product: str | None = None
    serial_number: str | None = None
    vendor_id: int | None = Field(default=None, ge=0, le=65535)
    product_id: int | None = Field(default=None, ge=0, le=65535)


class SerialPortListResponse(ApiModel):
    ports: list[SerialPortInfo]


class TransmitterInfo(ApiModel):
    device_id: str | None = None
    product_code: int | None = Field(default=None, ge=0)
    version_number: int | None = Field(default=None, ge=0)
    serial_number: int | None = Field(default=None, ge=0)


class ReceiverInfo(ApiModel):
    bound_device_id: str | None = None


class PinResponse(ApiModel):
    pin: str = Field(pattern=r"^[0-9]{6}$")


class WritePinRequest(ApiModel):
    pin: str = Field(pattern=r"^[0-9]{6}$")
