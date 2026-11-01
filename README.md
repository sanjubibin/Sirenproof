# Sirenproof

> Tie yourself to the mast.

[![CI](https://github.com/sanjubibin/Sirenproof/actions/workflows/ci.yml/badge.svg)](https://github.com/sanjubibin/Sirenproof/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

Sirenproof is a free, open-source blocker for Windows. It blocks the websites (whole sites or single
pages) and desktop apps that pull you off track, using rules you set while you're calm. It holds you to
those rules when the pull is strongest.

It runs entirely on your PC. There's no account, no cloud and no tracking, and your rules and browsing
never leave the machine.

In the Odyssey, Ulysses knew he couldn't resist the Sirens' song, so he had himself tied to the mast
before the ship came near. Sirenproof works the same way. You decide what's off-limits ahead of time, and
it keeps to that decision for you. When you hit a block, it reminds you of the goal you set rather than
lecturing you.

## Status

**Early development. Nothing to install yet.** The foundation (milestone M0) is in place: build, tests
and CI. The rule engine (M1) is next. See the [plan](docs/PLAN.md#8-milestones) for the roadmap.

## What it will do

- **Block websites, single pages and apps under one set of rules.** For example: YouTube tutorials yes,
  feed and Shorts no; Steam and Discord blocked during work hours.
- **Three strengths:**
  - **Nudge:** a pause that shows your goal, then lets you continue briefly.
  - **Block:** blocked, and loosening a rule waits out a delay.
  - **Lock:** holds until it ends.
- **Tighten instantly, loosen slowly.** Adding a block applies at once. Removing one waits out a delay.
- **Breaks and exceptions expire on their own.**
- **Safety rails.** It never blocks Windows itself, assistive technology or system updates. It warns
  before closing an app, and uninstalls cleanly when no lock is active.
- **Private and transparent.** Local only, open source, no telemetry.

The reasons behind each of these are in [docs/RESEARCH.md](docs/RESEARCH.md): what distracts people,
how it feels, and why existing blockers fail them.

## Building from source

Sirenproof is C++20, built with the [MSYS2](https://www.msys2.org/) UCRT64 toolchain, CMake and Ninja.

1. Install MSYS2 (the presets expect `C:\msys64`).
2. From the **MSYS2 UCRT64** shell, install the tools:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-catch
   ```

3. Build and test. These commands work from PowerShell or the UCRT64 shell, in the repository folder:

   ```bash
   C:/msys64/ucrt64/bin/cmake.exe --preset debug
   C:/msys64/ucrt64/bin/cmake.exe --build --preset debug
   C:/msys64/ucrt64/bin/ctest.exe --preset debug
   ```

Use `release` instead of `debug` for an optimised build. If MSYS2 is installed somewhere else, copy
`CMakePresets.json` to `CMakeUserPresets.json` (git-ignored) and change the paths.

## Repository layout

```
core/      Pure C++ logic (rules, matching, schedules, locks). No Windows headers; fully unit-tested.
tests/     Catch2 unit tests
cmake/     Warning and hardening flags
docs/      Research and plan
```

The service, command line, tray app, browser extension and installer arrive in later milestones (see
[the plan](docs/PLAN.md#7-repository-layout)).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). To report a security problem, see [SECURITY.md](SECURITY.md).
Please don't open a public issue for security problems.

## License

Sirenproof is free software under the [GNU General Public License v3.0 or later](LICENSE).
