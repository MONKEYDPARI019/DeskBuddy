# Wiring Guide

This document describes every physical connection in DeskBuddy. Pair this with [`PINOUT.md`](PINOUT.md) for the underlying GPIO reference.

## OLED Display (I2C)

| OLED Pin | Connects To | Notes |
|---|---|---|
| VCC | 3V3 | SH1106 modules are typically 3.3V-tolerant; check your specific module's rated voltage before connecting to 5V. |
| GND | GND | Common ground with the rest of the circuit. |
| SCL | D1 (GPIO5) | I2C clock. |
| SDA | D2 (GPIO4) | I2C data. |

No external pull-up resistors are required — most SH1106 breakout boards include them onboard. If your display doesn't respond, check whether your specific board needs external 4.7kΩ pull-ups on SDA/SCL.

## Buttons

| Button | Pin A | Pin B | Notes |
|---|---|---|---|
| BTN1 | D5 (GPIO14) | GND | Wired with `INPUT_PULLUP` in firmware — no external resistor needed. Pressing the button pulls the pin LOW. |
| BTN2 | D7 (GPIO13) | GND | Same as above. |

## LEDs

Each LED is wired in series with a 470Ω resistor between its GPIO and ground.

| LED | GPIO | Wiring |
|---|---|---|
| Red (STATUS_LED) | D6 | GPIO → 470Ω resistor → LED anode → LED cathode → GND |
| Yellow (NOTIFY_LED) | D3 | GPIO → 470Ω resistor → LED anode → LED cathode → GND |
| Green (EMOTION_LED) | D4 | GPIO → 470Ω resistor → LED anode → LED cathode → GND |

Signal flow: the firmware drives each GPIO HIGH to light the corresponding LED and LOW to turn it off (standard sourcing configuration, resistor limits current to a safe level for both the GPIO and the LED).

## Buzzer

| Component | Connects To | Notes |
|---|---|---|
| Piezo buzzer (+) | D8 (GPIO15) | Must be a passive buzzer — see [`COMPONENTS.md`](COMPONENTS.md). |
| Piezo buzzer (–) | GND | |

Signal flow: `tone(BUZZER_PIN, freq, duration)` drives a square wave at the requested frequency directly from the GPIO; no driver transistor is required for a standard small piezo element.

## Power

| Rail | Source | Notes |
|---|---|---|
| 5V | USB (via D1 Mini's onboard regulator) | Powers the D1 Mini itself. |
| 3V3 | D1 Mini's onboard 3.3V regulator output | Powers the OLED and is the logic level for all GPIOs. |
| GND | Common | Shared across USB, D1 Mini, OLED, LEDs, buttons, and buzzer. |

## Decoupling Capacitors

Place both capacitors as close as physically possible to the D1 Mini's 3V3 and GND pins:

- **100µF electrolytic** — smooths current draw spikes when WiFi transmits and the buzzer/LEDs activate simultaneously.
- **0.1µF ceramic** — filters high-frequency noise, complementing the bulk electrolytic capacitor.

These were added after observing occasional resets during testing under combined WiFi/display/LED/buzzer load, and are recommended on any build of this circuit.

## Full Signal Flow Summary

```
USB 5V ──> D1 Mini regulator ──> 3V3 rail ──┬──> OLED VCC
                                              ├──> 100µF + 0.1µF (decoupling)
                                              └──> (logic reference for all GPIOs)

D1 (GPIO5) ──> OLED SCL
D2 (GPIO4) ──> OLED SDA

D5 (GPIO14) ──> BTN1 ──> GND
D7 (GPIO13) ──> BTN2 ──> GND

D6 (GPIO12) ──> 470Ω ──> Red LED ──> GND
D3 (GPIO0)  ──> 470Ω ──> Yellow LED ──> GND
D4 (GPIO2)  ──> 470Ω ──> Green LED ──> GND

D8 (GPIO15) ──> Piezo Buzzer (+) ──> GND
```
