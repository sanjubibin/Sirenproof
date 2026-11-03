# Roadmap

Where Sirenproof stands and what comes next. The reasoning behind each step is in the
[plan](docs/PLAN.md); what has already changed is in the [changelog](CHANGELOG.md).

*Last updated: 3 November 2026 (M4 built and measured).*

## Milestones

| Milestone | What it delivers | Status |
|---|---|---|
| **M0** Foundation | Build, tests, CI, license and contributor docs | Done |
| **M1** Rule engine | Rules, schedules, Nudge/Block/Lock, loosen delays, unlock methods, trusted clock | Done |
| **M2** Service and enforcement | Windows service, secure pipe, signed rules, hosts blocking, closing programs | Built and tested; VM acceptance test pending |
| **M3** Browsers | Chrome/Edge extension, relay, block and pause pages, browser policies during locks | Built and tested; VM acceptance test pending |
| **M4** Interface | Main window, tray, pause overlay, "Lock down this PC" setup | Built; budgets met; usability test pending |
| **M5** v1.0 release | Signed installer, store listings, security review | Planned |

## Now: M4 interface

- [x] Style guide and clickable prototype approved
- [x] Main window: Now screen (mast ring, blocking now, waiting changes, breaks, unlock early)
- [x] Main window: Rules screen (targets, strength, rule tester, remove)
- [x] Main window: Start a lock (ready-made sets, unlock method, goal, preview, hold to lock)
- [x] Tray icon (status, open window, break, notifications)
- [x] Schedule painter: drag across the week to set when a rule applies
- [x] Command palette (Ctrl+K): type `lock youtube 2h`, `block reddit 25m` or `check twitch.tv`
- [x] Ready-made lists in the Rules screen, and a New rule card for scheduled blocks without the command
      line
- [x] The typing and friend-code unlock challenges, polished
- [x] "Lock down this PC" setup: a standard account for daily use ([plan §3.8](docs/PLAN.md#38-lock-down-this-pc))
- [x] Pause overlay for programs on a Nudge rule (Direct2D)
- [x] Events from the service, so the tray no longer asks every 15 seconds (the budget asks for no timers
      while nothing is active). The pause overlay needs them too.
- [x] Measure every budget in [plan §3.7](docs/PLAN.md#37-interface): all met, with
      `tools\measure-budgets.ps1` ([results](docs/TESTING.md#interface-budgets-plan-37))
- [ ] Usability test: two or three non-technical people set up a scheduled block without help

**Done when** two or three non-technical people can set up a scheduled block without the command line
or any help, and the budgets are met.

## Next: M5, the v1.0 release

- [x] Installer that refuses to uninstall during a lock, and starts the tray at sign-in
      (`installer\sirenproof.iss`, made by `tools\package.ps1`; VM test pending)
- [ ] Code signing
- [ ] Chrome Web Store and Edge Add-ons listings
- [ ] Testing against Microsoft Defender
- [ ] Threat-model review
- [ ] User docs and release checksums

**Done when** a signed installer is on GitHub Releases, and install → use → uninstall passes on a fresh
virtual machine.

## Waiting on hands-on testing

These need a Windows virtual machine; the steps are in [docs/TESTING.md](docs/TESTING.md).

- [ ] M2 acceptance test: a lock survives a hosts edit, killing the service, a clock change and a reboot
- [ ] M3 browser test: Shorts blocked while videos play, no private windows during a lock, a disabled
      extension leads to a warning and closed windows
- [ ] M4 "Lock down this PC" test: the setup walks through creating a second administrator and
      becoming a standard user, and refuses to go on without one

## After v1.0

**v1.x**

- Daily allowances ("30 minutes of YouTube a day")
- Allowlist work mode ("only these sites")
- Window-title rules (one Discord server, not all of Discord)
- Firefox
- Emergency passes
- Per-app network blocking, and blocking DNS-over-TLS/QUIC (Windows Filtering Platform)
- Local stats ("blocked 23 times today"), never leaving the PC
- Catch programs the moment they start (process-start events), not on the once-a-second check
- Separate rules per Windows user

**v2 and later**

- "Hard mode" with Windows App Control
- Unlocking with a hardware key
- Other operating systems
- Translations
