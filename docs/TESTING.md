# Testing Sirenproof

Three levels of testing, from safest to most real.

| Level | What it covers | Where it runs |
|---|---|---|
| **Unit tests** (`core_tests`) | Rules, matching, schedules, locks, the trusted clock, the state file, hosts-file editing | Anywhere, including CI |
| **Platform tests** (`platform_tests`) | Atomic files, folder trust checks, the signed state store, the pipe, closing programs | Anywhere. They use temporary files, private pipe names and a `sleeper.exe` built for the purpose, never the real hosts file, services or other programs |
| **Acceptance test** | The installed service on a real Windows, including reboots and tampering | **A virtual machine only** |

Run the first two with:

```bash
C:/msys64/ucrt64/bin/ctest.exe --preset debug
```

## Trying the service safely on your own PC

Console mode runs the service in a terminal window, with safe defaults:

- Data goes into `dev-data\` next to the exe.
- Blocks go into a **stand-in** hosts file there, never the real one.
- In debug builds nothing is enforced at all (`--dry-run`) unless you pass `--enforce`.

```bash
build/debug/bin/sirenproof-service.exe console
```

From a second terminal:

```bash
build/debug/bin/sirenproof.exe block reddit.com --for 25m --goal "Finish the report"
build/debug/bin/sirenproof.exe status
build/debug/bin/sirenproof.exe check https://www.reddit.com
```

Debug builds also cap lock sessions at 2 minutes. If you lock yourself out anyway, create an empty
file called `dev-unlock` in the data folder: debug builds then drop every rule.

> Run these commands from PowerShell or `cmd`, not Git Bash. Git Bash rewrites arguments such as pipe
> names (`\\.\pipe\...`) before the program sees them.

## Trying the main window and the tray icon

With the console service running, start either from PowerShell. The main window uses Qt's libraries
from MSYS2, so put them on the path first (the installer will ship its own copies):

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
build\debug\bin\sirenproof-ui.exe
build\debug\bin\sirenproof-tray.exe
```

Both talk to the service like the command line does, and only trust a service that runs from their own
folder. Clicking the tray icon opens the window, and right-clicking it offers a break. Closing the window
or hiding the icon never stops any blocking.

To check the tray without leaving an icon behind, `sirenproof-tray.exe --self-test` adds the icon, reads
the service's status once, prints what the tooltip says and removes the icon again. For a quick look at
a screen without a window, `sirenproof-ui.exe --screen rules --screenshot rules.png` renders it to a file.
Add `--palette "lock youtube 2h"` to see the command palette, or `--run "<command>"` to see what a command
does.

### The pause for programs

With the console service and the tray running, put a Nudge rule on a harmless program and open it:

```powershell
build\debug\bin\sirenproof.exe block app:notepad.exe --nudge --goal "Finish the report"
notepad
```

Within a second the pause covers the screen: your goal, a 15-second countdown, then **Continue for 5
minutes** or **Close Notepad**. In a dry run, closing only logs what it would do. `sirenproof status`
lists the program as paused while it waits. To see the pause without a service,
`sirenproof-tray.exe --pause-preview pause.png --elapsed 5` draws it into a file.

## Interface budgets (plan §3.7)

`tools\measure-budgets.ps1` measures every budget against a throwaway console service (dry run, its own
data folder, a stand-in hosts file and a private pipe) and prints a pass/fail table. The window and the
pause are measured invisible and click-through, so nothing gets in your way; the tray icon shows for the
idle minute. Close Sirenproof's own tray first, and build the `release` preset:

```powershell
powershell -ExecutionPolicy Bypass -File tools\measure-budgets.ps1
```

Results on 3 November 2026 (release build; Intel Core Ultra 9 285H with Arc 140T graphics, 2880 × 1800,
Windows 11). A fast laptop: slower PCs are worth measuring too, for example in the test VM.

