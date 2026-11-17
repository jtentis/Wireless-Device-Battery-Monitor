# Keychron K5 Max (ANSI RGB) firmware with battery reporting

`k5_max_ansi_rgb_battery.bin` lets the keyboard report its battery level through the Keychron Link 2.4 GHz receiver, which `src/drivers/keychron.cpp` reads.

- Source: [Keychron/qmk_firmware](https://github.com/Keychron/qmk_firmware) branch `2025q3` (commit `57e682c`), plus the `KC_GET_BATTERY` patch from [ldkv/qmk_firmware#1](https://github.com/ldkv/qmk_firmware/pull/1), plus `#define WILRESS_RAW_ENABLE` and `#define KC_BATTERY_MODEL_ID 0` in `keyboards/keychron/k5_max/config.h`.
- The complete source change is in [`k5_max_battery.patch`](k5_max_battery.patch): apply it to that commit with `git apply`.
- Build target: `make keychron/k5_max/ansi/rgb:keychron` (QMK Docker image `ghcr.io/qmk/qmk_cli`).
- SHA256: `5971a3429ca38805e1dc40b97a9fc9faf0ae1be59e673dc03acbb74eeb277abd`
- License: GPL-2.0-or-later (QMK). Anyone who receives the binary must also be able to get the source above.

## Flashing

1. Set the mode switch to **Cable** and unplug the USB cable.
2. Hold **Esc** while plugging the cable in. The keyboard enters the STM32 bootloader.
3. Flash the `.bin` with [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases).

Updating the keyboard from Keychron Launcher replaces this firmware and removes battery reporting. To undo it, flash Keychron's official firmware the same way.
