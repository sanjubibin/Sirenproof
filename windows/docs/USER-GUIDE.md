# Sirenproof user guide

Sirenproof blocks the websites and apps that pull you off track, on rules you set while you're calm.
When the pull is strongest, it holds you to them and reminds you of your goal.

Everything stays on your PC: no account, no cloud, no tracking.

- [Install](#install)
- [The three strengths](#the-three-strengths-nudge-block-lock)
- [Start a lock](#start-a-lock)
- [Rules on a schedule](#rules-on-a-schedule)
- [Ending a lock early](#ending-a-lock-early)
- [Breaks](#breaks)
- [The browser extension](#the-browser-extension)
- [Lock down this PC](#lock-down-this-pc)
- [Keyboard: the command palette](#keyboard-the-command-palette)
- [Uninstall](#uninstall)
- [Questions](#questions)

## Install

1. Download `Sirenproof-<version>-setup.exe` from the
   [releases page](https://github.com/sanjubibin/Sirenproof/releases).
2. Check it's the real file. In PowerShell, in your Downloads folder:

   ```powershell
   Get-FileHash .\Sirenproof-0.1.0-setup.exe -Algorithm SHA256
   ```

   The long number must match the line for that file in `SHA256SUMS.txt` on the releases page.
3. Run it. Windows asks for administrator rights once, to install the background service that keeps
   your rules.
4. Leave **Start Sirenproof now** ticked. The Sirenproof icon appears in the taskbar's notification
   area, and from now on it starts whenever you sign in.

Click the icon to open the main window. You can close the window at any time: blocking carries on.

## The three strengths: Nudge, Block, Lock

| | What happens when you open it | How you can loosen it |
|---|---|---|
| **Nudge** | A pause shows your goal and counts down 15 seconds. Then you choose: back to work, or continue for 5 minutes. A program isn't closed; the pause covers the screen | Changes wait 2 minutes |
| **Block** | A calm page with your goal and when the block ends. A program is closed, after 10 seconds to save your work | Changes wait 10 minutes, and you can cancel them while they wait |
| **Lock** | Blocked, like Block | **Not at all** until it ends, except by the way out you chose when it started |

Two promises hold everywhere:

- **Tightening applies at once.** Adding a site, adding hours, making a rule stronger.
- **Loosening waits.** Removing a site, clearing hours, making a rule weaker. The change shows under
  **Waiting to apply** on the Now screen with a countdown, and one click cancels it. A lock refuses
  loosening outright.

## Start a lock

A lock is for "this matters, hold me to it": an exam, a deadline, an evening off screens.

1. Open the window and choose **Start a lock**.
2. **What to lock:** pick ready-made lists (Video, Social, Games, News), or add your own.
3. **For how long:** 25 minutes to 4 hours.
4. **If you need out early:** choose now, because you can't change it later (see
   [Ending a lock early](#ending-a-lock-early)).
5. **What is this lock for?** Your goal is shown whenever something is blocked.
6. Check **Before you commit**: it lists exactly what will be blocked and which programs will close.
7. **Press and hold** the round button for two seconds. Letting go early cancels.

The Now screen then shows the lock as a ring that empties as time passes.

## Rules on a schedule

For "no social media during work hours", every week:

1. Open **Rules** and press **New rule**.
2. Pick lists, or type sites, pages or apps and press Enter. Examples:

   | Type | Blocks |
   |---|---|
   | `reddit.com` | the whole site |
   | `youtube.com/shorts` | that part of a site |
   | `youtube.com/` | only YouTube's home page (videos still play) |
   | `app:steam.exe` | a program, even if it's renamed |

3. Choose **Nudge** or **Block**.
4. Under **When**, choose **On a schedule** and paint the hours: drag across the grid to add hours, or
   start on a lit hour to clear them. It starts with Monday to Friday, 9:00 to 17:00.
5. Press **Create**.

Painting is free until you press Create. Afterwards, adding hours applies at once and clearing them
waits out the rule's delay, like any loosening.

To change a rule later, select it in the list. **+ Video**, **+ Social** and the other list buttons add
a whole list in one go. The **Rule tester** at the bottom tells you whether an address or program would
be blocked right now, and by which rule.

Windows itself, Windows Update and sign-in, security software, screen readers and magnifiers, and
Sirenproof itself can never be blocked.

## Ending a lock early

On the Now screen, press **Unlock early…**. What happens depends on what you chose when the lock
started:

- **No way out:** it ends when it ends.
- **Wait:** press **Start the wait**. The lock ends 30 minutes later, unless you press **Keep the lock**
  first. Often the urge has passed by then.
- **Type it out:** press **Show the text to type** and copy 200 random characters by hand. Pasting
  doesn't work. The text lights up as you get it right, and the first wrong character is marked. If you
  close the window part-way, you get the same text back, but have to type it again.
- **A friend's code:** when the lock started, Sirenproof showed a code once. You gave it to someone you
  trust. Ask them for it and type it in; capitals and dashes don't matter. After five wrong codes you
  have to wait 10 minutes, and longer each time after that.

Ending a lock early only ends that lock. Your scheduled rules still apply.

## Breaks

**Take a 5-minute break** (on the Now screen, or right-click the tray icon) pauses rules that allow
breaks. You get three a day, and they end on their own. A lock pauses for breaks only if it was set up
to.

## The browser extension

Whole sites are blocked in every browser without anything extra. To block **parts** of sites (YouTube
Shorts but not videos, say) and to see the pause page for Nudge rules, Chrome or Edge needs
Sirenproof's extension.

Until it's in the Chrome Web Store and Edge Add-ons:

1. Open `chrome://extensions` (Chrome) or `edge://extensions` (Edge).
2. Turn on **Developer mode**.
3. Choose **Load unpacked**, and pick `C:\Program Files\Sirenproof\extension`.

During a lock, a browser without the extension gets a warning and is closed after 60 seconds, and
private windows are turned off. Everything goes back to normal when the lock ends.

## Lock down this PC

If your everyday Windows account is an administrator, it could switch Sirenproof off. **Lock down this
PC** (offered on the Now screen during a lock) walks you through:

1. creating a second administrator account;
2. handing its password to someone you trust, or sealing it in an envelope;
3. making your own account a standard one.

After that, stopping Sirenproof, uninstalling it, editing the hosts file or changing the clock all need
that other password. Sirenproof never changes accounts itself: it opens Windows' own settings for each
step, and you can undo it with the other password.

## Keyboard: the command palette

Press **Ctrl+K** anywhere in the window and type:

| Type | Does |
|---|---|
| `lock youtube 2h` | fills in a 2-hour lock on YouTube (you still hold to start it) |
| `lock video social 90m --unlock type` | a lock on two lists, with the typing way out |
| `block reddit 25m` | blocks Reddit for 25 minutes |
| `nudge twitch 1h` | a nudge before Twitch for an hour |
| `break` | a 5-minute break |
| `check reddit.com` | would it be blocked right now? |
| `rules`, `now`, `lock` | go to that screen |

The `sirenproof` command in `C:\Program Files\Sirenproof` does the same from a terminal; run it without
arguments for help.

## Uninstall

Settings → Apps → Sirenproof → Uninstall. It asks whether to keep your rules for a later reinstall.

**While a lock holds, uninstalling (and updating) is refused** until the lock ends. That's on purpose.
Everything Sirenproof changed (the service, its section of the hosts file, browser settings during a
lock) is put back.

## Questions

**Does Sirenproof send anything anywhere?** No. It has no network code. Your rules and its log stay in
`C:\ProgramData\Sirenproof`, which only administrators can read, and the pages you visit aren't
recorded at all. [PRIVACY.md](PRIVACY.md) has the details.

**Windows says the publisher is unknown.** The build isn't signed yet. Check the SHA-256 as in
[Install](#install) before running it.

**A site still loads.** Whole sites are blocked through Windows' hosts file, and a browser may need a
moment or a restart to forget it. Parts of sites need the [extension](#the-browser-extension). Use the
Rule tester to see what Sirenproof thinks.

**A program closed and I lost work.** Block and Lock rules give 10 seconds' warning. For programs you
only want to think twice about, use Nudge: the program is never closed.

**I changed the clock and the lock didn't end.** Sirenproof counts lock time with a clock that changing
Windows' time can't move.

**Something's wrong.** Report it on the [issues page](https://github.com/sanjubibin/Sirenproof/issues).
For a security problem, see [SECURITY.md](../../SECURITY.md) instead.