| Surface | Budget | Measured |
|---|---|---|
| Main window | Start ≤ 400 ms | 236 ms (median of 4 warm starts). The first start after a fresh build took 1.46 s, with Windows Defender scanning the new program; a cold start after a reboot isn't measured yet |
| Main window | Draws only when something changes | 0.7 frames a second while idle (Direct3D 11) |
| Main window | 60 fps while animating | 60 fps; longest frame 19.8 ms |
| Tray | ≤ 3 MB | 2.08 MB private (10.4 MB working set, mostly shared Windows code) |
| Tray | 0% CPU and no timers when idle | 0 ms of CPU in 60 s; no timers |
| Pause | On screen ≤ 100 ms after the program is caught | 45 ms. Catching it takes up to a second after it starts, as programs are checked once a second |
| Block page | ≤ 50 KB, no downloaded fonts, animates only transform and opacity | 7.5 KB; none; transform and opacity |

Measuring found two things, both fixed:

- **The pause took 77–191 ms**, most of it starting a GPU device. It's now drawn on the CPU: about 40 ms
  to the first frame and a few milliseconds a frame after that.
- **The block page's countdown ring animated `stroke-dashoffset`**, which repaints every frame. It now
  steps once a second.

The pieces can also be run on their own: `sirenproof-ui.exe --measure <file.json>`,
`sirenproof-tray.exe --measure-idle <file.json> [--seconds <n>]` and
`sirenproof-tray.exe --measure-pause <file.json>`.

## Trying the browser extension safely

The extension only blocks pages. In console mode the service still uses its stand-in hosts file and
stand-in browser policies, and it never closes a browser unless you pass `--guard-browsers`.

1. Tell Chrome, Edge and Brave where the relay is, for your Windows account only:

   ```bash
   build/debug/bin/sirenproof-host.exe register --user
   ```

   `unregister --user` undoes it.

2. Load the extension. Open `chrome://extensions` (or `edge://extensions`), turn on **Developer mode**,
   choose **Load unpacked** and pick the `extension` folder. Its id should be
   `cgpgilkcgaojdaclgkllhndfgdphgdje`, which the relay checks.
3. Start the service in console mode (`build/debug/bin/sirenproof-service.exe console`). It uses the
   normal pipe name, which is the one the relay talks to.
4. From another terminal:

   ```bash
   build/debug/bin/sirenproof.exe block youtube.com/shorts youtube.com/feed --for 25m --goal "Finish the report"
   build/debug/bin/sirenproof.exe block reddit.com --nudge --for 25m
   ```

5. Check that:
   - `youtube.com/shorts/...` and the feed show the block page;
   - a video (`/watch?v=...`) plays;
   - moving from a video to Shorts inside YouTube is caught too;
   - Reddit shows the pause page, and **Continue** becomes available after 15 seconds.

The extension's own tests run under Node: `node --test extension/tests/background.test.mjs`.

## Acceptance test in a virtual machine (milestone M2)

The plan's goal for M2: a 25-minute lock on `youtube.com` and a game survives a hosts edit, killing
the service, a clock change and a reboot, and uninstalling when unlocked leaves nothing behind.

### Set up

