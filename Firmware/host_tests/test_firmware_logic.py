import unittest
from dataclasses import dataclass


SHELL_MAX_ARGS = 16

APP_SETTINGS_VERSION = 1
APP_AUDIO_DEFAULT_VOLUME = 6
APP_RGB_DEFAULT_BRIGHTNESS = 8
APP_RGB_BRINGUP_BRIGHTNESS_LIMIT = 24
APP_DEFAULT_SCENE = 0
APP_MOTOR_INTENSITY_LIMIT_DEFAULT = 25
APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE = 50
APP_BRINGUP_MOTOR_ARM_MAX_MS = 10_000
APP_MOTOR_COMMAND_TIMEOUT_MS = 500
APP_AUDIO_COMMAND_TIMEOUT_MS = 500

WT2003_START = 0x7E
WT2003_END = 0xEF
WT2003_MAX_PARAMS = 32
WT2003_CMD_STOP = 0xAB
WT2003_CMD_PAUSE = 0xAA
WT2003_CMD_PLAY_EXT_ROOT_INDEX = 0xA0
WT2003_CMD_PLAY_EXT_ROOT_NAME = 0xA1
WT2003_CMD_QUERY_VERSION = 0xC0
WT2003_CMD_QUERY_VOLUME = 0xC1
WT2003_CMD_QUERY_STATUS = 0xC2
WT2003_CMD_QUERY_PERIPHERAL_STATUS = 0xCA
WT2003_CMD_VOLUME = 0xAE

MOTOR_COAST = 0
MOTOR_FORWARD = 1
MOTOR_REVERSE = 2
MOTOR_BRAKE = 3

AUDIO_HW_NOT_TESTED = 0
AUDIO_HW_BLOCKED = 1
AUDIO_HW_UART_READY = 2
AUDIO_HW_ERROR = 3

AUDIO_STATUS_OK = 0
AUDIO_STATUS_HW_NOT_TESTED = 2
AUDIO_STATUS_UART_READY = 3
AUDIO_STATUS_TIMEOUT = 4
AUDIO_STATUS_PROTOCOL_ERROR = 5
AUDIO_STATUS_NOT_READY = 9


def wt2003_build_frame(cmd: int, params: bytes = b"") -> bytes:
    if len(params) > WT2003_MAX_PARAMS:
        raise ValueError("too many params")
    length = 1 + len(params) + 1 + 1
    checksum = (length + cmd + sum(params)) & 0xFF
    return bytes([WT2003_START, length, cmd]) + params + bytes([checksum, WT2003_END])


class Wt2003Parser:
    WAIT_START = 0
    LENGTH = 1
    PAYLOAD = 2
    END = 3

    def __init__(self):
        self.state = self.WAIT_START
        self.length = 0
        self.payload = bytearray()
        self.started_ms = 0

    def reset(self):
        self.state = self.WAIT_START
        self.length = 0
        self.payload.clear()
        self.started_ms = 0

    def feed(self, byte: int, now_ms: int):
        if byte == WT2003_START:
            previous = self.state
            self.reset()
            self.state = self.LENGTH
            self.started_ms = now_ms
            return ("framing_error", None) if previous == self.END else ("none", None)

        if self.state == self.WAIT_START:
            return "none", None
        if self.state == self.LENGTH:
            if byte < 3 or byte > WT2003_MAX_PARAMS + 3:
                self.reset()
                return "length_error", None
            self.length = byte
            self.payload.clear()
            self.state = self.PAYLOAD
            return "none", None
        if self.state == self.PAYLOAD:
            self.payload.append(byte)
            if len(self.payload) >= self.length - 1:
                self.state = self.END
            return "none", None
        if self.state == self.END:
            if byte != WT2003_END:
                self.reset()
                return "framing_error", None
            checksum = (self.length + sum(self.payload[:-1])) & 0xFF
            if checksum != self.payload[-1]:
                self.reset()
                return "checksum_error", None
            frame = {
                "command": self.payload[0],
                "params": bytes(self.payload[1:-1]),
                "checksum": self.payload[-1],
            }
            self.reset()
            return "frame", frame
        raise AssertionError("bad parser state")

    def timeout(self, now_ms: int, timeout_ms: int):
        if self.state != self.WAIT_START and now_ms - self.started_ms >= timeout_ms:
            self.reset()
            return "timeout"
        return "none"


def shell_tokenize(line: str):
    return line.split()[:SHELL_MAX_ARGS]


def crc16_settings(payload: bytes) -> int:
    crc = 0xFFFF
    for byte in payload:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


