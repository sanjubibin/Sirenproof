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

## Problems found

| # | Test | What happened | Fixed in |
|---|---|---|---|
| | | | |
