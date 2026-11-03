# Store listings: Chrome Web Store and Edge Add-ons

The text and answers to paste when submitting `Sirenproof-extension-<version>.zip` (made by
`tools\package.ps1`; see [RELEASING.md](RELEASING.md#the-browser-extension)). Both stores ask for much
the same.

## Basics

| Field | Value |
|---|---|
| Name | Sirenproof |
| Summary (132 characters at most) | Blocks the pages you chose, on your own rules. Works with the free Sirenproof app for Windows; nothing leaves your PC. |
| Category | Productivity (Chrome: *Workflow & Planning*; Edge: *Productivity*) |
| Language | English |
| Website | https://github.com/sanjubibin/Sirenproof |
| Support | https://github.com/sanjubibin/Sirenproof/issues |
| Privacy policy | https://github.com/sanjubibin/Sirenproof/blob/main/docs/PRIVACY.md |
| Price | Free |

## Description

> Sirenproof helps you keep the promises you make to yourself while you're calm, when the pull of a
> feed or a game is strongest.
>
> This extension is the browser half of Sirenproof, a free, open-source blocker for Windows. The
> Sirenproof app keeps your rules: which sites or single pages to block, when, and how strongly. The
> extension asks it about each page you open and shows a calm block page, with your goal and when the
> block ends, or a short pause before a site you only want to think twice about.
>
> • Block single pages, not just whole sites: YouTube's feed and Shorts, but not videos or search.
> • Catches in-page navigation, so moving from a video to Shorts is caught too.
> • Three strengths: a Nudge (a pause, then you may go on), a Block, and a Lock that holds until it ends.
> • Everything stays on your PC. The extension talks only to the Sirenproof app on the same computer,
>   makes no network requests and loads no remote code. No account, no tracking.
>
> Needs the Sirenproof app for Windows: https://github.com/sanjubibin/Sirenproof/releases

## Single purpose (Chrome asks for this)

> Blocks or pauses web pages according to the rules the user set in the Sirenproof app on the same PC.

## Why each permission is needed

| Permission | Justification to paste |
|---|---|
| `nativeMessaging` | To ask the Sirenproof app on this PC (through its registered native host, `sirenproof-host.exe`) whether a page is blocked by the user's rules. It is the only thing the extension talks to. |
| `webNavigation` | To see when a tab starts loading a page, or changes page without reloading (as YouTube does), so the rules can be checked before the page is shown. |
| `tabs` | To show Sirenproof's block or pause page in the tab that opened a blocked page, and to send it back when the user continues after a pause. |

No host permissions, no content scripts, and no remote code.

## Data use (the privacy questions)

| Question | Answer |
|---|---|
| Does it collect or use personal or sensitive data? | **No.** Page addresses are passed to the Sirenproof app on the same PC to check against the user's rules, and never leave the PC. Nothing is stored by the extension. |
| Is data sold or transferred to third parties? | No |
| Is data used for anything unrelated to the single purpose? | No |
| Is data used for creditworthiness or lending? | No |
| Remote code? | No: every script is in the package |

Chrome's form has tick boxes per data type: tick **none**. If a reviewer asks about "web history" (the
extension does see page addresses), the answer is the first row: they are only checked locally, by the
user's own app, and not collected.

## Images to prepare

| Image | Size | What to show |
|---|---|---|
| Store icon | 128 × 128 PNG | The mast-ring mark (Edge also wants 300 × 300) |
| Screenshots | 1280 × 800 PNG, 1 to 5 | 1. The block page on a YouTube Short, with a goal. 2. The pause page for a Nudge, mid-countdown. 3. The Sirenproof window's Now screen during a lock. 4. The Rules screen with a painted schedule |
| Small promo tile (Chrome) | 440 × 280 PNG | Name and "Tie yourself to the mast" on the deep-water background |

The window screens can be rendered without a running service:
`sirenproof-ui.exe --screen rules --screenshot rules.png` (TESTING.md). The block and pause pages are
`extension/block.html`.

## After the first listing

Each store gives the extension a new id. Add both ids to `kExtensionIds` in
`core/include/sirenproof/core/browser.hpp` and make a new release, as RELEASING.md describes; until
then the store versions can't reach the app.
