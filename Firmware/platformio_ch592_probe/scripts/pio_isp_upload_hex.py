Import("env")
import os
from SCons.Script import Default


def project_path(option_value):
    path = env.subst(option_value)
    if not os.path.isabs(path):
        path = os.path.join(env.subst("$PROJECT_DIR"), path)
    return os.path.normpath(path)


if env.subst("$UPLOAD_PROTOCOL") == "isp":
    external_hex = env.GetProjectOption("custom_wchisp_hex_path", "").strip()

    if external_hex:
        firmware_hex = project_path(external_hex)
    else:
        firmware_hex = env.subst("$BUILD_DIR/${PROGNAME}.hex")

        # The community CH32V platform uploads the ELF for ISP by default.
        # MounRiver and known CH592 wchisp examples flash Intel HEX files,
        # which preserve the intended load addresses without asking wchisp to
        # interpret ELF sections.
        hex_target = env.ElfToHex(firmware_hex, "$BUILD_DIR/${PROGNAME}.elf")
        Default(hex_target)
        env.Depends("upload", firmware_hex)

    quoted_hex = firmware_hex.replace("\\", "/")
    env.Replace(UPLOADCMD='$UPLOADER -v $UPLOADERFLAGS flash "%s"' % quoted_hex)

if env.GetProjectOption("custom_wlink_download_upload", "no").lower() in (
    "1",
    "true",
    "yes",
    "on",
):
    external_image = env.GetProjectOption("custom_wlink_image", "").strip()
    wlink_exe = env.GetProjectOption("custom_wlink_exe", "wlink").strip()
    chip = env.GetProjectOption("custom_wlink_chip", "CH59X").strip()
    speed = env.GetProjectOption("custom_wlink_speed", "low").strip()
    erase = env.GetProjectOption("custom_wlink_erase", "yes").lower() in (
        "1",
        "true",
        "yes",
        "on",
    )
    no_run = env.GetProjectOption("custom_wlink_no_run", "no").lower() in (
        "1",
        "true",
        "yes",
        "on",
    )

    if external_image:
        firmware_image = project_path(external_image)
    else:
        firmware_image = env.subst("$BUILD_DIR/${PROGNAME}.hex")
        hex_target = env.ElfToHex(firmware_image, "$BUILD_DIR/${PROGNAME}.elf")
        Default(hex_target)
        env.Depends("upload", firmware_image)

    if wlink_exe != "wlink" or "/" in wlink_exe or "\\" in wlink_exe:
        wlink_exe = project_path(wlink_exe)
        wlink_command = '"%s"' % wlink_exe
    else:
        wlink_command = wlink_exe

    upload_parts = [
        wlink_command,
        "flash",
        "--chip",
        chip,
        "--speed",
        speed,
    ]

    if erase:
        upload_parts.append("--erase")

    if no_run:
        upload_parts.append("--no-run")

    upload_parts.append('"%s"' % firmware_image)
    env.Replace(UPLOADCMD=" ".join(upload_parts))
