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

### If something goes wrong

`tools\dev-reset.ps1` (administrator PowerShell) removes the service, Sirenproof's hosts entries and,
with `-RemoveData`, its data, even while a lock holds. Add `-WhatIf` first to see what it would do.
