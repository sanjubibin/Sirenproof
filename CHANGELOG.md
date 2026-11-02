# Changelog

All notable changes to Sirenproof are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions will follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Nothing has been released yet. Until v1.0, everything is listed under **Unreleased**, grouped by
milestone, newest first. What comes next is in the [roadmap](ROADMAP.md).

## [Unreleased]

### Added

- **Interface (M4, in progress)**
  - The "deep-water HUD" design, approved as a clickable prototype before any interface code.
  - `sirenproof-ui.exe`, the main window (Qt Quick). It runs only while it's open:
    - **Now:** the mast ring counts down the lock that holds. It also shows what's blocked right now,
      changes waiting to apply, breaks left today, and ending a lock early with its unlock method (wait,
      type or friend code; pasting is blocked).
    - **Rules:** every rule; add or remove sites, pages and apps; change the strength; a live rule
      tester; remove a rule.
    - **Schedule painter:** the week as 7 × 24 hours. Drag to paint hours on, or start on a lit hour to
      clear them; arrow keys and Space work too. Hours you don't touch keep their exact minutes (09:30
      stays 09:30), and the current hour is marked. Adding hours applies at once; clearing them waits
      for the rule's delay.
    - **Start a lock:** ready-made sets, length, unlock method, goal and a preview, then hold for two
      seconds to commit.
    - **Command palette** (Ctrl+K, or the bar at the top): type `lock youtube 2h`, `block reddit 25m`,
      `nudge twitch 1h`, `break`, `check reddit.com` or `rules`. It uses the command line's words, with
      a few shortcuts (a bare length, list names like `video`, and names like `youtube`), shows what a
      command will do before you press Enter, and checks addresses as you type. A lock fills in the
      Start a lock screen, so it's still held to commit.
    - **Lock down this PC:** a four-step setup that moves everyday use to a standard account, so that
      switching Sirenproof off needs a password someone else holds. It checks the PC's administrators
      live and opens Windows' own settings for each change; Sirenproof never changes an account itself.
      It won't go on until a second administrator exists, and warns if that one disappears. The Now
      screen offers it while a lock holds and your account is an administrator.
    - `--palette <text>` opens the palette with that text, and `--run <command>` does a command at start.
    - Reconnects to the service by itself, and honours Windows' "show animations" setting.
  - `sirenproof-tray.exe`, the tray icon (plain Win32, about 2 MB of private memory):
    - Its tooltip says whether a lock holds and until when.
    - A click opens the main window.
    - The right-click menu offers a 5-minute break.
    - It notifies you when a lock starts or ends.
    - `--self-test` checks it without leaving an icon behind.
    - It no longer asks the service every 15 seconds: it waits for the service to say something changed
      (`events.wait`), so it does nothing at all while nothing happens.
  - **The pause for programs.** A program caught by a Nudge rule is neither closed nor frozen: the tray
    covers the screen with a pause showing your goal and a countdown, then offers "Continue for 5
    minutes" or "Close it". Drawn with Direct2D; `sirenproof-tray --pause-preview <file.png>` draws it
    into a file. `sirenproof status` lists programs waiting at the pause.
  - CI builds the main window with Qt and loads it offscreen.
  - `tools\measure-budgets.ps1` measures every interface budget in the plan (§3.7) and prints a pass/fail
    table; all are met. The window (`--measure`) and the tray (`--measure-idle`, `--measure-pause`) have
    the hooks it uses.

- **Browsers (M3)**
  - The Chrome and Edge extension (Manifest V3). It asks the service about every page, including
    YouTube-style in-page navigation, and shows a block or pause page.
  - `sirenproof-host.exe`, the relay between the extension and the service. It checks which extension
    is calling, caps message size and passes on only the extension's own requests.
  - During a lock:
    - private windows and DNS-over-HTTPS are turned off by policy in Chrome, Edge and Brave, and the
      previous values are restored afterwards;
    - browsers without the extension are closed after a 60-second warning.
  - The plan for the "Lock down this PC" setup (a standard account for daily use).

- **Windows service and enforcement (M2)**
  - `sirenproof-service.exe`, a Windows service that restarts if it's killed.
  - A local named pipe as the only way in. Clients only talk to the service program installed next to
    them, and the service takes the extension's requests only from the relay.
  - The rules file is signed with a key protected by Windows (DPAPI), so edits by hand are detected.
  - Blocking in the hosts file, with tamper repair within a second.
  - Closing blocked programs, with a warning first.
  - The `sirenproof` command line talks to the service: `status`, `rules`, `block`, `remove`, `cancel`,
    `break`, `continue`, `unlock` and `check`.
  - Console mode for safe development. It uses a stand-in hosts file and a dry run by default, and debug
    builds cap locks at 2 minutes.
  - `tools/dev-reset.ps1` undoes everything Sirenproof changed.
  - Installing refuses to run from outside Program Files.

- **Rule engine (M1)**
  - Rules for whole sites, parts of sites, single pages, programs, folders, publishers and Store apps.
  - Weekly schedules and one-off sessions.
  - Three strengths: Nudge (a pause), Block and Lock (can't be loosened until it ends).
  - Tightening applies at once; loosening waits for a delay.
  - Unlock methods: none, wait, type a passage, or a friend's code.
  - Breaks with a daily limit.
  - A trusted clock that resists changes to the Windows clock and time zone.
  - A strict rules-file format, with `sirenproof check`, `plan` and `validate` to try rules without
    changing anything.
  - An example rules file.

- **Foundation (M0)**
  - Research into why people struggle with distractions on Windows, and the product plan.
  - C++23 build with CMake presets on MSYS2 UCRT64, strict warnings and hardening flags.
  - Unit tests, and CI on GitHub Actions.
  - GPL-3.0-or-later license, README, contributing guide and security policy.

### Changed

- The pause for programs is drawn on the CPU: on screen in about 45 ms instead of 80–190 ms, since it no
  longer starts a GPU device first.
- The block page's countdown ring steps once a second instead of animating `stroke-dashoffset`, which
  repainted on every frame.

- The ready-made lists (Video, Social, Games) are defined once in the core, for the lock screen and the
  palette, and the command line and the palette read lengths ("1h30m") the same way.
- Messages from the service show times on your PC's clock instead of in UTC.
- A rule's schedule can be read and written on its own (`when_from_json`, `when_to_json`), and
  `WeeklySchedule::from_minutes` writes any set of minutes back as the fewest readable windows.
- The service's status now describes each lock in full: its rule, name, goal, start, unlock method and
  whether breaks are allowed. The main window uses this.