@dataclass
class Settings:
    version: int = APP_SETTINGS_VERSION
    volume: int = APP_AUDIO_DEFAULT_VOLUME
    brightness: int = APP_RGB_DEFAULT_BRIGHTNESS
    default_scene: int = APP_DEFAULT_SCENE
    motor_intensity_limit: int = APP_MOTOR_INTENSITY_LIMIT_DEFAULT
    demo_mode: int = 0
    reserved: int = 0
    crc: int = 0

    def payload_without_crc(self) -> bytes:
        return bytes(
            [
                self.version,
                self.volume,
                self.brightness,
                self.default_scene,
                self.motor_intensity_limit,
                self.demo_mode,
                self.reserved & 0xFF,
                (self.reserved >> 8) & 0xFF,
            ]
        )

    def finalize(self):
        self.reserved = 0
        self.crc = crc16_settings(self.payload_without_crc())
        return self

    def valid(self) -> bool:
        if self.version != APP_SETTINGS_VERSION:
            return False
        if self.volume > 30:
            return False
        if self.brightness > APP_RGB_BRINGUP_BRIGHTNESS_LIMIT:
            return False
        if self.motor_intensity_limit > APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE:
            return False
        if self.demo_mode > 1:
            return False
        return self.crc == crc16_settings(self.payload_without_crc())


def settings_factory_reset() -> Settings:
    return Settings().finalize()


def settings_set_or_recover(candidate: Settings) -> Settings:
    return candidate if candidate.valid() else settings_factory_reset()


@dataclass
class MotorModel:
    armed_until_ms: int = 0
    mode: int = MOTOR_COAST
    duty: int = 0
    expires_ms: int = 0
    sleep: int = 0

    def arm(self, now_ms: int, duration_ms: int):
        duration_ms = min(duration_ms, APP_BRINGUP_MOTOR_ARM_MAX_MS)
        self.mode = MOTOR_COAST
        self.duty = 0
        self.expires_ms = 0
        self.sleep = 1
        self.armed_until_ms = now_ms + duration_ms

    def off(self):
        self.armed_until_ms = 0
        self.mode = MOTOR_COAST
        self.duty = 0
        self.expires_ms = 0
        self.sleep = 0

    def is_armed(self, now_ms: int) -> bool:
        return self.armed_until_ms != 0 and now_ms < self.armed_until_ms

    def command_for(self, now_ms: int, mode: int, duty: int, duration_ms: int) -> int:
        if duty == 0 and mode != MOTOR_BRAKE:
            mode = MOTOR_COAST
        if mode != MOTOR_COAST and not self.is_armed(now_ms):
            self.off()
            return -2
        if duty > APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE:
            self.off()
            return -3
        if mode != MOTOR_COAST and duration_ms == 0:
            return -4
        duration_ms = min(duration_ms, APP_MOTOR_COMMAND_TIMEOUT_MS)
        self.mode = mode
        self.duty = duty
        self.expires_ms = 0 if mode == MOTOR_COAST else now_ms + duration_ms
        return 0

    def poll(self, now_ms: int):
        if self.armed_until_ms and now_ms >= self.armed_until_ms:
            self.off()
            return
        if self.mode != MOTOR_COAST and self.expires_ms and now_ms >= self.expires_ms:
            self.mode = MOTOR_COAST
            self.duty = 0
            self.expires_ms = 0


def limit_brightness(value: int) -> int:
    return min(value, APP_RGB_BRINGUP_BRIGHTNESS_LIMIT)


def scale_rgb(value: int, brightness: int) -> int:
    return (value * limit_brightness(brightness)) // 255


@dataclass
class AudioModel:
    hw_state: int = AUDIO_HW_NOT_TESTED
    pending: bool = False
    last_error: int = AUDIO_STATUS_HW_NOT_TESTED
    deadline_ms: int = 0
    ready_after_ms: int = 1000

    def start_command(self, now_ms: int):
        if now_ms < self.ready_after_ms:
            self.pending = False
            self.last_error = AUDIO_STATUS_NOT_READY
            return AUDIO_STATUS_NOT_READY
        self.hw_state = AUDIO_HW_UART_READY
        self.pending = True
        self.last_error = AUDIO_STATUS_UART_READY
        self.deadline_ms = now_ms + APP_AUDIO_COMMAND_TIMEOUT_MS
        return AUDIO_STATUS_OK

    def play(self, now_ms: int):
        if self.hw_state == AUDIO_HW_ERROR:
            self.pending = False
            self.last_error = AUDIO_STATUS_PROTOCOL_ERROR
            return AUDIO_STATUS_PROTOCOL_ERROR
        return self.start_command(now_ms)

    def poll(self, now_ms: int):
        if self.pending and self.deadline_ms and now_ms >= self.deadline_ms:
            self.pending = False
            self.hw_state = AUDIO_HW_ERROR
            self.last_error = AUDIO_STATUS_TIMEOUT


