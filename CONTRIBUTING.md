# Contributing to DeskBuddy

First off — thanks for considering a contribution. DeskBuddy is a small hobby project, but it's built to grow with input from anyone who wants to hack on ESP8266 firmware, embedded UI, or IoT integrations. This guide covers everything you need to get from "I want to help" to "my PR got merged."

No contribution is too small. Fixing a typo in the docs is just as welcome as a new feature.

---

## Getting Started

### 1. Fork the Repository

Click **Fork** in the top-right of the GitHub page, then clone your fork locally:

```bash
git clone https://github.com/<your-username>/deskbuddy.git
cd deskbuddy
```

Add the original repository as an upstream remote so you can keep your fork in sync:

```bash
git remote add upstream https://github.com/<original-owner>/deskbuddy.git
```

### 2. Set Up Your Environment

Follow the [Installation](README.md#installation) section of the README to get the Arduino IDE, ESP8266 core, and required libraries installed. You don't need physical hardware to contribute to documentation, but firmware changes should be tested on real hardware where possible — note in your PR if you were unable to test on a device.

---

## Branch Naming

Create a new branch for every change, off an up-to-date `main`:

```bash
git checkout main
git pull upstream main
git checkout -b <type>/<short-description>
```

Use one of these prefixes:

| Prefix | Use for |
|---|---|
| `feature/` | New functionality |
| `fix/` | Bug fixes |
| `docs/` | Documentation-only changes |
| `refactor/` | Code cleanup with no behavior change |
| `chore/` | Tooling, CI, repo maintenance |

Examples: `feature/telegram-notifications`, `fix/mqtt-reconnect-loop`, `docs/pinout-diagram`

---

## Commit Message Style

Keep commits small and focused. Write messages in the imperative mood, as if finishing the sentence "This commit will...":

```
Add exponential backoff to MQTT reconnect
Fix off-by-one in MOOD_COUNT
Update pinout table for buzzer relocation
```

Avoid vague messages like `fix stuff` or `update`. If a change needs more explanation, add a short body after a blank line:

```
Fix HTTP notification endpoint not registering

The /notify route was only added to the config-portal web
server instance, not the one started during normal boot.
```

---

## Pull Requests

1. Push your branch and open a PR against `main`.
2. Fill in the PR template: what changed, why, and how you tested it.
3. Keep PRs focused — one feature or fix per PR is much easier to review than a bundle of unrelated changes.
4. Link any related issue with `Closes #123` in the PR description.
5. Be responsive to review feedback. It's normal to go through a couple of rounds before merging.

For firmware changes, please include:
- What hardware you tested on (or a note if untested on hardware)
- Any new library dependencies
- Whether the change affects existing configuration (e.g. new `Config` struct fields)

---

## Bug Reports

Before filing a bug, search existing issues to avoid duplicates. A good bug report includes:

- **What happened** vs **what you expected**
- Steps to reproduce
- Firmware version / commit hash
- Hardware setup (board variant, display type, any deviations from the default pinout)
- Serial monitor output, if relevant

---

## Feature Requests

Open an issue describing:

- The problem you're trying to solve (not just the solution you have in mind)
- Any alternatives you've considered
- Whether you're interested in implementing it yourself

Check [`ROADMAP.md`](docs/ROADMAP.md) first — your idea might already be planned, and it's a good place to see how it might fit.

---

## Coding Style

DeskBuddy is intentionally kept as a single, readable `.ino` file. When contributing firmware changes:

- Match the existing section structure (the sketch is organized into clearly labeled blocks — includes, hardware definitions, config, screens, etc.). Add new code to the section it belongs in rather than appending to the end of the file.
- Prefer `char` buffers and `strlcpy` over `String` objects where possible — ESP8266 has limited RAM, and heap fragmentation from repeated `String` concatenation is a real problem on this platform.
- Use `F()` or `PROGMEM` for string literals that don't need to live in RAM.
- Keep functions focused — if a function is doing two unrelated things, consider splitting it.
- Avoid using boot-strapping pins (GPIO0, GPIO2, GPIO15) for new peripherals.
- Test both the "happy path" and reasonable failure cases (e.g. WiFi drops, malformed MQTT payloads) before submitting.

## Documentation Style

- Write like you're explaining something to a capable friend, not writing a spec sheet.
- Prefer short paragraphs and concrete examples over long abstract explanations.
- Keep beginner-friendliness in mind — not every contributor has prior embedded systems experience.
- Match the tone of existing docs: clear, direct, and free of unnecessary jargon.

---

## Code of Conduct

Be respectful, be patient with beginners, and assume good intent. This is a hobby project maintained in spare time — kindness goes a long way.

Thanks again for contributing to DeskBuddy!
