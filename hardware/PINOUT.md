# Pinout

DeskBuddy runs on a Wemos D1 Mini (ESP8266). All pin references below use the D1 Mini silkscreen labels, with the underlying GPIO number noted since that's what matters for electrical behavior.

| Pin (D1 Mini) | GPIO | Signal | Direction | Default/Boot State | Notes |
|---|---|---|---|---|---|
| D1 | GPIO5 | I2C SCL | Output (I2C clock) | — | To SH1106 OLED. Bus runs at 400kHz (`Wire.setClock(400000)`). |
| D2 | GPIO4 | I2C SDA | I/O (I2C data) | — | To SH1106 OLED. |
| D5 | GPIO14 | BTN1 | Input, `INPUT_PULLUP` | HIGH (idle) | Active LOW. Mood cycle / notification scroll. Not a strapping pin — safe for a button. |
| D7 | GPIO13 | BTN2 | Input, `INPUT_PULLUP` | HIGH (idle) | Active LOW. Screen cycle (short press), silent-mode toggle (long press), config-portal trigger (held at boot). Not a strapping pin. |
| D6 | GPIO12 | STATUS_LED (Red) | Output | LOW | Solid ON = silent/DND mode active. |
| D3 | GPIO0 | NOTIFY_LED (Yellow) | Output | LOW | **Boot-strapping pin.** GPIO0 must read HIGH at power-on for normal boot (LOW forces flash mode). The LED here is only driven after `setup()` runs, but keep this in mind if you ever add external pull resistors or other circuitry on this line. |
| D4 | GPIO2 | EMOTION_LED (Green) | Output | LOW at boot (must be HIGH during boot) | **Boot-strapping pin.** GPIO2 must not be pulled LOW during power-on. The onboard LED on most D1 Mini boards is also tied to this pin. |
| D8 | GPIO15 | BUZZER_PIN | Output (`tone()`/`noTone()`) | Must be LOW at boot | **Boot-strapping pin.** GPIO15 must read LOW at power-on (HIGH forces an invalid boot mode). A piezo buzzer wired directly to this pin is fine electrically (no significant pull-up), but avoid adding your own pull-up resistor here. |

## Why This Matters

Three of the eight GPIOs on the ESP8266 (0, 2, and 15) have special meaning during the boot sequence — their logic level at power-on/reset determines which mode the chip boots into. DeskBuddy uses all three for LEDs and the buzzer (D3, D4, D8). In practice this hasn't caused boot issues because these are used purely as outputs with no added pull resistors, but it's worth knowing if you're modifying the hardware — adding a pull-up/pull-down on any of these three pins, or driving them from an external source before `setup()` runs, can prevent the board from booting at all.

## I2C Bus

The OLED is the only device on the I2C bus. `Wire.begin(SDA_PIN, SCL_PIN)` is called once in `setup()` with the bus clocked at 400kHz for smoother animation redraws.

## Not Used

No analog pin (A0) is currently used. It's read once at boot purely to seed the random number generator (`randomSeed(analogRead(0))`), which drives Mochi's idle eye movement — it isn't used as a real sensor input.
