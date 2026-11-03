# Sirenproof for Windows

> Tie yourself to the mast.

[![CI](https://github.com/sanjubibin/Sirenproof/actions/workflows/ci.yml/badge.svg)](https://github.com/sanjubibin/Sirenproof/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](../LICENSE)

This folder is the Windows app. The other platforms (macOS, Android, iOS) have folders of their own
next to it; see the [project README](../README.md). Every path and command below is from this
`windows` folder.

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

**Getting ready for v1.0.** There's no release yet. Every build on `main` makes an unsigned installer
(the **sirenproof-installer** artifact of a [CI run](https://github.com/sanjubibin/Sirenproof/actions/workflows/ci.yml)),
for testing in a virtual machine. How to use it is in the [user guide](docs/USER-GUIDE.md).

- **M0 (foundation):** done. Build, tests and CI are in place.
- **M1 (rule engine):** done. It covers rules for sites, pages and apps; schedules; Nudge, Block and Lock;
  loosen delays; unlock methods; breaks; and a clock that resists tampering.
- **M2 (Windows service and enforcement):** built and tested on a development PC. It covers the
  service, the secure pipe, the signed state, hosts-file blocking with tamper repair, and closing
  blocked programs; the `sirenproof` command line drives it. The acceptance test in a virtual machine
  ([docs/TESTING.md](docs/TESTING.md)) is still to run.
- **M3 (browser extension):** built and tested on a development PC.
  - The Chrome/Edge extension asks the service about every page, including YouTube-style in-page
    navigation, and shows the block or pause page.
  - A relay connects the extension to the service.
  - During locks, private windows and DNS-over-HTTPS are turned off by policy, and browsers without
    the extension are closed after a warning.
  - The browser part of the virtual-machine test is still to run.
- **M4 (interface):** built, and every speed and memory budget is measured and met. A usability test is
  still to run. It has:
  - the main window's Now, Rules and Start a lock screens, with hold-to-lock, the typing and
    friend-code ways to end a lock early, ready-made lists, a New rule card for scheduled blocks, a
    live rule tester, a schedule painter and a command palette (Ctrl+K);
  - the "Lock down this PC" setup, which guides you to a standard account for everyday use;
  - the tray icon, which shows whether a lock holds, opens the window and offers a break;
  - the pause for programs on a Nudge rule, drawn over the screen by the tray.
- **M5 (v1.0 release):** in progress. The installer (it refuses to uninstall during a lock), the
  packaging and release workflow with checksums, the user guide, the privacy policy and the store
  listing text are done. Still to come: code signing, the store submissions, and the tests on a
  virtual machine, tracked in [docs/TEST-TRACKER.md](docs/TEST-TRACKER.md).

See the [roadmap](ROADMAP.md) for what's next and the [changelog](CHANGELOG.md) for what has changed.
The reasoning behind both is in the [plan](docs/PLAN.md#8-milestones).

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

Sirenproof is C++23, built with the [MSYS2](https://www.msys2.org/) UCRT64 toolchain, CMake and Ninja.

1. Install MSYS2 (the presets expect `C:\msys64`).
2. From the **MSYS2 UCRT64** shell, install the tools and libraries:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-catch mingw-w64-ucrt-x86_64-nlohmann-json
   ```

   The main window also needs Qt 6. Without it, everything else still builds:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-declarative
   ```

3. Build and test. These commands work from PowerShell or the UCRT64 shell, in the repository's
   `windows` folder (`cd windows`):

   ```bash
   C:/msys64/ucrt64/bin/cmake.exe --preset debug
   C:/msys64/ucrt64/bin/cmake.exe --build --preset debug
   C:/msys64/ucrt64/bin/ctest.exe --preset debug
   ```

Use `release` instead of `debug` for an optimised build. If MSYS2 is installed somewhere else, copy
`CMakePresets.json` to `CMakeUserPresets.json` (git-ignored) and change the paths.

## Trying it

**Without the service.** The command line can evaluate a rules file and report what would happen,
without changing anything on your PC. [examples/state.example.json](examples/state.example.json) has
three rules:

- Social sites and the YouTube feed are blocked during work hours.
- Games get a nudge on school nights.
- A locked exam-revision session that still allows YouTube videos and search.

```bash
build/debug/bin/sirenproof.exe plan --state examples/state.example.json --at 2026-11-02T10:00:00Z
build/debug/bin/sirenproof.exe check https://www.youtube.com/shorts/abc --state examples/state.example.json
```

Times given with `--at` are UTC. Output is shown in your local time.

**With the service, safely.** `sirenproof-service.exe console` runs the service in a terminal window. It
writes to a stand-in hosts file, and in debug builds it only logs what it would do. Then use
`sirenproof status`, `block`, `check`, `remove`, `break` and `unlock` from another terminal.
[docs/TESTING.md](docs/TESTING.md) has the details, and the virtual-machine test for the real
thing.

### Rule targets

| Target | Blocks |
|---|---|
| `reddit.com` | the whole site, including subdomains |
| `youtube.com/shorts` | part of a site: that path and everything below it |
| `youtube.com/` | only the home page |
| `youtube.com/watch?v=abc` | pages with that query parameter |
| `app:discord.exe` | a program by name, even if the exe is renamed |
| `app:C:\Games\Steam\` | every program in a folder |
| `publisher:Valve Corp.` | every program signed by a publisher |
| `package:<family name>` | a Microsoft Store app |

Windows itself, Windows Update and sign-in, security software, assistive technology and Sirenproof itself
can never be targeted.

## Layout of this folder

```
core/      Pure C++ logic (rules, matching, schedules, locks, trusted clock, state file, hosts editing).
           No Windows headers; fully unit-tested.
platform/  The Windows layer: files, signed state store, hosts file, pipe, processes, clock
service/   sirenproof-service.exe: the background service
host/      sirenproof-host.exe: the relay between the browser extension and the service
extension/ The Chrome / Edge extension (Manifest V3, no build step) and its tests
cli/       sirenproof.exe: the command line
ui/        sirenproof-ui.exe: the main window (Qt Quick); runs only while it's open
tray/      sirenproof-tray.exe: the tray icon (plain Win32)
tests/     Unit tests (core) and integration tests (platform)
installer/ The Inno Setup script for the installer
tools/     For developers: package.ps1 builds a release (installer, extension zip, checksums);
           dev-reset.ps1 undoes everything Sirenproof changed; measure-budgets.ps1 checks the
           interface budgets
examples/  An example rules file
cmake/     Warning and hardening flags
docs/      User guide, privacy policy, research, plan, testing guide and test tracker, releasing
```

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md). To report a security problem, see [SECURITY.md](../SECURITY.md).
Please don't open a public issue for security problems.

## License

Sirenproof is free software under the [GNU General Public License v3.0 or later](../LICENSE).
