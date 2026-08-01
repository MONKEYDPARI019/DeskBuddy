# DeskBuddy — Project Overview

## Introduction

DeskBuddy is a small, self-contained desk companion built on an ESP8266 microcontroller. It displays the time, local weather, and phone notifications on a 128×64 OLED screen, wrapped in the personality of an animated face called Mochi. It is a study and focus tool disguised as a desk toy.

This document covers the reasoning behind the project, how it evolved during development, the problems that came up along the way, and where it's headed next. If you're looking for wiring diagrams or step-by-step setup instructions, see the [README](../README.md) and [Quick Start Guide](../docs/QUICK_START_GUIDE.md) instead — this document is about the *why* and the *how it came to be*.

## Background

Small IoT displays aren't new. Smart clocks, weather stations, and notification hubs already exist as commercial products and open-source projects. What's usually missing is restraint. Most of them are built to *keep you engaged* — richer notifications, more screens, more reasons to look and interact. DeskBuddy was built with the opposite goal: show less, so you need your phone less.

The project also grew out of a very ordinary constraint — using parts that were already on hand. An ESP8266 board, a spare OLED, a couple of LEDs, and a piezo buzzer were enough to start, and that constraint shaped a lot of the early design decisions: single microcontroller, single display, no unnecessary peripherals.

## Why I Built This Project

As a student, I found myself constantly checking my phone while studying. Even checking the time often turned into scrolling through social media and losing focus. I wanted a small desk companion that could display only the essential information like time, weather and important notifications while allowing me to keep my phone away. DeskBuddy was created to improve concentration while still keeping me informed.

## Problem Statement

Phones are designed to be picked up. Every glance at the lock screen is an opportunity for a notification, an app icon, or a stray thought to pull attention away from whatever was actually being worked on. During study sessions or focused work, this constant low-grade interruption adds up — not because any single glance is long, but because each one resets your concentration.

The core problem DeskBuddy addresses: **how do you stay reachable for things that matter, without keeping a phone within arm's reach?**

## Objectives

1. Replace "checking the phone" for time and weather with a glance at a dedicated device
2. Forward only real notifications — not a social feed, not app badges, not infinite scroll
3. Keep the bill of materials small and affordable, ideally reusing parts people already have
4. Build the whole thing as a single, readable Arduino sketch that others can study and modify
5. Make the device configurable without needing to reflash firmware for every small change

## Development Journey

The project started as a basic OLED clock — just NTP time on a screen, nothing else. From there, features were layered in roughly in this order:

1. **Static screens first.** Clock, then weather, using hardcoded WiFi and API credentials just to get something on the display.
2. **The Mochi face.** What began as a placeholder "idle screen" became the most iterative part of the project — physics-based eyes, blinking, saccades, and eventually 15 distinct moods with matching buzzer tones.
3. **Persistence and configuration.** Hardcoded values were replaced with a `Config` struct backed by LittleFS, and a self-hosted web portal was added so settings could be changed from a phone browser instead of the Arduino IDE.
4. **Notifications.** This was the feature that justified the whole project. MQTT (via a HiveMQ Cloud broker) was chosen for its reliability over a flaky home network, with MacroDroid on the Android side forwarding real notifications as JSON payloads. An HTTP endpoint was added afterward as a simpler, broker-free alternative for basic use cases.
5. **Hardening.** A pass focused specifically on reliability: fixing broken control flow in the main loop, correcting an off-by-two mood counter, wiring up buzzer calls that were written but never triggered, and adding exponential backoff to MQTT reconnection so a dropped broker connection wouldn't spam retries.

## Challenges

**Boot-strapping pins.** Several ESP8266 GPIOs (GPIO0, GPIO2, GPIO15) affect the boot process and can't be freely used for buttons or outputs without causing unexpected resets. Early prototypes wired the buzzer to one of these pins, which caused the device to reset itself on certain button presses — a confusing bug that took a while to trace back to pin selection rather than software.

**TLS on a memory-constrained MCU.** Connecting to HiveMQ Cloud over TLS pushed PubSubClient's default 256-byte buffer past its limit, silently truncating or dropping messages. Diagnosing this took longer than expected because the MQTT connection itself appeared to succeed.

**Dead code that looked alive.** A full audit turned up buzzer melodies that were composed but never called, LED pins that were configured but never written to, and a config portal function that was defined but never invoked from `setup()`. None of these caused compile errors, which is exactly why they went unnoticed for a while — the firmware compiled cleanly and *looked* complete.

**Balancing features against a single file.** Keeping everything in one `.ino` was a deliberate choice for portability, but it meant being disciplined about section organization as the feature set grew from "clock" to "clock + weather + animated face + MQTT + HTTP + web config portal."

## Solutions

- Buzzer and status LEDs were moved off boot-strapping pins entirely.
- PubSubClient's buffer was explicitly enlarged (`setBufferSize`) to comfortably handle TLS packet sizes from HiveMQ.
- A structured audit pass went through every defined function and confirmed it was actually reachable from `setup()` or `loop()`, catching the unused config portal call and the silent buzzer functions.
- MQTT reconnection was rewritten with exponential backoff (capped delay, capped retry count) instead of a fixed retry interval, so failures degrade gracefully instead of hammering the broker.
- The firmware file was kept strictly organized into labeled sections (includes, hardware definitions, config, network, screens, etc.) so it stays navigable despite living in one file.

## Final Outcome

DeskBuddy in its current state runs as a single ESP8266 sketch that:

- Displays clock, weather, and an animated Mochi face across three cycling screens, plus a fourth notification history screen
- Connects to WiFi via a captive portal on first boot, and to a configurable MQTT broker over TLS afterward
- Forwards phone notifications from MacroDroid over both MQTT and a plain HTTP endpoint
- Stores all configuration in flash via a self-hosted web portal, requiring no reflashing for day-to-day changes
- Gives feedback through three status LEDs and a piezo buzzer, with a silent mode for focus sessions

It has been running reliably enough for daily study use, which was the actual bar for "done" — not perfection, but a tool that quietly does its one job.

## Lessons Learned

- **Compiling isn't the same as working.** Dead code, unused pins, and unreachable functions can sit in a sketch indefinitely without ever throwing an error. A deliberate audit pass — going function by function and confirming every one is actually called — caught more real bugs than any amount of casual testing.
- **Constrained hardware punishes assumptions.** Defaults that work fine on a desktop (like a 256-byte network buffer) can silently fail on a microcontroller, and the failure mode is often "quietly drops data" rather than "crashes loudly."
- **Configuration belongs in flash, not in code.** Moving from hardcoded credentials to a LittleFS-backed config struct with a web portal made the project dramatically easier to iterate on and much easier to hand to someone else to build.
- **Small constraints produce better design.** Committing to parts already on hand and a single-file firmware forced simpler, more deliberate decisions than an open-ended parts list would have.

## Future Scope

- A custom single-sided PCB (designed in KiCad, fabricated on standard single-sided copper-clad board) to replace the current breadboard/perfboard build
- OTA firmware updates, so future changes don't require a USB connection
- Battery monitoring, to eventually support a fully cordless build
- ESP32 support, for anyone who wants more headroom for future features
- A wider integration story beyond MacroDroid, including Telegram and Discord notification sources

See [`ROADMAP.md`](ROADMAP.md) for the versioned breakdown of what's planned next.