1. Create a Windows 10 or 11 VM in [VirtualBox](https://www.virtualbox.org/). Windows Home has no
   Hyper-V or Windows Sandbox. Take a snapshot called **clean**.
2. On your PC, build a **release** build, since debug builds cap locks at 2 minutes:

   ```bash
   C:/msys64/ucrt64/bin/cmake.exe --preset release
   C:/msys64/ucrt64/bin/cmake.exe --build --preset release
   ```

3. Copy `build/release/bin/sirenproof.exe` and `sirenproof-service.exe` into the VM, at
   `C:\Program Files\Sirenproof\`.
4. Choose a "game" to block. Any program will do, for example a copy of `sleeper.exe` from
   `build/release/bin/`, saved as `C:\Games\game.exe`.

### Run it (administrator prompt in the VM)

```bat
cd "C:\Program Files\Sirenproof"
sirenproof-service install
sirenproof block youtube.com app:C:\Games\game.exe --for 25m --lock --unlock wait:10 --id exam --goal "Revise chapter 3"
sirenproof status
```

Then try each bypass. Write down what happens:

| # | Try this | Expected |
|---|---|---|
| 1 | Open `https://youtube.com` in Edge | Doesn't load (the hosts file blocks it) |
| 2 | Start `C:\Games\game.exe` | Closed within about a second |
| 3 | Open `C:\Windows\System32\drivers\etc\hosts` in Notepad as administrator, delete the Sirenproof section, save | The section is back within a second |
| 4 | `sc stop Sirenproof` | Refused: the service doesn't accept stop while locked |
| 5 | `taskkill /f /im sirenproof-service.exe` | Windows restarts it within a few seconds; `sirenproof status` still shows the lock |
| 6 | Settings → Time: turn off automatic time and move the clock 3 hours ahead | `sirenproof status` still shows the lock, with a note that the Windows clock is off |
| 7 | Change the time zone | Same as 6 |
| 8 | Restart the VM | After sign-in the lock still holds, with the same end time (minus the time the VM was off) |
| 9 | `sirenproof-service uninstall` | Refused: a lock is active |
| 10 | `sirenproof unlock exam`, then wait 10 minutes | Unlocked; YouTube loads again |
| 11 | `sirenproof-service uninstall --remove-data` | Done. `sc query Sirenproof` says the service doesn't exist; the hosts file has no Sirenproof section; `C:\ProgramData\Sirenproof` is gone |

Finally, restore the **clean** snapshot.

### Browser part (milestone M3)

`install` also registers the relay with Chrome, Edge and Brave. Until the extension is in the stores
(M5), load it unpacked in each browser you test, as described above. Start from the **clean** snapshot:

```bat
sirenproof-service install
sirenproof block youtube.com/shorts youtube.com/feed youtube.com/ --for 25m --lock --id yt --goal "Revise chapter 3"
```

| # | Try this | Expected |
|---|---|---|
| 12 | Open YouTube's home page, the feed and a Short | The block page, with your goal and the end time |
| 13 | Open a video (`/watch?v=...`), then click into Shorts from it | The video plays; the Short is blocked |
| 14 | Open an Incognito (Chrome) or InPrivate (Edge) window | Not available; the browser says it's managed. Back to normal when the lock ends |
| 15 | Turn the extension off in `chrome://extensions` | `sirenproof status` lists the browser as unprotected; about 60 seconds later its windows close |
| 16 | Open Firefox, if installed | Closed after the same grace period (the extension isn't available for it yet) |
| 17 | After the lock: `sirenproof-service uninstall` | The browser policies are back as they were, and the relay is unregistered |

### Lock down this PC (milestone M4)

This changes Windows accounts, so it belongs in the VM too. Start from the **clean** snapshot, signed in
as the VM's only administrator, with the service installed and `sirenproof-ui.exe` copied next to it
(with Qt's libraries) and running.

| # | Try this | Expected |
|---|---|---|
| 18 | Open **Lock down this PC** | Step 1 names your account as an administrator, with no other administrator |
| 19 | Step 2, before creating anything | "I've created it" stays disabled |
| 20 | Step 2: **Open account settings**, add a local account "Keeper" and make it an administrator | Within a few seconds the step shows "Found: Keeper" and lets you continue |
| 21 | Go on to step 4, then in Settings make Keeper a standard user again | Step 4 warns that there's no other administrator and offers no button. Make Keeper an administrator again: the button comes back |
| 22 | Step 4: **Open User Accounts**, change your account type to Standard (Keeper's password is asked for) | The step shows that your account is now a standard user |
| 23 | Sign out and back in. Try `sc stop Sirenproof`, editing the hosts file, and uninstalling | Each needs Keeper's password |
| 24 | Undo: as Keeper, make your account an administrator again | Step 1 shows it as an administrator again |

### If something goes wrong

`tools\dev-reset.ps1` (administrator PowerShell) removes the service, Sirenproof's hosts entries and,
with `-RemoveData`, its data, even while a lock holds. Add `-WhatIf` first to see what it would do.
