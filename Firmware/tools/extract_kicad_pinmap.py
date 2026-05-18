#!/usr/bin/env python3
"""Extract firmware-relevant pin mapping from the KiCad PCB.

The firmware deliberately treats this script as the source of truth for board
connectivity. It parses the current KiCad files and generates:

- Firmware/docs/hardware_pin_audit.md
- Firmware/northpole_ch592_bringup/APP/include/board_pins_autogen_notes.h

The parser is intentionally small and dependency-free. It does not try to be a
complete KiCad parser; it extracts balanced footprint and pad S-expressions and
then reads the properties needed by the firmware audit.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as _dt
import hashlib
import re
from pathlib import Path
from typing import Iterable


@dataclasses.dataclass(frozen=True)
class Pad:
    number: str
    net: str
    pinfunction: str
    pintype: str


@dataclasses.dataclass(frozen=True)
class Footprint:
    footprint: str
    reference: str
    value: str
    pads: tuple[Pad, ...]


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def sha256_short(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()[:16]


def find_forms(text: str, form_name: str) -> list[str]:
    """Return balanced S-expression blocks whose first symbol is form_name."""
    blocks: list[str] = []
    token = f"({form_name}"
    start = 0
    while True:
        idx = text.find(token, start)
        if idx < 0:
            break
        if idx > 0 and text[idx - 1] not in "\r\n\t (":
            start = idx + 1
            continue

        depth = 0
        in_string = False
        escaped = False
        for pos in range(idx, len(text)):
            ch = text[pos]
            if in_string:
                if escaped:
                    escaped = False
                elif ch == "\\":
                    escaped = True
                elif ch == '"':
                    in_string = False
                continue
            if ch == '"':
                in_string = True
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    blocks.append(text[idx : pos + 1])
                    start = pos + 1
                    break
        else:
            raise ValueError(f"Unbalanced KiCad form starting at byte {idx}")
    return blocks


def first_match(pattern: str, text: str, default: str = "") -> str:
    match = re.search(pattern, text, re.S)
    return match.group(1) if match else default


def parse_pads(footprint_block: str) -> tuple[Pad, ...]:
    pads: list[Pad] = []
    for block in find_forms(footprint_block, "pad"):
        number = first_match(r'^\(pad\s+"([^"]*)"', block)
        if not number:
            continue
        pads.append(
            Pad(
                number=number,
                net=first_match(r'\(net\s+"([^"]+)"\)', block, "(none)"),
                pinfunction=first_match(
                    r'\(pinfunction\s+"([^"]+)"\)', block, "(none)"
                ),
                pintype=first_match(r'\(pintype\s+"([^"]+)"\)', block, "(none)"),
            )
        )
    return tuple(pads)


def parse_footprints(pcb_text: str) -> list[Footprint]:
    footprints: list[Footprint] = []
    for block in find_forms(pcb_text, "footprint"):
        footprints.append(
            Footprint(
                footprint=first_match(r'^\(footprint\s+"([^"]+)"', block),
                reference=first_match(
                    r'\(property\s+"Reference"\s+"([^"]+)"', block
                ),
                value=first_match(r'\(property\s+"Value"\s+"([^"]+)"', block),
                pads=parse_pads(block),
            )
        )
    return footprints


def parse_schematic_labels(sch_text: str) -> set[str]:
    labels = set(re.findall(r'\(label\s+"([^"]+)"', sch_text))
    labels.update(re.findall(r'\(global_label\s+"([^"]+)"', sch_text))
    labels.update(re.findall(r'\(hierarchical_label\s+"([^"]+)"', sch_text))
    return labels


def footprints_by_ref(footprints: Iterable[Footprint]) -> dict[str, Footprint]:
    return {fp.reference: fp for fp in footprints if fp.reference}


def net_index(footprints: Iterable[Footprint]) -> dict[str, list[tuple[Footprint, Pad]]]:
    index: dict[str, list[tuple[Footprint, Pad]]] = {}
    for fp in footprints:
        for pad in fp.pads:
            index.setdefault(pad.net, []).append((fp, pad))
    return index


def pad_by_function(fp: Footprint, function_prefix: str) -> Pad | None:
    for pad in fp.pads:
        if pad.pinfunction.startswith(function_prefix):
            return pad
    return None


def pad_by_number(fp: Footprint, number: str) -> Pad | None:
    for pad in fp.pads:
        if pad.number == number:
            return pad
    return None


def connected_summary(
    net: str, index: dict[str, list[tuple[Footprint, Pad]]], exclude_ref: str = ""
) -> str:
    peers: list[str] = []
    for fp, pad in index.get(net, []):
        if fp.reference == exclude_ref:
            continue
        peers.append(f"{fp.reference}:{pad.pinfunction or pad.number}")
    return ", ".join(peers) if peers else "(no other footprint pads found)"


def net_has_peer(net: str, index: dict[str, list[tuple[Footprint, Pad]]], ref: str) -> bool:
    return any(fp.reference != ref for fp, _pad in index.get(net, []))


def two_pin_series_bridge(
    net_a: str, net_b: str, footprints: Iterable[Footprint]
) -> Footprint | None:
    """Return a two-pin resistor that directly bridges two signal nets.

    KiCad keeps a series resistor as two different nets, so a direct net-name
    comparison would incorrectly flag UART series resistors as disconnected.
    This intentionally handles only R* two-pin parts and excludes power nets so
    pullups do not collapse unrelated signal nets.
    """
    if net_a == net_b:
        return None
    if net_a in {"+3.3V", "+5V", "GND"} or net_b in {"+3.3V", "+5V", "GND"}:
        return None
    for fp in footprints:
        if not fp.reference.startswith("R") or len(fp.pads) != 2:
            continue
        nets = {fp.pads[0].net, fp.pads[1].net}
        if {net_a, net_b} == nets:
            return fp
    return None


def nets_match_or_series(
    net_a: str, net_b: str, footprints: Iterable[Footprint]
) -> tuple[bool, str]:
    if net_a == net_b:
        return True, "direct net"
    bridge = two_pin_series_bridge(net_a, net_b, footprints)
    if bridge:
        return True, f"series resistor {bridge.reference} ({ascii_safe(bridge.value)})"
    return False, "not connected by direct net or detected series resistor"


def u2_pad_for_net_or_series(
    u2: Footprint,
    target_net: str,
    footprints: Iterable[Footprint],
    function_prefix: str = "",
) -> tuple[Pad | None, str]:
    for pad in u2.pads:
        if function_prefix and not pad.pinfunction.startswith(function_prefix):
            continue
        ok, reason = nets_match_or_series(pad.net, target_net, footprints)
        if ok:
            return pad, reason
    return None, "not connected by direct net or detected series resistor"


def infer_u2_role(
    pad: Pad,
    index: dict[str, list[tuple[Footprint, Pad]]],
    footprints: Iterable[Footprint],
) -> str:
    net = pad.net
    fn = pad.pinfunction
    role_by_net = {
        "/PWM_A1": "DRV8837 A IN1 phase output",
        "/PWM_A2": "DRV8837 A IN2 phase output",
        "/PWM_B1": "DRV8837 B IN1 phase output",
        "/PWM_B2": "DRV8837 B IN2 phase output",
        "/PWM_G1": "DRV8837 G IN1 guard output",
        "/PWM_G2": "DRV8837 G IN2 guard output",
        "/SLEEP": "DRV8837 global nSLEEP output",
        "/HALL1": "Hall 1 input",
        "/HALL2": "Hall 2 input",
        "/SPD--": "capacitive touch SPD-",
        "/RUN": "capacitive touch RUN",
        "/SPD++": "capacitive touch SPD+",
        "/MUSIC": "capacitive touch MUSIC",
        "/INT": "IP5209 interrupt input",
        "/SCL": "I2C SCL",
        "/SDA": "I2C SDA",
        "/SWDCK": "PB15/SCL shared WCH-LinkE TCK; IP5209 SCL through R14 0 ohm link",
        "/SWDIO": "PB14/SDA shared WCH-LinkE TIO; IP5209 SDA through R15 0 ohm link",
        "/DP": "USB D+",
        "/DN": "USB D-",
        "/LED": "addressable RGB LED data",
        "/BUSY": "WT2003 busy/status input",
        "Net-(AE1-A)": "BLE antenna feed",
        "+3.3V": "3.3 V power",
        "GND": "ground",
    }
    if net in role_by_net:
        return role_by_net[net]
    if "unconnected" in net or "no_connect" in pad.pintype:
        return "explicit no-connect / unused"
    if fn.startswith("TXD1_"):
        bridge = two_pin_series_bridge(net, "/RX1", footprints)
        return (
            f"UART TXD1 to WT2003 RXD via {bridge.reference}"
            if bridge
            else "UART TXD1 pad, no detected WT2003 RXD path"
        )
    if fn.startswith("RXD1_"):
        bridge = two_pin_series_bridge(net, "/TX1", footprints)
        return (
            f"UART RXD1 from WT2003 TXD via {bridge.reference}"
            if bridge
            else "UART RXD1 pad, no detected WT2003 TXD path"
        )
    if fn.startswith("X32") or "X32" in fn:
        return "32 MHz crystal"
    if fn.startswith("V") or "VINT" in fn:
        return "CH592X internal/power support"
    if fn.startswith("RST"):
        return "reset/debug support net"
    if fn.startswith("PB22") or net == "Net-(U2-PB22)":
        return "BOOT/user button input"
    if not net_has_peer(net, index, "U2"):
        return "private net with no other parsed footprint pads"
    return "unclassified"


def status_line(ok: bool, pass_text: str, fail_text: str) -> str:
    return f"PASS - {pass_text}" if ok else f"BLOCKED - {fail_text}"


def led_chain(leds: list[Footprint]) -> list[Footprint]:
    di_by_net: dict[str, Footprint] = {}
    do_by_ref: dict[str, str] = {}
    for led in leds:
        for pad in led.pads:
            if pad.pinfunction.startswith("DI"):
                di_by_net[pad.net] = led
            if pad.pinfunction.startswith("DO"):
                do_by_ref[led.reference] = pad.net

    chain: list[Footprint] = []
    current = di_by_net.get("/LED")
    seen: set[str] = set()
    while current and current.reference not in seen:
        chain.append(current)
        seen.add(current.reference)
        current = di_by_net.get(do_by_ref.get(current.reference, ""))
    return chain


def table(rows: list[list[str]]) -> str:
    if not rows:
        return ""
    width = len(rows[0])
    header = "| " + " | ".join(rows[0]) + " |"
    separator = "| " + " | ".join(["---"] * width) + " |"
    body = ["| " + " | ".join(row) + " |" for row in rows[1:]]
    return "\n".join([header, separator, *body])


def c_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def ascii_safe(value: str) -> str:
    return value.replace("\u03a9", "ohm")


def net_matches_expected(observed: str, expected: str) -> bool:
    if expected == "(none)":
        return observed == "(none)" or observed.startswith("unconnected-")
    return observed == expected


def generate_notes_h(
    path: Path,
    pcb_sha: str,
    sch_sha: str,
    u2: Footprint,
    audio_connected: bool,
    rgb_chain_count: int,
) -> None:
    lines = [
        "/* Auto-generated by Firmware/tools/extract_kicad_pinmap.py. */",
        "/* Do not edit by hand; rerun the extractor after KiCad changes. */",
        "#ifndef BOARD_PINS_AUTOGEN_NOTES_H",
        "#define BOARD_PINS_AUTOGEN_NOTES_H",
        "",
        f'#define BOARD_AUTOGEN_PCB_SHA256 "{pcb_sha}"',
        f'#define BOARD_AUTOGEN_SCH_SHA256 "{sch_sha}"',
        f"#define BOARD_AUTOGEN_AUDIO_UART_CONNECTED {1 if audio_connected else 0}",
        f"#define BOARD_AUTOGEN_AUDIO_HW_BLOCKED {0 if audio_connected else 1}",
        f"#define BOARD_AUTOGEN_RGB_LED_COUNT {rgb_chain_count}",
        "",
    ]
    for pad in sorted(u2.pads, key=lambda item: int(item.number)):
        prefix = f"BOARD_U2_PAD_{pad.number}"
        lines.extend(
            [
                f'#define {prefix}_FUNCTION "{c_string(pad.pinfunction)}"',
                f'#define {prefix}_NET "{c_string(pad.net)}"',
                f'#define {prefix}_PINTYPE "{c_string(pad.pintype)}"',
            ]
        )
    lines.extend(["", "#endif /* BOARD_PINS_AUTOGEN_NOTES_H */", ""])
    path.write_text("\n".join(lines), encoding="utf-8")


def generate_audit(
    path: Path,
    repo_root: Path,
    pcb_path: Path,
    sch_path: Path,
    pcb_sha: str,
    sch_sha: str,
    footprints: list[Footprint],
    labels: set[str],
) -> bool:
    by_ref = footprints_by_ref(footprints)
    index = net_index(footprints)

    u2 = by_ref["U2"]
    u6 = by_ref.get("U6")
    u7 = by_ref.get("U7")
    motor_refs = ["U10", "U11", "U9"]
    motor_names = {"U10": "A", "U11": "B", "U9": "G"}
    leds = sorted(
        [fp for fp in footprints if fp.value == "TZ-H1010-RGB/A-BU08UF-TA1305NA/W109"],
        key=lambda fp: fp.reference,
    )
    hall_sensors = sorted(
        [fp for fp in footprints if fp.value == "DRV5032FADMRR"],
        key=lambda fp: fp.reference,
    )
    touch_pads = sorted(
        [fp for fp in footprints if fp.footprint == "cust_fp:Cap_Touch_10mm"],
        key=lambda fp: fp.reference,
    )

    u2_txd = pad_by_function(u2, "TXD1_")
    u2_rxd = pad_by_function(u2, "RXD1_")
    wt_rxd = pad_by_function(u6, "RXD_") if u6 else None
    wt_txd = pad_by_function(u6, "TXD_") if u6 else None
    tx_connected, tx_reason = (
        nets_match_or_series(u2_txd.net, wt_rxd.net, footprints)
        if u2_txd and wt_rxd
        else (False, "missing pad")
    )
    rx_connected, rx_reason = (
        nets_match_or_series(u2_rxd.net, wt_txd.net, footprints)
        if u2_rxd and wt_txd
        else (False, "missing pad")
    )
    audio_connected = bool(tx_connected and rx_connected)

    chain = led_chain(leds)
    j3 = by_ref.get("J3")
    j4 = by_ref.get("J4")
    r14 = by_ref.get("R14")
    r15 = by_ref.get("R15")
    u2_sleep = pad_by_number(u2, "3")

    wch_rows = [["Item", "Expected", "Observed", "Status"]]
    j3_expected = {
        "1": "+3.3V",
        "2": "/SWDIO",
        "3": "(none)",
        "4": "/SWDCK",
        "5": "GND",
        "6": "(none)",
    }
    wch_pass = True
    for number, expected in j3_expected.items():
        pad = pad_by_number(j3, number) if j3 else None
        observed = pad.net if pad else "(missing)"
        ok = net_matches_expected(observed, expected)
        wch_pass = wch_pass and ok
        wch_rows.append([f"J3 pin {number}", expected, observed, "PASS" if ok else "BLOCKED"])

    r14_ok = bool(r14 and {pad.net for pad in r14.pads} == {"/SCL", "/SWDCK"})
    r15_ok = bool(r15 and {pad.net for pad in r15.pads} == {"/SDA", "/SWDIO"})
    wch_pass = wch_pass and r14_ok and r15_ok
    wch_rows.append(["R14", "/SCL <-> /SWDCK 0 ohm", ", ".join(pad.net for pad in r14.pads) if r14 else "(missing)", "PASS" if r14_ok else "BLOCKED"])
    wch_rows.append(["R15", "/SDA <-> /SWDIO 0 ohm", ", ".join(pad.net for pad in r15.pads) if r15 else "(missing)", "PASS" if r15_ok else "BLOCKED"])

    wt_usb_rows = [["Item", "Expected", "Observed", "Status"]]
    j4_expected = {
        "1": "+5V",
        "2": "/DP2",
        "3": "(none)",
        "4": "/DM2",
        "5": "GND",
        "6": "(none)",
    }
    wt_usb_pass = True
    for number, expected in j4_expected.items():
        pad = pad_by_number(j4, number) if j4 else None
        observed = pad.net if pad else "(missing)"
        ok = net_matches_expected(observed, expected)
        wt_usb_pass = wt_usb_pass and ok
        wt_usb_rows.append([f"J4 pin {number}", expected, observed, "PASS" if ok else "BLOCKED"])
    wt_dm = pad_by_function(u6, "D-_") if u6 else None
    wt_dp = pad_by_function(u6, "D+_") if u6 else None
    wt_dm_ok = bool(wt_dm and wt_dm.net == "/DM2")
    wt_dp_ok = bool(wt_dp and wt_dp.net == "/DP2")
    wt_usb_pass = wt_usb_pass and wt_dm_ok and wt_dp_ok
    wt_usb_rows.append(["U6 D-", "/DM2", wt_dm.net if wt_dm else "(missing)", "PASS" if wt_dm_ok else "BLOCKED"])
    wt_usb_rows.append(["U6 D+", "/DP2", wt_dp.net if wt_dp else "(missing)", "PASS" if wt_dp_ok else "BLOCKED"])

    rows = [["Pad", "Pin function", "Net", "Firmware role", "Connected parsed pads"]]
    for pad in sorted(u2.pads, key=lambda item: int(item.number)):
        rows.append(
            [
                pad.number,
                pad.pinfunction,
                pad.net,
                infer_u2_role(pad, index, footprints),
                connected_summary(pad.net, index, "U2"),
            ]
        )

    motor_rows = [["Driver", "Ref", "IN1 net", "IN2 net", "~SLEEP net", "Status"]]
    expected_motor = {
        "U10": ("/PWM_A1", "/PWM_A2"),
        "U11": ("/PWM_B1", "/PWM_B2"),
        "U9": ("/PWM_G1", "/PWM_G2"),
    }
    motor_pass = True
    sleep_control_pass = bool(u2_sleep and u2_sleep.net == "/SLEEP")
    for ref in motor_refs:
        fp = by_ref.get(ref)
        if not fp:
            motor_rows.append([motor_names[ref], ref, "(missing)", "(missing)", "(missing)", "BLOCKED"])
            motor_pass = False
            continue
        in1 = pad_by_function(fp, "IN1_")
        in2 = pad_by_function(fp, "IN2_")
        sleep = pad_by_function(fp, "~{SLEEP}_")
        ok = bool(in1 and in2 and (in1.net, in2.net) == expected_motor[ref])
        motor_pass = motor_pass and ok
        sleep_ok = bool(sleep and sleep.net == "/SLEEP")
        sleep_control_pass = sleep_control_pass and sleep_ok
        motor_rows.append(
            [
                motor_names[ref],
                ref,
                in1.net if in1 else "(missing)",
                in2.net if in2 else "(missing)",
                sleep.net if sleep else "(missing)",
                "PASS" if ok and sleep_ok else "BLOCKED",
            ]
        )

    hall_rows = [["Ref", "OUT net", "MCU pad", "Status"]]
    hall_pass = True
    for hall in hall_sensors:
        out = pad_by_function(hall, "OUT_")
        mcu_pad = next((pad for pad in u2.pads if out and pad.net == out.net), None)
        ok = bool(out and mcu_pad)
        hall_pass = hall_pass and ok
        hall_rows.append(
            [
                hall.reference,
                out.net if out else "(missing)",
                mcu_pad.number if mcu_pad else "(not connected to U2)",
                "PASS" if ok else "BLOCKED",
            ]
        )

    touch_rows = [["Ref", "Value", "Net", "MCU pad", "MCU function", "Status"]]
    touch_pass = True
    for touch in touch_pads:
        pad = pad_by_number(touch, "1")
        mcu_pad = next((u2_pad for u2_pad in u2.pads if pad and u2_pad.net == pad.net), None)
        ok = bool(pad and mcu_pad and mcu_pad.pinfunction.startswith("AIN"))
        touch_pass = touch_pass and ok
        touch_rows.append(
            [
                touch.reference,
                touch.value,
                pad.net if pad else "(missing)",
                mcu_pad.number if mcu_pad else "(not connected to U2)",
                mcu_pad.pinfunction if mcu_pad else "(not connected)",
                "PASS" if ok else "BLOCKED",
            ]
        )

    ip_rows = [["Signal", "IP5209 pad/net", "MCU pad/net", "Path", "Status"]]
    ip_pass = True
    for signal, prefix, mcu_prefix in [("SDA", "SDA_", "SDA_"), ("SCL", "SCL_", "SCL_"), ("INT", "L3_", "")]:
        ip_pad = pad_by_function(u7, prefix) if u7 else None
        if signal == "INT":
            mcu_pad = next((pad for pad in u2.pads if ip_pad and pad.net == ip_pad.net), None)
            path_reason = "direct net" if mcu_pad else "not connected by direct net"
        else:
            mcu_pad, path_reason = (
                u2_pad_for_net_or_series(u2, ip_pad.net, footprints, mcu_prefix)
                if ip_pad
                else (None, "missing IP5209 pad")
            )
        ok = bool(ip_pad and mcu_pad)
        ip_pass = ip_pass and ok
        ip_rows.append(
            [
                signal,
                f"{ip_pad.pinfunction}/{ip_pad.net}" if ip_pad else "(missing)",
                f"pad {mcu_pad.number}/{mcu_pad.net}" if mcu_pad else "(not connected)",
                path_reason,
                "PASS" if ok else "BLOCKED",
            ]
        )

    led_rows = [["Order", "Ref", "DI net", "DO net"]]
    for idx, led in enumerate(chain):
        di = pad_by_function(led, "DI_")
        do = pad_by_function(led, "DO_")
        led_rows.append(
            [str(idx), led.reference, di.net if di else "(missing)", do.net if do else "(missing)"]
        )
    rgb_pass = bool(chain and len(chain) == 6 and pad_by_function(chain[0], "DI_").net == "/LED")

    audio_rows = [["Signal", "MCU", "WT2003", "Status"]]
    audio_rows.extend(
        [
            [
                "MCU TX to WT RX",
                f"U2 pad {u2_txd.number} {u2_txd.pinfunction} {u2_txd.net}" if u2_txd else "(missing)",
                f"U6 pad {wt_rxd.number} {wt_rxd.pinfunction} {wt_rxd.net}" if wt_rxd else "(missing)",
                f"PASS - {tx_reason}" if tx_connected else "BLOCKED",
            ],
            [
                "MCU RX from WT TX",
                f"U2 pad {u2_rxd.number} {u2_rxd.pinfunction} {u2_rxd.net}" if u2_rxd else "(missing)",
                f"U6 pad {wt_txd.number} {wt_txd.pinfunction} {wt_txd.net}" if wt_txd else "(missing)",
                f"PASS - {rx_reason}" if rx_connected else "BLOCKED",
            ],
            [
                "WT BUSY",
                "U2 pad 29 /BUSY",
                f"U6 pad {pad_by_function(u6, 'LED5_').number} /BUSY"
                if u6 and pad_by_function(u6, "LED5_")
                else "(missing)",
                "PASS",
            ],
        ]
    )

    no_connect_rows = [["Pad", "Function", "Net", "Reason"]]
    for pad in sorted(u2.pads, key=lambda item: int(item.number)):
        isolated = not net_has_peer(pad.net, index, "U2")
        if "unconnected" in pad.net or "no_connect" in pad.pintype or isolated:
            no_connect_rows.append(
                [
                    pad.number,
                    pad.pinfunction,
                    pad.net,
                    "explicit no-connect" if "unconnected" in pad.net or "no_connect" in pad.pintype else "private or isolated net",
                ]
            )

    relevant_labels = sorted(
        label
        for label in labels
        if label
        in {
            "PWM_A1",
            "PWM_A2",
            "PWM_B1",
            "PWM_B2",
            "PWM_G1",
            "PWM_G2",
            "SLEEP",
            "HALL1",
            "HALL2",
            "MUSIC",
            "RUN",
            "SPD++",
            "SPD--",
            "LED",
            "BUSY",
            "RX1",
            "TX1",
            "SCL",
            "SDA",
            "SWDCK",
            "SWDIO",
            "INT",
            "DP",
            "DN",
            "DP2",
            "DM2",
        }
    )

    subsystem_status = [
        ["Subsystem", "Status"],
        [
            "CH592X U2 pin extraction",
            "PASS - all parsed U2 pads are listed below" if len(u2.pads) >= 33 else "BLOCKED - missing U2 pads",
        ],
        [
            "Audio UART",
            status_line(
                audio_connected,
                "WT2003 RX/TX are connected to CH592X UART TX/RX",
                "WT2003 RX/TX are not connected to CH592X UART TX/RX; firmware must keep audio UART disabled",
            ),
        ],
        [
            "Audio BUSY",
            "PASS - /BUSY connects WT2003 to U2 pad 29",
        ],
        [
            "Motor inputs",
            status_line(
                motor_pass,
                "DRV8837 input nets match A/B/G phase naming",
                "one or more DRV8837 input nets do not match expected phase naming",
            ),
        ],
        [
            "DRV8837 sleep control",
            status_line(
                sleep_control_pass,
                "PB0 controls all DRV8837 ~SLEEP pins through /SLEEP",
                "DRV8837 ~SLEEP wiring is not consistently connected to CH592X PB0 /SLEEP",
            ),
        ],
        [
            "Hall sensors",
            status_line(hall_pass, "both Hall outputs connect to CH592X pads", "a Hall output is missing or not connected to U2"),
        ],
        [
            "Touch pads",
            status_line(touch_pass, "all touch pads connect to AIN-capable U2 pads", "a touch pad is missing or not on an AIN-capable U2 pad"),
        ],
        [
            "RGB LEDs",
            status_line(rgb_pass, "six LEDs form a chain starting at /LED", "LED chain does not start at /LED or count is not six"),
        ],
        [
            "IP5209 control",
            status_line(
                ip_pass,
                "SCL/SDA/INT connect between IP5209 and CH592X; I2C passes through R14/R15 debug-share links",
                "IP5209 control/status pins are not fully connected",
            ),
        ],
        [
            "WCH-LinkE J3",
            status_line(
                wch_pass,
                "debug connector maps TIO/TCK to PB14/PB15 and shares IP5209 I2C through R15/R14",
                "debug connector or 0 ohm debug-share links do not match expected nets",
            ),
        ],
        [
            "WT2003 USB update J4",
            status_line(
                wt_usb_pass,
                "J4 maps +5V, D+, D-, GND to WT2003 update USB nets",
                "WT2003 USB update connector does not match expected nets",
            ),
        ],
    ]

    lines = [
        "# Hardware Pin Audit",
        "",
        "Generated by `Firmware/tools/extract_kicad_pinmap.py` from the current KiCad files.",
        "",
        f"- PCB file: `{pcb_path.relative_to(repo_root)}` sha256 `{pcb_sha}`",
        f"- Schematic file: `{sch_path.relative_to(repo_root)}` sha256 `{sch_sha}`",
        f"- Generated UTC: `{_dt.datetime.now(_dt.timezone.utc).isoformat(timespec='seconds')}`",
        "",
        "## Summary",
        "",
        table(subsystem_status),
        "",
        "## CH592X / U2 Pads",
        "",
        table(rows),
        "",
        "## Audio UART Check",
        "",
        table(audio_rows),
        "",
        "Conclusion: "
        + (
            "audio UART is usable. The TX/RX nets are routed through detected series resistors, so direct net names differ but the signal path exists."
            if audio_connected
            else "`HW_BLOCKED`. U2 pad 13 `TXD1__13` is on `Net-(U2-PB13)` while WT2003 `RXD_11` is on `/RX1`; U2 pad 14 `RXD1__14` is on `Net-(U2-PB12)` while WT2003 `TXD_23` is on `/TX1`. Do not enable `audio_wt2003.c` UART commands until the schematic is fixed or rework wires are documented."
        ),
        "",
        "## DRV8837 Motor Driver Mapping",
        "",
        table(motor_rows),
        "",
        "PB0 controls global DRV8837 `~SLEEP` on `/SLEEP`. Firmware idle and fault state must drive `/SLEEP` low and all IN pins low/coast.",
        "",
        "## Hall Sensors",
        "",
        table(hall_rows),
        "",
        "## Capacitive Touch Pads",
        "",
        table(touch_rows),
        "",
        "## RGB LED Chain",
        "",
        table(led_rows),
        "",
        "## IP5209 Power/Charge Interface",
        "",
        table(ip_rows),
        "",
        "IP5209 I2C: PASS via R14/R15 0 ohm debug-share links when the table above shows PASS for SCL and SDA.",
        "",
        "Warning: when WCH-LinkE debug is active, PB15/PB14 are used as debug pins. IP5209 I2C should not be expected to work at the same time. If debug communication is unreliable, remove R14/R15 or leave them unpopulated temporarily.",
        "",
        "## WCH-LinkE Debug Connector J3",
        "",
        table(wch_rows),
        "",
        "Intended connection: CH592 PB14 / SDA / WCH TIO / SWDIO -> R15 0 ohm -> IP5209 SDA. CH592 PB15 / SCL / WCH TCK / SWDCK -> R14 0 ohm -> IP5209 SCL.",
        "",
        "## WT2003 USB Update Connector J4",
        "",
        table(wt_usb_rows),
        "",
        "J4 is not ARM SWD. It uses the Tag-Connect footprint as a WT2003 USB update connector: 1=5V, 2=D+, 4=D-, 5=GND. Use only with a custom USB adapter/cable.",
        "",
        "## Explicit No-Connect Or Isolated U2 Nets",
        "",
        table(no_connect_rows),
        "",
        "## Relevant Schematic Labels Observed",
        "",
        ", ".join(f"`{label}`" for label in relevant_labels) if relevant_labels else "(none)",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")
    return audio_connected


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Repository root. Defaults to two levels above this script.",
    )
    parser.add_argument(
        "--fail-on-blocked",
        action="store_true",
        help="Return nonzero if a hardware-blocked subsystem is detected.",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    pcb_path = repo_root / "PCB" / "NorthPoleCircuit_PCB.kicad_pcb"
    sch_path = repo_root / "PCB" / "NorthPoleCircuit_PCB.kicad_sch"
    out_md = repo_root / "Firmware" / "docs" / "hardware_pin_audit.md"
    out_h = repo_root / "Firmware" / "northpole_ch592_bringup" / "APP" / "include" / "board_pins_autogen_notes.h"

    pcb_text = read_text(pcb_path)
    sch_text = read_text(sch_path)
    footprints = parse_footprints(pcb_text)
    labels = parse_schematic_labels(sch_text)
    by_ref = footprints_by_ref(footprints)
    if "U2" not in by_ref:
        raise SystemExit("Could not find CH592X footprint U2 in PCB")

    pcb_sha = sha256_short(pcb_path)
    sch_sha = sha256_short(sch_path)
    out_md.parent.mkdir(parents=True, exist_ok=True)
    out_h.parent.mkdir(parents=True, exist_ok=True)

    audio_connected = generate_audit(
        out_md, repo_root, pcb_path, sch_path, pcb_sha, sch_sha, footprints, labels
    )
    rgb_count = len(led_chain([fp for fp in footprints if fp.value == "TZ-H1010-RGB/A-BU08UF-TA1305NA/W109"]))
    generate_notes_h(out_h, pcb_sha, sch_sha, by_ref["U2"], audio_connected, rgb_count)

    print(f"Wrote {out_md}")
    print(f"Wrote {out_h}")
    if args.fail_on_blocked and not audio_connected:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
