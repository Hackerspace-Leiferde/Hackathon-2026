"""
Flashes ESP_SR's speech-recognition model blob (srmodels.bin — the "Hi ESP"
wake word + English command-recognition model) to the "model" partition
defined in partitions.csv, right after the normal firmware upload.

This is a separate binary from firmware.bin: `pio run -t upload` only
flashes the app partition, so without this step the ESP_SR runtime would
find an empty "model" partition and voice control would silently never
detect anything. srmodels.bin itself ships inside the pioarduino
framework package (not part of this repo — no large binary in git).
"""

Import("env")

import os


MODEL_PARTITION_OFFSET = "0x310000"  # must match the "model" row in partitions.csv


def flash_sr_models(source, target, env):
    platform = env.PioPlatform()
    libs_dir = platform.get_package_dir("framework-arduinoespressif32-libs")
    if not libs_dir:
        print("flash_sr_models.py: framework-arduinoespressif32-libs package not found — skipping model flash")
        return

    model_bin = os.path.join(libs_dir, "esp32s3", "esp_sr", "srmodels.bin")
    if not os.path.isfile(model_bin):
        print("flash_sr_models.py: srmodels.bin not found at %s — skipping model flash" % model_bin)
        return

    esptool_dir = platform.get_package_dir("tool-esptoolpy")
    esptool_py = os.path.join(esptool_dir, "esptool.py") if esptool_dir else "esptool.py"

    # By the time this post-upload action runs, PlatformIO has already
    # auto-detected the port for the main firmware upload, so $UPLOAD_PORT
    # is resolved to a real device path here.
    upload_port = env.subst("$UPLOAD_PORT")
    upload_speed = env.subst("$UPLOAD_SPEED") or "460800"

    cmd = [
        env.subst("$PYTHONEXE"),
        esptool_py,
        "--chip", "esp32s3",
        "--port", upload_port,
        "--baud", upload_speed,
        "write_flash", MODEL_PARTITION_OFFSET, model_bin,
    ]

    env.Execute(
        env.VerboseAction(
            " ".join('"%s"' % c if " " in c else c for c in cmd),
            "Flashing speech-recognition model (srmodels.bin) to 'model' partition...",
        )
    )


env.AddPostAction("upload", flash_sr_models)
