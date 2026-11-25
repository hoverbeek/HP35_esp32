# HP-35 Phase 7.18 — power diagnostics and backlight control

**Built on merged and hardware-tested Phase 7.17.** The hardware QMI8658
interrupt wake, 7.16 polling fallback, and 7.15 RTC stack retention remain.

New UART0 commands:

- `power stats` — current-boot uptime, number and measured elapsed time of
  light-sleep timer episodes and interrupt-driven light-sleep episodes,
  GPIO/timer wake breakdown, sleep failures, RTC-retained deep-sleep
  entry/wake counters, backlight state.
- `power reset` — reset cumulative sleep statistics.
- `power backlight off` / `power backlight on` — control GPIO20 LED
  backlight while leaving calculation and LVGL tasks running. The preference
  is respected when returning from light sleep, but is not retained through
  a deep-sleep boot.

The stats structure is stored in RTC_DATA_ATTR, surviving normal timer-based
deep-sleep wake. Full power removal invalidates/clears it. Timing is measured
by the MCU and is **not** a current draw or battery-life measurement. Deep
sleep reports requested timer duration, NOT verified actual sleeping duration.
Do not infer milliamp consumption from software counters.

## Suggested hardware test

```
1 2 3 4
power stats
sleep motion irq 15
power stats
power backlight off
power stats
power backlight on
sleep deep 10
power stats
stack
```

The LCD should remain dark during motion sleep, GPIO wake should restore
the original stack, and deep-sleep wake should restore RTC state as before.
You can now separately measure backlight-on and backlight-off power using
an external USB power meter or series ammeter.

**Validation:** GitHub source changes prepared; PlatformIO compilation and
on-device behavior still need to be verified. `main` is unchanged.

---

# HP-35 Phase 7.17 — hardware motion interrupt (experimental)

7.16 remains on main. This feature branch adds `sleep motion irq 15`
using QMI8658A's hardware Wake-on-Motion mode (150 mg threshold),
interrupt INT1 -> ESP32-S3 GPIO46 and `esp_sleep_enable_gpio_wakeup()`.
The ESP32-S3 stays in **light sleep** until GPIO46 goes HIGH or a
safety timer expires. It does **not** wake from deep sleep.

## Test
1. Compile with PlatformIO; run `motion status`.
2. Run `sleep motion irq 15`. The LCD backlight should remain off.
3. Pick up the board. Expect `Hardware motion interrupt woke the calculator!`.
4. If the interrupt is already HIGH, the operation aborts safely.
5. If no interrupt arrives, the 15-second timer wakes the ESP32.
6. Verify stack and `motion sample` still work.
7. Existing `sleep motion 15` (7.16 polling) remains an unchanged fallback.

Status: **not compiled or tested on the physical board**. QMI8658 WoM
register initialization and GPIO interrupt polarity should be validated
against the hardware; do not merge until tested.

QMI8658 wake-on-motion config based on QST datasheet and SensorLib
documentation:
https://www.qstcorp.com/upload/pdf/202301/13-52-25%20QMI8658A%20Datasheet%20Rev%20A.pdf
https://raw.githubusercontent.com/lewisxhe/SensorLib/master/src/SensorQMI8658.hpp
https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html

---

# Phase 7.16 — QMI8658 motion-aware light sleep

- Motion sensor: QMI8658 at I2C GPIO47/48. INT1=GPIO46 and INT2=GPIO45.
- ESP32-S3 RTC deep-sleep wake supports GPIO0..21, so onboard IMU interrupts
  cannot directly wake this ESP32-S3 from deep sleep without extra hardware.
- New commands: motion status, motion sample, sleep motion 15.
- sleep motion N uses LIGHT sleep with 125 ms timer sampling, detects a change
  in accelerometer axis reading, and resumes the calculator.
- Existing 7.15 deep sleep with RTC register retention stays unchanged.
- First test: motion status, motion sample, then sleep motion 15 and lift board.
- Source not yet compiled on PlatformIO nor tested on real board.

References:
https://www.waveshare.com/wiki/ESP32-S3-LCD-1.3
https://spotpear.com/index.php/wiki/ESP32-S3-1.3-inch-LCD-ST7789-240x240-Display-Screen.html
https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html

---

# Phase 7.15 — ESP32-S3 sleep and RTC register retention

Based on Phase 7.14, with `sleep status`, `sleep light [seconds]`,
`sleep deep [seconds]`, and `sleep auto on/off` through Serial0 at 115200.

