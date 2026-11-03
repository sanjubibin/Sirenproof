# Threat model

The M5 review of what Sirenproof has to protect, from whom, and whether the code does what
[the plan, §5](PLAN.md#5-security) promises. Reviewed on 3 November 2026 against `main`, by reading the
code (nothing was run). Findings that need a fix or a test are tracked in
[TEST-TRACKER.md, section 7](TEST-TRACKER.md#7-security-review-follow-ups).

## What's worth protecting

| Asset | Why it matters |
|---|---|
| **The PC itself** | The service runs as LocalSystem. A flaw in it, its pipe or its files could hand an ordinary user (or malware running as them) full control of Windows |
| **The user's privacy** | Rules, goals and which pages they open. Sirenproof promises none of it leaves the PC |
| **The user's own commitment** | A lock they chose must hold against their impulsive self. Losing that is the product failing, not a security breach, but it's why the tool exists |
| **Other people's rules** | On a shared PC, one account mustn't change another's locks |

## Who might attack

| Actor | Can | Goal |
|---|---|---|
| **You, on an impulse** (the main one) | Everything an ordinary or administrator account can do in a few minutes | End a lock early |
| **You, determined, as an administrator** | Safe Mode, taking ownership, reinstalling Windows | End a lock. Out of scope by design (SECURITY.md); "Lock down this PC" moves the everyday account to a standard one |
| **Another local user, or malware running as one** | Connect to the pipe, write to their own folders and `%TEMP%`, create folders under ProgramData, run programs | Get SYSTEM, or change someone else's rules |
| **A web page** | Run script in the browser | Reach the extension's relay or the service |
| **A tampered download** | Replace the installer on the way | Install something else as SYSTEM |

## Trust boundaries

```
 web pages ──► browser ──► extension ──stdin/stdout──► sirenproof-host.exe ─┐
                                                                             │ named pipe
 sirenproof.exe / sirenproof-ui.exe / sirenproof-tray.exe (user) ────────────┤ \\.\pipe\Sirenproof.v1
                                                                             ▼
                                                     sirenproof-service.exe (LocalSystem)
                                                        │ hosts file, browser policies,
                                                        │ closing programs, ProgramData\Sirenproof
```

1. **Pipe:** everything a user-level process sends the service is untrusted input.
2. **Relay:** the extension is reached only through Chrome's native messaging, and the relay only
   through the pipe; the service tells the relay apart by its process image path.
3. **Files:** the service's data folder, the hosts file, and the folder it runs from.
4. **Installer:** runs elevated, once, from a download.

## Promises checked against the code

| Promise (PLAN §4–5, SECURITY.md) | Result | Where |
|---|---|---|
| Pipe access list: SYSTEM and Administrators full, signed-in users read/write only, no new instances | Holds | `platform/include/sirenproof/platform/pipe.hpp` (`kPipeSddl`) |
| Remote clients rejected; first-instance flag | Holds | `platform/src/pipe.cpp` (`create_instance`) |
| Clients connect identification-only; the service never acts as them | Holds | `pipe.cpp` (`SECURITY_IDENTIFICATION`, `client_sid` reverts or aborts) |
| 64 KiB message cap | Holds | `pipe.cpp`, `host/src/main.cpp` |
| Clients only trust the real service | **Fixed** (was: same exe path was enough; see S3) | `pipe.cpp` (`pipe_call`, `service_process_id`) |
| Only the relay may use `ext.*`, and only those | Holds, by the relay's path (not a signature: there is none yet) | `service/src/protocol.cpp` |
| Strict input | Holds for rules and settings; messages also get a nesting limit (S6) | `core/src/state_json.cpp`, `protocol.cpp` |
| The hosts file only ever gets `0.0.0.0` / `::` | Holds | `core/src/hosts.cpp` |
| The service never runs a program | Holds | no `CreateProcess`, `ShellExecute` or `system` in the service |
| Protected programs, folders and domains can't be targeted | Holds | `core/src/target.cpp`, `platform/src/app_blocker.cpp` |
| Data folder: admin-only, links and user-owned folders refused, atomic writes | Holds | `platform/src/files.cpp` |
| Signed state with a last-good backup | Holds at load; **fixed** so an edited file never becomes the backup (S9) | `platform/src/state_store.cpp` |
| Installs only from an admin-only folder | **Fixed** to use Windows' known folders, not environment variables (S8) | `service/src/scm.cpp`, `platform/src/paths.cpp` |
| Service, CLI and relay linked statically | Holds | `service/`, `cli/`, `host/` CMakeLists |
| Hardening flags | Holds; `clang-tidy` wasn't set up (the plan now says "later") | `cmake/Hardening.cmake` |
| Clock changes don't end locks | Held while running; **fixed** across reboots (S4) | `core/src/clock.cpp` |
| Uninstall and update refuse during a lock | **Fixed** for a stopped service (S5) | `scm.cpp`, `installer/sirenproof.iss` |
| Friend codes: only a salted hash kept, never sent to clients, compared in constant time, guesses slowed | Holds; **fixed** so a client can't supply its own hash (S10) | `core/src/policy.cpp`, `protocol.cpp` |
| Nothing leaves the PC | **Fixed** (S1: a crafted program path could make the service connect to a share) | `platform/src/process.cpp` |

## Findings

Severity is for this product: **High** can hurt the PC or the user's privacy, **Medium** defeats a lock
or the protection quickly, **Low** needs unusual rights or effort. Every fix here has a test, where one
can run without a VM; the rest are in the test tracker, section 7.

| # | Severity | What | Status |
|---|---|---|---|
| S1 | High | **The service could be made to open a network path.** `check \\server\share\x.exe` (or a running program started from a share) made the SYSTEM service read the file's version and signature, signing this PC in to that server (an NTLM relay route on domain-joined PCs) and stalling enforcement while it waited | **Fixed.** Only files on this PC's own drives are read, with no symbolic link on the way; others are matched by name and path only. A race that swaps a folder for a link mid-check remains, and needs the right to create links (not given to ordinary accounts outside Developer Mode) |
| S2 | Medium | **Any program renamed `sirenproof….exe` was exempt from app rules** | **Fixed.** Only Sirenproof's five programs, in its own folder, are exempt |
| S3 | Medium | **A copy of the service in console mode could pose as the service** while the real one was down, on the service's own pipe; clients trusted it by its path | **Fixed.** Once installed, clients accept only the process the service manager names, and console mode refuses the service's pipe |
| S4 | Medium | **Setting the clock forward while the PC was off ended locks** (the time off was taken from the wall clock unchecked) | **Fixed.** Time off counts only when that loosens nothing; time since boot always counts |
| S5 | Medium | **Uninstall went ahead whenever the service wasn't running**, for example a second after killing it | **Fixed.** A stopped service is started and asked; only one that can't start at all is removed without asking |
| S6 | Medium | **Deeply nested JSON could overflow the stack** (copying and printing a parsed value recurse) | **Fixed.** Messages nesting more than 32 levels are refused before parsing, in the service and the relay |
| S7 | Medium | **Rules belong to the whole PC.** Any signed-in user, or malware running as one, can read every rule and goal and *tighten* anything, including a week-long lock that also blocks uninstalling | **Accepted for v1.0** and documented (SECURITY.md). Separate rules per Windows user are on the roadmap for v1.x |
| S8 | Low | Install-location check and data folder came from environment variables | **Fixed** (known folders) |
| S9 | Low | An edited state file could become the backup | **Fixed** |
| S10 | Low | A client could supply its own friend-code hash, and so know the code | **Fixed.** The service always makes the code |
| S11 | Low | **Fake browser heartbeats.** A user can start the relay with a spoofed parent process and send heartbeats, keeping a browser without the extension open during a lock | **Accepted**: it takes writing a program for it. Store builds with a force-installed extension close it |
| S12 | Low | **Stop race:** Windows could deliver Stop in the moment between a lock starting and the service saying it no longer accepts Stop | **Accepted**: the service re-plans as soon as a rule changes, so the window is milliseconds, and it needs an administrator |
| S13 | Low | DLL search order: the service doesn't restrict where DLLs load from | **Accepted**: it only loads from its own admin-only folder and System32, and no staged DLL shares a Windows DLL's name. Checked in the tracker |
| S14 | Info | If both the state file and its backup are bad, the service refuses to start, so nothing is enforced | **Accepted**: starting from empty would be no better, and editing either needs an administrator. It's logged |

## Known limits, by design

- An administrator with time (Safe Mode, taking ownership, reinstalling Windows) can always remove
  Sirenproof. "Lock down this PC" is the answer, not code (PLAN §3.8).
- Bypassing on another device is out of scope.
- The relay is identified by its path, not a signature, until releases are signed.
