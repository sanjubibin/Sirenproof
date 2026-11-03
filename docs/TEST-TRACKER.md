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

## Problems found

| # | Test | What happened | Fixed in |
|---|---|---|---|
| | | | |