- Deep sleep stores BCD X/Y/Z/T, RPN entry-replacement status, error message,
  floating stack, active view and prism mode in RTC retention memory, protected
  by a version marker and checksum.
- Light sleep retains live RAM and returns from a timer wake without reboot.
- USB serial is **not** configured as a deep-sleep wake source. Timers are used.
- Auto sleep is experimental and disabled by default. Idle serial bytes can be
  missed while sleeping. Deep sleep automatically wakes after 10 seconds.
- This does **not** persist across battery disconnect or full power-off.
- Full dynamic CORDIC trace buffers are intentionally not persisted.
- Run `test-host/test_sleep_state.cpp` alongside the existing host tests.
- Firmware remains **untested on the physical board**; verify using PlatformIO.

---
# HP-35 Phase 7.14 — Isolated LVGL prism screen

**Fix:** Phase 7.13 mixed LVGL's partial-screen flushes with direct Arduino_GFX drawing. This could leave the debugger title/tabs visible over the rotating prism value, and corrupt text. Phase 7.14 uses a **second, all-black LVGL screen** and an `LV_EVENT_DRAW_MAIN` custom drawing callback for the same rotating bitmap glyphs. The only ST7789 writer is the existing LVGL flush callback.

- `display prism`: hardware rotation 180°, horizontal pre-mirror, black screen, continuously rotating yellow `X` and current result.
- `display normal`: returns to the original four colored LVGL tabs.
- Serial UART0 and BCD/CORDIC source remain unchanged.
- Source folder includes version number for VS Code identification.
- `HP35_USE_LVGL=0` retains the older direct display fallback; the **new fix requires LVGL enabled** (default).

## Test

1. Build/upload this folder with PlatformIO.
2. On the 115200-baud serial monitor run `display prism`.
3. Confirm the entire screen is black except the rotating X/result; the debugger header/tabs should **not** remain.
4. Run `2.02 ln exp` to update the value, then `display normal`, then `view regs`.
5. Repeat switching between `display prism` and `display normal` to check the transition.

**Validation note:** This source revision has not been compiled on the user's PC or run on the ESP32. The Phase 6 C++ engine was not changed.

# HP-35 ESP32-S3 Phase 7.13 — prism horizontal mirror

**Change from 7.12:** the prism animation is now horizontally pre-mirrored, in addition to the existing 180-degree LCD rotation. This corrects the left/right reversal seen through the supplied crystal prism. This mirror is implemented in the pixel projection for the rotating X/result text; it does **not** change the normal LVGL tabs. 

Commands remain `display prism` and `display normal`. Prism mode shows only the animated X/result against a true-black background. Internal Y/Z/T values remain unchanged.

The build has not been tested on physical ESP32 hardware. Keep your working 7.12 version available as fallback.

# HP-35 ESP32-S3 Phase 7.12 — Pepper’s Ghost prism view

This project is based on Phase 7.11. The existing BCD/CORDIC engine and its
four normal LVGL views are unchanged. The top-level directory now includes the
release version, as requested.

## Prism commands

- `display prism`: switch to a **fully black screen** with only the X register
  and result, rotating around a **vertical axis**. The LCD is rotated by 180°
  as in Phase 7.11 so the supplied prism's image appears upright.
- `display normal`: stop the animation, restore the original LCD orientation,
  and return to the normal colored LVGL four-tab debugger.
- Input calculations as usual, e.g. `2.02 ln exp`; the floating X display will
  update after the command completes. Y/Z/T remain intact but are not shown.

The rotating effect is drawn directly with Arduino_GFX (using a small native
5×7 bitmap font) and does not require a framebuffer. During this mode the
LVGL task does not draw, to prevent it overwriting the black animation.
The background is black, not physically transparent; the prism's reflections
produce the Pepper's Ghost illusion. Rotation takes about 4 seconds, refreshed
at up to ~25 frames/s. The back face intentionally appears mirrored as a
simple double-sided rotating plane.

## Other notes

- Keep your working Phase 7.11 folder; build this release separately.
- The exact visual effect depends on the physical prism orientation.
- UART0 remains COM5 / 115200 when Windows assigns the same port.
- The color scheme for STACK / BCD / HIDDEN / TRACE is unchanged.
- The C++ calculation engine uses BCD routines (the prism's animation alone
  uses floating-point trigonometry for graphical rotation).
- This release has not been compiled using the ESP32 PlatformIO toolchain or
  validated on the physical board here.
