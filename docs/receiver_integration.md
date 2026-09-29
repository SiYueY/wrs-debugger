# Receiver integration status

The debugger keeps the current Receiver page and REST contract intact. Its production
receiver transport uses the current binding and LoRa DDS services plus the `rt/wireless_estop_state` topic from
`humanoid-driver/function/wirelessestop`. It does not access the receiver's internal
RS485 bus directly.

| Receiver page operation | Tool interface | Current driver service | Device verification |
| --- | --- | --- | --- |
| Bound device ID | Implemented | `GetEStopBindingState` | Pending |
| LoRa read/write | Implemented | `GetEStopConfig` / `SetEStopConfig` | Pending |
| LoRa restore | Write protocol defaults, then read back | `SetEStopConfig` / `GetEStopConfig` | Pending |
| LoRa sync from transmitter | Existing operation, now using DDS write | `SetEStopConfig` | Pending |
| Factory binding | Serial Bind and FIND plus DDS Start Binding and state check | `StartEStopBinding` / `GetEStopBindingState` | Pending |
| GFSK read/write/restore/sync | Preserved; returns `RECEIVER_UNSUPPORTED` | Not provided | Pending |
| Receiver SDO read/write | Preserved; returns `RECEIVER_UNSUPPORTED` | Not provided | Pending |

The native client also exposes DDS status samples. It rejects samples marked invalid;
status is not used for robot safety control. The current driver leaves
`binding_transaction_id` unset in the binding state reply, so verification uses the
bound flag, device ID, and serial FIND result. The driver does not provide a safe
receiver-side cancellation service. After a receiver binding request may have reached
the driver, a failed factory binding reports rollback failure for manual inspection
instead of issuing an unsafe unbind.

Build the native module with `cmake -S wrs -B build/native`. The receiver always
uses DDS and requires `HUMANOID_DRIVER_ROOT` (for `thirdparty/fastdds` and
`thirdparty/dds_wrapper`) and `GENERIC_BIN_DIR`. The four service types and
state topic generated C++ files are copied into `wrs/src/receiver/dds/generated/`;
`WIRELESS_ESTOP_ROOT` is not a build dependency. Their source is
`humanoid-driver/function/wirelessestop/common/dds/idl_generated` at commit
`82b7d25bc5cd47c27418ce190ffbbfc8be3aba5c`. Update the copied files
manually when the driver interface changes. The desktop packaging script supplies
`HUMANOID_DRIVER_ROOT` from the sibling driver checkout by default.
For an unfrozen backend, set
`WRS_DDS_PROFILE` to a Fast DDS XML profile; the frozen backend sets it to its
bundled profile. The selected Receiver Domain ID is passed to the DDS participant.

Native and backend tests plus the frozen desktop build are automated. Hardware
interoperability for LoRa and factory binding, and future driver services for GFSK
and SDO, remain separate acceptance gates.
