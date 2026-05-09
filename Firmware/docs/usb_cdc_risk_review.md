# USB CDC Risk Review

Status: `NEEDS_HARDWARE_TEST`.

The custom CDC shell is intentionally small and has not enumerated on hardware yet. Do not treat USB CDC as proven until a CH592 dev board or target board passes `Firmware/tools/usb_shell_smoke_test.py`.

## References Compared

Official WCH examples checked:

```text
C:\WCH\CH592EVT\EVT\EXAM\USB\Device\COM\src\Main.c
C:\WCH\CH592EVT\EVT\EXAM\BLE\BLE_USB\APP\app_usb.c
```

North Pole implementation:

```text
Firmware/northpole_ch592_bringup/APP/northpole/usb_cdc_shell.c
```

## Descriptor Parity

| Area | WCH USB Device COM | North Pole CDC shell | Review |
|---|---|---|---|
| Device descriptor | CDC class `0x02`, VID `0x1A86`, PID `0x8040`, EP0 64 bytes | Same class, VID/PID, EP0 size | `MATCHES_WCH_EXAMPLE` except product version/string values |
| Configuration total length | `0x0043` | `0x0043` | `MATCHES_WCH_EXAMPLE` |
| Interfaces | CDC communication interface 0 and data interface 1 | Same | `MATCHES_WCH_EXAMPLE` |
| CDC ACM descriptors | Header, ACM, union, call-management descriptors | Same byte layout | `MATCHES_WCH_EXAMPLE` |
| Interrupt IN endpoint | `0x84`, interrupt, 8 bytes | `0x84`, interrupt, 8 bytes | `INTENTIONAL_DIFFERENCES`: interval is `0x10` instead of WCH `0x01`; endpoint is left NAK-only |
| Bulk OUT endpoint | `0x01`, bulk, 64 bytes | Same | `MATCHES_WCH_EXAMPLE` |
| Bulk IN endpoint | `0x81`, bulk, 64 bytes | Same | `MATCHES_WCH_EXAMPLE` |
| Strings | WCH manufacturer/product/serial | North Pole manufacturer/product/serial | `INTENTIONAL_DIFFERENCES` |

## Control Transfers

| Request | WCH COM example | North Pole behavior | Review |
|---|---|---|---|
| `GET_DESCRIPTOR` | Device/config/string handled over EP0 chunks | Same basic EP0 chunking | `MATCHES_WCH_EXAMPLE` |
| `SET_ADDRESS` | Address applied after status IN | Same | `MATCHES_WCH_EXAMPLE` |
| `SET_CONFIGURATION` | Sets configured state and endpoint ACK/NAK state | Same for EP1/EP4 used by CDC shell | `INTENTIONAL_DIFFERENCES`: does not configure unused EP2/EP3 |
| `GET_CONFIGURATION` | Returns current config | Same | `MATCHES_WCH_EXAMPLE` |
| `GET_INTERFACE` / `GET_STATUS` | Returns zero status | Same | `MATCHES_WCH_EXAMPLE` |
| `CLEAR_FEATURE` | Clears endpoint halt/toggle for supported endpoints | Simplified for EP1 data endpoint | `INTENTIONAL_DIFFERENCES` |
| `SET_LINE_CODING` | Accepts 7-byte CDC line coding | Same | `MATCHES_WCH_EXAMPLE` |
| `GET_LINE_CODING` | Returns stored line coding | Same | `MATCHES_WCH_EXAMPLE` |
| `SET_CONTROL_LINE_STATE` | ACKs request | Same | `MATCHES_WCH_EXAMPLE` |

The custom code does not implement WCH CH341/vendor-mode requests from the standalone COM example. That is intentional because this firmware exposes CDC ACM only.

## Endpoint And DMA Setup

| Register/setup | WCH COM example | North Pole behavior | Review |
|---|---|---|---|
| `R8_UEP4_1_MOD` | Enables EP4 TX, EP1 TX, EP1 RX | Same | `MATCHES_WCH_EXAMPLE` |
| `R8_UEP2_3_MOD` | Also enables EP2 and EP3 for vendor/extra paths | `0x00` | `INTENTIONAL_DIFFERENCES`: unused endpoints disabled |
| `R16_UEP0_DMA` | EP0 buffer | EP0 buffer | `MATCHES_WCH_EXAMPLE` |
| `R16_UEP1_DMA` | 128-byte EP1 OUT/IN buffer | 128-byte EP1 OUT/IN buffer | `MATCHES_WCH_EXAMPLE` |
| `R16_UEP2_DMA` / `R16_UEP3_DMA` | Allocated in WCH examples for extra endpoints | Only `R16_UEP3_DMA` is assigned to an EP4 placeholder buffer | `NEEDS_HARDWARE_TEST` |
| EP4 interrupt buffer | WCH examples do not actively use CDC notification payloads | North Pole keeps EP4 NAK-only | `NEEDS_HARDWARE_TEST` |

The EP4 notification endpoint is the highest-risk mismatch. The descriptor exposes it because Windows CDC ACM expects a notification endpoint, but the firmware does not send serial-state notifications. This should still be acceptable for many hosts, but enumeration must be tested on Windows and Linux.

## Interrupt And Data Path

| Area | WCH behavior | North Pole behavior | Review |
|---|---|---|---|
| USB ISR model | Standalone COM queues IRQ events; BLE_USB handles transfer work in `USB_DevTransProcess()` | Custom ISR handles setup, EP0, and EP1 directly | `INTENTIONAL_DIFFERENCES` |
| EP1 OUT | Copies received packet to an application buffer | Pushes packet bytes into a nonblocking shell RX ring | `INTENTIONAL_DIFFERENCES` |
| EP1 IN | Copies application data to EP1 IN buffer and ACKs | Pulls from shell TX ring and ACKs | `INTENTIONAL_DIFFERENCES` |
| Blocking behavior | Example code is demo-oriented | Shell drops TX when ring is full and polls RX | `INTENTIONAL_DIFFERENCES`, chosen to avoid blocking BLE/app safety |
| USB reset | Clears address and endpoint states | Clears address/configured state and endpoint states | `MATCHES_WCH_EXAMPLE` for active CDC endpoints |

## Enumeration Expectations

Expected later validation:

- Windows: appears as a USB serial COM port without a vendor driver requirement.
- Linux: appears as `cdc_acm`, typically `/dev/ttyACM*`.
- `version`, `status`, `pins verify`, and `safe check` work through the virtual COM port.
- Disconnect/reconnect and host USB reset do not leave motor pins or `/SLEEP` unsafe.

## Fallback If CDC Fails

1. Build with USB CDC disabled:

   ```powershell
   powershell -ExecutionPolicy Bypass -File Firmware\tools\build.ps1 -Profile bringup -ExtraDefine APP_USB_CDC_SHELL_ENABLE=0
   ```

2. Use BLE diagnostic reads for identity/status once BLE is validated.
3. Use a temporary UART fixture only if UART1 is isolated from WT2003.

## Decision

Do not rewrite the USB stack before hardware arrives. The implementation has descriptor parity for the CDC shape and intentional simplifications for the shell use case, but endpoint 4 notification handling and DMA/register behavior remain `NEEDS_HARDWARE_TEST`.
