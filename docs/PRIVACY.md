# Privacy

*Applies to the Sirenproof app for Windows and its browser extension. Last changed 3 November 2026.*

**Sirenproof collects nothing. Nothing it knows leaves your PC.**

- **No network code.** The app's service, its command line, its window, its tray icon and the
  extension's relay don't connect to the internet. The extension makes no network requests of its own
  and loads no remote code.
- **What stays on your PC.** Your rules, locks and breaks, and a log of changes to them and of programs
  it closed, are kept in `C:\ProgramData\Sirenproof`, which only administrators and Windows itself can
  read.
- **The extension.** When you open a page, the extension passes its address to the Sirenproof app on
  the same PC, which checks it against your rules and answers "allow", "pause" or "block". Page
  addresses aren't stored, by the extension or by the app.
- **No accounts, no analytics, no ads, no tracking**, and nothing is sold or shared, because there's
  nothing to share.
- **Removing it.** Uninstalling can delete everything in `C:\ProgramData\Sirenproof`; the uninstaller
  asks.

Sirenproof is open source, so all of this can be checked:
https://github.com/sanjubibin/Sirenproof. Questions go to the
[issues page](https://github.com/sanjubibin/Sirenproof/issues).