class ShellTests(unittest.TestCase):
    def test_tokenize_basic_commands(self):
        self.assertEqual(shell_tokenize("motor pwm A forward 10 100"), ["motor", "pwm", "A", "forward", "10", "100"])

    def test_tokenize_limits_argument_count(self):
        self.assertEqual(len(shell_tokenize("a b c d e f g h i j k l m n o p q r")), SHELL_MAX_ARGS)


class SettingsTests(unittest.TestCase):
    def test_defaults(self):
        settings = settings_factory_reset()
        self.assertEqual(settings.version, APP_SETTINGS_VERSION)
        self.assertEqual(settings.volume, APP_AUDIO_DEFAULT_VOLUME)
        self.assertEqual(settings.brightness, APP_RGB_DEFAULT_BRIGHTNESS)
        self.assertEqual(settings.default_scene, APP_DEFAULT_SCENE)
        self.assertEqual(settings.motor_intensity_limit, APP_MOTOR_INTENSITY_LIMIT_DEFAULT)
        self.assertTrue(settings.valid())

    def test_crc_changes_when_payload_changes(self):
        settings = settings_factory_reset()
        original_crc = settings.crc
        settings.brightness += 1
        self.assertNotEqual(original_crc, crc16_settings(settings.payload_without_crc()))
        self.assertFalse(settings.valid())

    def test_corruption_recovers_to_defaults(self):
        settings = settings_factory_reset()
        settings.crc ^= 0x5A5A
        recovered = settings_set_or_recover(settings)
        self.assertTrue(recovered.valid())
        self.assertEqual(recovered.brightness, APP_RGB_DEFAULT_BRIGHTNESS)


class MotorTests(unittest.TestCase):
    def test_non_coast_requires_arm(self):
        motor = MotorModel()
        self.assertEqual(motor.command_for(100, MOTOR_FORWARD, 10, 100), -2)
        self.assertEqual(motor.mode, MOTOR_COAST)

    def test_zero_duty_forward_becomes_coast_without_arm(self):
        motor = MotorModel()
        self.assertEqual(motor.command_for(100, MOTOR_FORWARD, 0, 100), 0)
        self.assertEqual(motor.mode, MOTOR_COAST)

    def test_duty_limit_rejects_command(self):
        motor = MotorModel()
        motor.arm(0, 1000)
        self.assertEqual(motor.command_for(10, MOTOR_FORWARD, APP_BRINGUP_MOTOR_DUTY_LIMIT_PERMILLE + 1, 100), -3)
        self.assertEqual(motor.sleep, 0)

    def test_arm_enables_sleep_and_off_disables_sleep(self):
        motor = MotorModel()
        motor.arm(0, 1000)
        self.assertEqual(motor.sleep, 1)
        motor.off()
        self.assertEqual(motor.sleep, 0)

    def test_arm_timeout_disables_sleep(self):
        motor = MotorModel()
        motor.arm(0, 1000)
        motor.poll(999)
        self.assertEqual(motor.sleep, 1)
        motor.poll(1000)
        self.assertEqual(motor.sleep, 0)

    def test_command_timeout_coasts_output(self):
        motor = MotorModel()
        motor.arm(0, 1000)
        self.assertEqual(motor.command_for(10, MOTOR_REVERSE, 10, 100), 0)
        motor.poll(109)
        self.assertEqual(motor.mode, MOTOR_REVERSE)
        motor.poll(110)
        self.assertEqual(motor.mode, MOTOR_COAST)

    def test_command_duration_is_capped(self):
        motor = MotorModel()
        motor.arm(0, 10_000)
        self.assertEqual(motor.command_for(10, MOTOR_FORWARD, 10, 5000), 0)
        self.assertEqual(motor.expires_ms, 10 + APP_MOTOR_COMMAND_TIMEOUT_MS)


class RgbTests(unittest.TestCase):
    def test_brightness_limit(self):
        self.assertEqual(limit_brightness(255), APP_RGB_BRINGUP_BRIGHTNESS_LIMIT)

    def test_scale_uses_limited_brightness(self):
        self.assertEqual(scale_rgb(255, 255), APP_RGB_BRINGUP_BRIGHTNESS_LIMIT)


class AudioTests(unittest.TestCase):
    def test_audio_command_waits_for_power_on_delay(self):
        audio = AudioModel()
        self.assertEqual(audio.play(999), AUDIO_STATUS_NOT_READY)
        self.assertFalse(audio.pending)
        self.assertEqual(audio.last_error, AUDIO_STATUS_NOT_READY)

    def test_audio_play_after_power_on_delay(self):
        audio = AudioModel()
        self.assertEqual(audio.play(1000), AUDIO_STATUS_OK)
        self.assertTrue(audio.pending)

    def test_audio_command_timeout(self):
        audio = AudioModel()
        self.assertEqual(audio.start_command(1000), AUDIO_STATUS_OK)
        audio.poll(1499)
        self.assertTrue(audio.pending)
        audio.poll(1500)
        self.assertFalse(audio.pending)
        self.assertEqual(audio.hw_state, AUDIO_HW_ERROR)
        self.assertEqual(audio.last_error, AUDIO_STATUS_TIMEOUT)


