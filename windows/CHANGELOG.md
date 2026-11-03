# Changelog

All notable changes to Sirenproof are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions will follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Nothing has been released yet. Until v1.0, everything is listed under **Unreleased**, grouped by
milestone, newest first. What comes next is in the [roadmap](ROADMAP.md).

## [Unreleased]

### Added

- **Release (M5, in progress)**
  - An installer (Inno Setup, `installer\sirenproof.iss`). It installs into Program Files only, starts
    the service, and starts the tray at sign-in for everyone on the PC. It refuses to uninstall, or to
    update, while a lock holds, and asks whether to keep your rules when you uninstall.
  - `sirenproof-service can-stop` says whether the service could be stopped right now (exit code 3 while
    a lock holds), and `uninstall` exits with 3 during a lock and 4 if there's nothing to remove.
  - `tools\package.ps1` builds a release: it stages the programs with Qt and the compiler's runtime,
    checks the staged main window starts with MSYS2 off the PATH, optionally signs them, compiles the
    installer, zips the extension for the stores, and writes `SHA256SUMS.txt`. CI does the same on
    every push and keeps the installer as an artifact.

- **Interface (M4, in progress)**
  - The "deep-water HUD" design, approved as a clickable prototype before any interface code.
  - `sirenproof-ui.exe`, the main window (Qt Quick). It runs only while it's open:
    - **Now:** the mast ring counts down the lock that holds. It also shows what's blocked right now,
      changes waiting to apply, breaks left today, and ending a lock early with its unlock method (wait,
      type or friend code; pasting is blocked).
    - **Rules:** every rule; add or remove sites, pages and apps; change the strength; a live rule
      tester; remove a rule.
    - **New rule:** a card on the Rules screen to set up a Nudge or Block without the command line: pick
      ready-made lists or type sites, pages and apps, then choose Always or paint the hours (Mon–Fri
      09:00–17:00 to start with). Painting there costs nothing until you press Create.
    - **Ready-made lists in the Rules screen:** each rule offers Video, Social, Games and News as buttons
      that add whatever the rule doesn't block yet, in one change; a list it already holds is ticked.
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
    - **Unlocking early, polished:**
      - *Type it out:* the text is shown in groups of five and lights up as you type it right, with the
        first wrong character marked and a progress bar. Only typing counts: besides Ctrl+V, anything that
        drops in more than a character at once (the context menu, dragging text in) is undone. Enter
        unlocks once it all matches, and the same text comes back if you close the window part-way.
      - *Friend code:* the field adds the dashes and maps look-alike letters as you type, says how many
        tries are left before a wait and how long that wait will be, and counts the wait down.
      - When the code is shown at the start of a lock, it's in large, spaced capitals, and closing it
        takes a tick that someone else has it, since it can't be shown again.
      - The wait method shows the lock's real wait instead of always "30 minutes".
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

### Security

From the M5 threat-model review ([docs/THREAT-MODEL.md](docs/THREAT-MODEL.md)):

- The service only reads program files on this PC's own drives, never through a share or a symbolic
  link. Before, `check \\server\share\x.exe` (or a program run from a share) made it read the file as
  SYSTEM, signing the PC in to that server and stalling enforcement.
- A program renamed `sirenproof….exe` is no longer exempt from app rules; only Sirenproof's own
  programs in its own folder are.
- Clients only trust the service's own process, as Windows' service manager names it, and console mode
  refuses the service's pipe where Sirenproof is installed, so a copy can't pose as the service.
- Setting the clock forward while the PC is off no longer ends a lock: time off counts only when that
  can't end anything early.
- Uninstalling starts a stopped service and asks it about locks, instead of going ahead.
- Messages that nest more than 32 levels are refused before parsing, so they can't exhaust the stack.
- The install-location check and the data folder use Windows' known folders, not environment variables.
- An edited state file never becomes the backup, and a client can't supply its own friend-code hash.

### Changed

- The pause for programs is drawn on the CPU: on screen in about 45 ms instead of 80–190 ms, since it no
  longer starts a GPU device first.
- The block page's countdown ring steps once a second instead of animating `stroke-dashoffset`, which
  repainted on every frame.

- The ready-made lists (Video, Social, Games) are defined once in the core, for the lock screen and the
  palette, and the command line and the palette read lengths ("1h30m") the same way.
- A fourth ready-made list, **News** (Google News, Hacker News, CNN, Fox News, the New York Times), for
  the lock screen, the Rules screen and the palette (`lock news 2h`).
- Messages from the service show times on your PC's clock instead of in UTC.
- A rule's schedule can be read and written on its own (`when_from_json`, `when_to_json`), and
  `WeeklySchedule::from_minutes` writes any set of minutes back as the fewest readable windows.
- The service's status now describes each lock in full: its rule, name, goal, start, unlock method and
  whether breaks are allowed. The main window uses this.
- Each lock in the status also says what its unlock method needs: the wait, the length of the text to
  type, or the friend-code tries left, the next wait and when guessing may resume (`friend_status`).
- Asking for the text to type again gives the same text until it's typed (or the lock is tightened to a
  longer one), and typed codes may run over several lines.

### Fixed

- The packaged main window couldn't start: it exited with "module QtQuick.Layouts is not installed",
  because MSYS2's Qt looks for its QML modules where MSYS2 keeps them rather than next to the program.
  `tools\package.ps1` now writes a `qt.conf` into the stage, and checks the staged window starts on its
  own, so this can't come back unnoticed. Found by the test tracker's check 3.2.
- The main window makes each screen the first time it's shown, instead of all of them at start, as the
  plan asks (§3.7): its start went from about 385 ms to 245 ms measured side by side, back under the
  400 ms budget after the New rule card and the unlock challenges had added to it.
- Option tiles in a row (Start a lock's lists, the unlock methods) are all as tall as the tallest, so a
  row no longer looks ragged when one description wraps.
- Screen readers can read the unlock challenges' progress and mistake lines ("4 of 20 characters",
  "Character 5 doesn't match", "That's not it. 4 tries left…"), and hear a mistake, a wrong code or
  the wait as it happens. Before, those lines were only on screen.
- A 1-minute unlock wait says "1 minute", not "1 minutes".
