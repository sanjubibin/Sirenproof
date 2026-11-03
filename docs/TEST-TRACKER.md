# Test tracker

Everything built since 3 November 2026 that still has to be tried on a real Windows PC. The work was
written and compiled on one PC but **not run there**. Every check below is for the test PC (or its
virtual machine, where marked **VM**). How to run things safely is in [TESTING.md](TESTING.md).

**How to use it.** Pull, build, then work down the tables. Set **Result** to `Pass`, `Fail` or
`Blocked`, and add the date and anything odd under **Notes**. A failure gets a line in
[Problems found](#problems-found) at the bottom, so it can be fixed and tried again.

| Mark | Meaning |
|---|---|
| ☐ | Not tried yet |
| ✅ | Pass |
| ❌ | Fail (see Problems found) |
| ⏸ | Blocked (say why) |

## Where things stand

| Section | What | Checks | Done |
|---|---|---|---|
| [0](#0-set-up-the-test-pc) | Set up the test PC | 5 | 0 |
| [1](#1-unlock-challenges-m4) | Unlock challenges | 24 | 0 |
| [2](#2-ready-made-lists-and-new-rule-m4) | Ready-made lists and New rule, and the usability test | 20 | 0 |
| [3](#3-installer-m5--vm) | Installer, Defender, SmartScreen (VM) | 18 | 0 |
| [4](#4-release-workflow-checksums-and-signing-m5) | Release workflow, checksums, signing | 6 | 1 |
| [5](#5-docs-and-store-listings-m5) | Docs and store listings | 6 | 0 |
| [6](#6-earlier-acceptance-tests-m2m4--vm) | Earlier acceptance tests (VM) | 12 | 0 |
| [7](#7-security-review-follow-ups) | Security review follow-ups (VM) | 14 | 0 |

Update **Done** as you go, so this table shows the state at a glance.

## 0. Set up the test PC

| # | Step | Result | Notes |
|---|---|---|---|
| 0.1 | Install [MSYS2](https://www.msys2.org/), then in its UCRT64 shell: `pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-catch mingw-w64-ucrt-x86_64-nlohmann-json mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-declarative` | ☐ | |
| 0.2 | `git pull`, then `cmake --preset debug` and `cmake --build --preset debug` | ☐ | |
| 0.3 | `ctest --preset debug`: every test passes | ☐ | On the PC this was written on, one platform test, *read_administrators agrees with this process's own token*, already failed before any of this work (that PC signs in through Azure AD). Note whether it fails here too |
| 0.4 | `node --test extension/tests/background.test.mjs` | ☐ | |
| 0.5 | `cmake --build --preset release` builds too (the acceptance tests need it) | ☐ | |

## 1. Unlock challenges (M4)

Start the console service from a debug build (see TESTING.md, *Trying the service safely*), then the
main window. Debug builds cap a lock at 2 minutes, so make the lock from the command line with a short
text, and open **Now → Unlock early…** in the window:

```powershell
build\debug\bin\sirenproof.exe block reddit.com --for 2m --lock --unlock type:20 --id typing
```

### 1a. Type it out

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 1.1 | Look at the button before starting | It says how many characters (20) | ☐ | |
| 1.2 | **Show the text to type** | The text appears in groups of five, the cursor is in the box below | ☐ | |
| 1.3 | Type the first few characters correctly | They turn sea glass; the bar fills; "4 of 20 characters" | ☐ | |
| 1.4 | Type one wrong character | That character is marked amber in the text, the bar and box turn amber, "Character 5 doesn't match" | ☐ | |
| 1.5 | Backspace it and go on | Back to normal | ☐ | |
| 1.6 | Ctrl+V, Shift+Insert, right-click → Paste, and dragging text in from Notepad | Nothing is added; a message says only typing counts | ☐ | |
| 1.6b | Type some characters, select them all, then right-click → Paste text of the same length | Undone, with the same message | ☐ | |
| 1.6c | Select one character and type over it | Accepted (one character in) | ☐ | |
| 1.7 | Type in capitals, with or without the dashes | Still counts as right | ☐ | |
| 1.8 | Press Enter before it's all typed | Nothing happens, no new line | ☐ | |
| 1.9 | Close the window part-way, open it again, **Show the text to type** | The same text as before (what you typed is gone) | ☐ | |
| 1.10 | **Start over** | The box empties | ☐ | |
| 1.11 | Type all of it, then Enter | "Unlocked"; Reddit is allowed again (`sirenproof check https://reddit.com`) | ☐ | |
| 1.12 | Narrator on (Ctrl+Win+Enter): move through the challenge | The text and the box are read out with sensible names | ☐ | |

### 1b. Friend code

```powershell
build\debug\bin\sirenproof.exe block reddit.com --for 2m --lock --unlock friend --id friend
```

Write down the code it prints. Then in the window, **Unlock early…**:

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 1.13 | Type the code without dashes | Dashes appear after every five characters; "10 of 15 characters" | ☐ | |
| 1.14 | Type `O`, `I` or `L` | They become 0, 1 and 1; `U` and symbols are ignored | ☐ | |
| 1.15 | Paste the code (Ctrl+V) | Allowed, and tidied the same way | ☐ | |
| 1.16 | Enter a wrong code (all zeros), Enter | Amber: "That's not it. 4 tries left, then a 10-minute wait." | ☐ | |
| 1.17 | Four more wrong codes | The field is disabled, with a countdown to when you can try again | ☐ | Debug locks end after 2 minutes, before the 10-minute wait does. Fine to see the countdown start, then let the lock end |
| 1.18 | New lock (as above), enter the right code | "Unlocked" | ☐ | |

### 1c. Friend code when a lock starts

In the window: **Start a lock → A friend's code → Hold to lock**.

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 1.19 | Look at the code | Large, upper case, groups spaced apart | ☐ | |
| 1.20 | Try to close it straight away | **Done** stays disabled until "Someone else has the code now" is ticked | ☐ | |
| 1.21 | Tick it, **Done** | The popup closes; Now shows the lock | ☐ | |

### 1d. Wait

```powershell
build\debug\bin\sirenproof.exe block reddit.com --for 2m --lock --unlock wait:1 --id waiting
```

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 1.22 | **Unlock early…** | The text and button say 1 minute, not 30 | ☐ | |

## 2. Ready-made lists and New rule (M4)

Console service and main window as in section 1. Go to **Rules**.

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 2.1 | With no rules yet | The list says to press **New rule**; no mention of the command line | ☐ | |
| 2.2 | **New rule** | The card opens where a rule would show; the button greys out | ☐ | |
| 2.3 | Pick **Social** and **News**, type `twitch.tv, app:notepad.exe` and press Enter | Two chips appear; Create says how many targets | ☐ | |
| 2.4 | Remove a chip with × | It goes | ☐ | |
| 2.5 | Look at **When** | "On a schedule" is chosen and the grid shows Mon–Fri 09:00–17:00 | ☐ | |
| 2.6 | Paint Saturday 10:00–12:00 on, then clear Friday afternoon | The grid changes at once; nothing is sent yet (`sirenproof rules` shows no new rule) | ☐ | |
| 2.7 | Clear every hour | A message says a schedule needs at least one hour; the grid keeps the last hour | ☐ | |
| 2.8 | Choose **Nudge**, name it "Weekdays", **Create** | "Rule added"; the rule is selected, named Weekdays, NUDGE, with the painted hours | ☐ | |
| 2.9 | `sirenproof rules` | The same rule, with a 2-minute loosen delay (Nudge) and the same windows | ☐ | |
| 2.10 | **New rule → Always → Block**, one target, Create | A rule that says "Always" | ☐ | |
| 2.11 | **New rule**, then **Cancel** | Back to the selected rule; the card is empty when opened again | ☐ | |
| 2.12 | **New rule** with a bad target (`not a site`) | An error names it; nothing is created | ☐ | |
| 2.13 | Select a rule: under **Blocks**, the list buttons | `+ Video`, `+ Social`, `+ Games`, `+ News`, with a tooltip saying what each blocks | ☐ | |
| 2.14 | Press `+ Video` | Its three pages appear as chips at once; the button turns to `✓ Video` and greys out | ☐ | |
| 2.15 | Remove one Video chip | The button is `+ Video` again (pressing it adds only the missing one) | ☐ | Removing loosens, so on a rule with a delay it waits first; it shows under **Waiting to apply** on Now |
| 2.16 | During a lock (section 1), open the lock's rule and press `+ News` | Applied at once (it only tightens) | ☐ | |
| 2.17 | **Start a lock** screen | Four lists (Video, Social, Games, News), two to a row in a narrow window; picking News adds its line to the preview | ☐ | |
| 2.18 | Palette (Ctrl+K): `lock news 1h` | The Start a lock screen with News chosen | ☐ | |
| 2.19 | Keyboard only: Tab through the New rule card | Every control is reachable and shows a focus ring | ☐ | |
| 2.20 | Usability: someone who has never seen Sirenproof sets up "block Reddit on weekday evenings" from **Rules**, without help | Done within a few minutes | ☐ | This is the M4 usability test (ROADMAP); do it with two or three people and note where they hesitate |

## 3. Installer (M5) — **VM**

The installer changes the whole PC, so it belongs in the VM (TESTING.md, *Acceptance test*), starting
from the **clean** snapshot each time. Make it on the test PC with
`powershell -ExecutionPolicy Bypass -File tools\package.ps1` (needs [Inno Setup 6.3+](https://jrsoftware.org/isdl.php)),
or download the **sirenproof-installer** artifact from the CI run. Copy
`build\package\Sirenproof-<version>-setup.exe` into the VM.

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 3.1 | `tools\package.ps1` on the test PC | Ends with "Done" and lists two SHA-256 sums; `build\package\stage` has the five programs, Qt's DLLs and `qml\` folder, and `libstdc++-6.dll` | ☐ | Also check the CI run's *Package the installer* step went green |
| 3.2 | `build\package\stage\sirenproof-ui.exe` on a PC **without** MSYS2 (or with it off the PATH) | The window opens | ☐ | Proves the staging found every DLL. The VM is a good place |
| 3.3 | Run the setup in the VM | Asks for administrator rights, shows the GPL, no folder page; installs to `C:\Program Files\Sirenproof` | ☐ | |
| 3.4 | Finish with "Start Sirenproof now" ticked | The tray icon appears; `sc query Sirenproof` is RUNNING | ☐ | |
| 3.5 | Start menu → **Sirenproof** | The main window opens and is connected | ☐ | |
| 3.6 | Sign out and in again (and sign in as a second user, if there is one) | The tray starts by itself | ☐ | |
| 3.7 | `"C:\Program Files\Sirenproof\sirenproof-service.exe" can-stop` (no lock) | Exit 0, "No lock holds" | ☐ | `echo %errorlevel%` |
| 3.8 | Start a 25-minute lock in the window, then run `can-stop` again | Exit 3 | ☐ | |
| 3.9 | During the lock: Settings → Apps → Sirenproof → Uninstall | Refused with the "a lock is active" message; nothing is removed | ☐ | |
| 3.10 | During the lock: run the same setup again (an update) | Refused on the "Preparing to install" page with the same message | ☐ | |
| 3.11 | During the lock: `"C:\Program Files\Sirenproof\unins000.exe" /SILENT` | Doesn't uninstall | ☐ | |
| 3.12 | After the lock ends: run the setup again | Updates; the service is running again and your rules are still there | ☐ | |
| 3.13 | Uninstall, answer **No** to deleting your rules | Removed; `sc query Sirenproof` says it doesn't exist; the hosts file has no Sirenproof section; the tray icon is gone; `C:\ProgramData\Sirenproof` is still there | ☐ | |
| 3.14 | Install again | Your rules from before are back | ☐ | |
| 3.15 | Uninstall, answer **Yes** | As 3.13, and `C:\ProgramData\Sirenproof` is gone too | ☐ | |
| 3.16 | After uninstalling: `C:\Program Files\Sirenproof`, the Start menu, `HKLM\Software\Microsoft\Windows\CurrentVersion\Run` | All gone | ☐ | |
| 3.17 | Microsoft Defender: full scan of `C:\Program Files\Sirenproof`, and watch **Protection history** during 3.4–3.12 | Nothing flagged; if the hosts-file edits are flagged, note exactly what | ☐ | This is the M5 "testing against Defender" item |
| 3.18 | SmartScreen on the downloaded (unsigned) setup | Note what it says | ☐ | Expected until it's signed (section 4) |

## 4. Release workflow, checksums and signing (M5)

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 4.1 | After pushing to `main`: the CI run | Both presets green; the `ci-release` job has a **sirenproof-installer** artifact with the setup, the extension zip and `SHA256SUMS.txt` | ✅ | 3 Nov 2026, [run 12](https://github.com/sanjubibin/Sirenproof/actions/runs/41605924255) (commit `e00e5b0`): debug, release and extension jobs green; artifact 31.8 MB. Run 11 had failed on two new platform tests, fixed in that commit |
| 4.2 | `Get-FileHash` on the downloaded setup | Matches its line in `SHA256SUMS.txt` | ☐ | |
| 4.3 | Open the extension zip | No `"key"` in its `manifest.json`; no `tests` folder | ☐ | |
| 4.4 | Change only `extension/manifest.json`'s version, run `tools\package.ps1 -SkipBuild` | Stops, saying the two versions differ. Put it back | ☐ | |
| 4.5 | On a fork of the repository, push the tag `v0.1.0` (the version in `CMakeLists.txt`) | **Release** runs and leaves a *draft* release with the three files; nothing is published | ☐ | Delete the draft and the tag afterwards. A tag that doesn't match the version must fail at *Check the tag* |
| 4.6 | Once there's a certificate: `tools\package.ps1 -Sign -CertThumbprint <thumbprint>` | `Get-AuthenticodeSignature` says `Valid` for the five programs, the setup and `unins000.exe`; SmartScreen names the publisher | ⏸ | Needs a certificate (RELEASING.md, *Code signing*) |

## 5. Docs and store listings (M5)

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 5.1 | Follow [USER-GUIDE.md](USER-GUIDE.md) from *Install* to *Uninstall* in the VM, as a newcomer would | Every step, label and button it names is there and does what it says | ☐ | Note any wording that doesn't match the window |
| 5.2 | `C:\Program Files\Sirenproof\USER-GUIDE.md` and `CHANGELOG.md` | Installed with the app | ☐ | |
| 5.3 | Load `C:\Program Files\Sirenproof\extension` unpacked, as the guide says | Loads, with the id `cgpgilkcgaojdaclgkllhndfgdphgdje`; blocking a Short works | ☐ | |
| 5.4 | Chrome Web Store: submit the zip with the text in [STORE-LISTING.md](STORE-LISTING.md) | Accepted; note the store's id | ⏸ | Needs the $5 developer account. Then add the id to `kExtensionIds` (RELEASING.md) |
| 5.5 | Edge Add-ons: the same | Accepted; note the id | ⏸ | Needs a Partner Center account |
| 5.6 | After 5.4/5.5 and a new release: install the extension from each store | It reaches the app (a blocked Short shows the block page) | ⏸ | |

## 6. Earlier acceptance tests (M2–M4) — **VM**

These were written before this tracker; the steps and expected results are the numbered rows in
[TESTING.md, *Acceptance test in a virtual machine*](TESTING.md#acceptance-test-in-a-virtual-machine-milestone-m2).
Now that there's an installer, install with it (section 3) instead of copying files by hand.

| TESTING.md rows | What | Result | Notes |
|---|---|---|---|
| 1–2 | YouTube and a "game" are blocked during a lock | ☐ | |
| 3 | Hosts file edits are repaired within a second | ☐ | |
| 4–5 | The service can't be stopped, and comes back when killed | ☐ | |
| 6–7 | Clock and time zone changes don't end the lock | ☐ | |
| 8 | The lock survives a restart | ☐ | |
| 9–11 | Uninstalling is refused during the lock, and clean after | ☐ | Do it through the installer's uninstaller too (3.9, 3.13) |
| 12–13 | The extension blocks Shorts and the feed while videos play | ☐ | |
| 14 | No Incognito or InPrivate during a lock | ☐ | |
| 15–16 | A browser without the extension is closed after the grace period | ☐ | |
| 17 | Browser policies are back as they were after uninstalling | ☐ | |
| 18–24 | Lock down this PC | ☐ | |
| — | The interface budgets on the VM: `tools\measure-budgets.ps1` (TESTING.md, *Interface budgets*) | ☐ | The figures in TESTING.md are from a fast laptop; a slower PC is worth a row |

## 7. Security review follow-ups

From [THREAT-MODEL.md](THREAT-MODEL.md). The unit and platform tests for these run in 0.3. The rows
below need the installed service, so they're for the **VM** (with the installer, section 3).

| # | Try this | Expected | Result | Notes |
|---|---|---|---|---|
| 7.1 | S1: as a standard user, `sirenproof check \\127.0.0.1\c$\Windows\notepad.exe` | Answers at once ("allow"); `sirenproof status` responds normally straight after | ☐ | Before the fix the service read the file over SMB as SYSTEM |
| 7.2 | S1: with a rule on `app:notepad.exe`, run Notepad from `\\127.0.0.1\c$\Windows\notepad.exe` | Still closed (matched by name) | ☐ | |
| 7.3 | S2: copy a "game" to `C:\Games\sirenproof.exe`, block `app:C:\Games\`, start it | Closed | ☐ | |
| 7.4 | S2: during any app rule, Sirenproof's own window and tray keep running | Never closed | ☐ | |
| 7.5 | S3: `sirenproof-service.exe console` on the VM (service installed) | Refuses: "Sirenproof is installed on this PC, so its pipe belongs to the service…" | ☐ | |
| 7.6 | S3: as a **standard user** (after Lock down this PC), the window, tray and `sirenproof status` | All connect to the service | ☐ | Important: the clients now check the service's process id, which standard users can read; before, they checked its image path, which they possibly couldn't |
| 7.7 | S4: start a 25-minute lock, shut the VM down, set the VM's clock (firmware/host time) a day ahead, start it | The lock still holds, with about the time it had left | ☐ | VirtualBox: `VBoxManage modifyvm <vm> --biossystemtimeoffset 86400000`; set it back to 0 afterwards |
| 7.8 | S4: start a 25-minute lock, shut down for 10 real minutes, start again | Ends at the original time (the time off counted) | ☐ | |
| 7.9 | S5: during a lock, as administrator: `taskkill /f /im sirenproof-service.exe` and straight away `sirenproof-service uninstall` | Refused (exit 3), the service is running again | ☐ | Try the same with the uninstaller from Settings |
| 7.10 | S5: with no lock, stop the service (`sc stop Sirenproof`), then uninstall | Removed cleanly | ☐ | The uninstall starts it, asks, and stops it again |
| 7.11 | S6: send a deeply nested message to the pipe (PowerShell below) | The service answers with an error and keeps running | ☐ | See the snippet after this table |
| 7.12 | S10: `sirenproof block reddit.com --for 2m --lock --unlock friend` twice | A different code each time | ☐ | That the service ignores a client's own `salt`/`hash` is in `protocol.cpp`; nothing more to run |
| 7.13 | S13: `Get-ChildItem "C:\Program Files\Sirenproof" -Filter *.dll` | No DLL with the name of one in `C:\Windows\System32` (compare the lists) | ☐ | |
| 7.14 | Defender and SmartScreen notes from 3.17–3.18 | | ☐ | |

The nesting check (7.11), from an administrator PowerShell on the VM (administrators may open the pipe
with any rights; this asks for read and write):

```powershell
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream('.', 'Sirenproof.v1', 'InOut')
$pipe.Connect(2000); $pipe.ReadMode = 'Message'
$bytes = [Text.Encoding]::UTF8.GetBytes('{"v":1,"op":"status","args":' + ('[' * 30000) + '}')
$pipe.Write($bytes, 0, $bytes.Length)
$buffer = New-Object byte[] 65536; $n = $pipe.Read($buffer, 0, $buffer.Length)
[Text.Encoding]::UTF8.GetString($buffer, 0, $n)   # {"ok":false,"error":"the message nests too deeply"}
$pipe.Dispose(); sc.exe query Sirenproof         # still RUNNING
```

## Problems found

| # | Test | What happened | Fixed in |
|---|---|---|---|
| | | | |