class Wt2003ProtocolTests(unittest.TestCase):
    def assert_frame(self, cmd, params, expected_hex):
        expected = bytes.fromhex(expected_hex)
        self.assertEqual(wt2003_build_frame(cmd, params), expected)

    def test_datasheet_stop_frame(self):
        self.assert_frame(WT2003_CMD_STOP, b"", "7E 03 AB AE EF")

    def test_datasheet_pause_frame(self):
        self.assert_frame(WT2003_CMD_PAUSE, b"", "7E 03 AA AD EF")

    def test_datasheet_external_flash_root_index_1_frame(self):
        self.assert_frame(WT2003_CMD_PLAY_EXT_ROOT_INDEX, bytes([0x00, 0x01]), "7E 05 A0 00 01 A6 EF")

    def test_datasheet_external_flash_root_name_0001_frame(self):
        self.assert_frame(WT2003_CMD_PLAY_EXT_ROOT_NAME, b"0001", "7E 07 A1 30 30 30 31 69 EF")

    def test_datasheet_query_frames(self):
        self.assert_frame(WT2003_CMD_QUERY_VERSION, b"", "7E 03 C0 C3 EF")
        self.assert_frame(WT2003_CMD_QUERY_VOLUME, b"", "7E 03 C1 C4 EF")
        self.assert_frame(WT2003_CMD_QUERY_STATUS, b"", "7E 03 C2 C5 EF")

    def test_datasheet_volume_31_frame(self):
        self.assert_frame(WT2003_CMD_VOLUME, bytes([31]), "7E 04 AE 1F D1 EF")

    def test_encoder_rejects_overlong_params(self):
        with self.assertRaises(ValueError):
            wt2003_build_frame(WT2003_CMD_VOLUME, bytes(range(WT2003_MAX_PARAMS + 1)))

    def parse_all(self, frame: bytes):
        parser = Wt2003Parser()
        result = ("none", None)
        for i, byte in enumerate(frame):
            result = parser.feed(byte, i)
        return result

    def test_parser_valid_ack_frame(self):
        kind, frame = self.parse_all(bytes.fromhex("7E 04 AB 00 AF EF"))
        self.assertEqual(kind, "frame")
        self.assertEqual(frame["command"], WT2003_CMD_STOP)
        self.assertEqual(frame["params"], b"\x00")

    def test_parser_valid_query_frame(self):
        kind, frame = self.parse_all(bytes.fromhex("7E 04 C2 02 C8 EF"))
        self.assertEqual(kind, "frame")
        self.assertEqual(frame["command"], WT2003_CMD_QUERY_STATUS)
        self.assertEqual(frame["params"], b"\x02")

    def test_parser_checksum_error(self):
        kind, frame = self.parse_all(bytes.fromhex("7E 04 AB 00 00 EF"))
        self.assertEqual(kind, "checksum_error")
        self.assertIsNone(frame)

    def test_parser_wrong_end_byte(self):
        kind, frame = self.parse_all(bytes.fromhex("7E 04 AB 00 AF 00"))
        self.assertEqual(kind, "framing_error")
        self.assertIsNone(frame)

    def test_parser_rejects_impossible_length(self):
        parser = Wt2003Parser()
        parser.feed(WT2003_START, 0)
        kind, frame = parser.feed(0x01, 1)
        self.assertEqual(kind, "length_error")
        self.assertIsNone(frame)

    def test_parser_resync_after_garbage(self):
        parser = Wt2003Parser()
        last = ("none", None)
        stream = bytes.fromhex("00 FF 12 7E 03 C0 C3 EF")
        for i, byte in enumerate(stream):
            last = parser.feed(byte, i)
        self.assertEqual(last[0], "frame")
        self.assertEqual(last[1]["command"], WT2003_CMD_QUERY_VERSION)

    def test_parser_timeout_with_partial_frame(self):
        parser = Wt2003Parser()
        parser.feed(WT2003_START, 100)
        parser.feed(0x04, 101)
        self.assertEqual(parser.timeout(599, APP_AUDIO_COMMAND_TIMEOUT_MS), "none")
        self.assertEqual(parser.timeout(600, APP_AUDIO_COMMAND_TIMEOUT_MS), "timeout")

    def test_parser_unsolicited_peripheral_status_frame(self):
        kind, frame = self.parse_all(bytes.fromhex("7E 04 CA 05 D3 EF"))
        self.assertEqual(kind, "frame")
        self.assertEqual(frame["command"], WT2003_CMD_QUERY_PERIPHERAL_STATUS)
        self.assertEqual(frame["params"], b"\x05")


if __name__ == "__main__":
    unittest.main()
