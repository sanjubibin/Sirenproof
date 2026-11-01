# Sirenproof research

Evidence behind the decisions in [PLAN.md](PLAN.md). Collected November 2026.

Three questions:

1. **People.** What distracts Windows users, how it feels, and why their attempts to stop it fail.
2. **Existing tools.** What people use today and where those tools fall short.
3. **Mechanics.** How blocking can be done on Windows, and what it costs in reliability, safety and trust.

Each claim cites its sources. Support levels: **[Strong]** means five or more independent sources, **[Moderate]**
means two to four, **[Single]** means one. User quotes are kept short. Most of the text paraphrases its sources.

---

## 1. People

### Sources and caveats

- 38 sources: Reddit (read through an archive mirror because Reddit blocks direct fetching), Hacker News,
  Blind, Steam forums, Trustpilot, browser-store reviews, AlternativeTo, blogs and peer-reviewed studies.
  Most date from 2022–2026.
- Several "blockers never work for me" posts come from developers promoting their own tool. Their framing
  of failure is real but motivated.
- The two strongest intervention studies ([U30], [U31]) measured phones, not PCs.

### 1.1 Who struggles

| Group | What's specific to them | Support |
|---|---|---|
| **People with ADHD** | The urge arrives faster than a decision. They forget to turn a block back on after a legitimate use, and want a block that holds at their worst moment. | Strong [U1][U3][U4][U5][U6][U7][U26] |
| **Students** | The laptop is the study tool, and YouTube, meme sites and games take over exam time. One study logged 37 minutes of non-course browsing in a 1h50m lecture [U32]. Another found non-course apps active about 42% of the time [U33]. | Moderate [U5][U24][U26][U11] |
| **Knowledge workers and developers** | Waiting on builds or slow tools triggers drift to Reddit, HN and news. Leads to "nothing done" days and feeling dishonest toward colleagues [U22], or working all day and bingeing every evening [U16]. Cyberloafing tracks boredom and low workload [U34]. | Moderate [U6][U16][U17][U22][U23] |
| **Gamers** | The most Windows-specific group. Their distractions are executables (Dota 2, Apex, CS2) and Discord voice, not domains. Some take drastic steps: one student broke their laptop to stop playing [U24]. | Moderate [U9][U24][U3][U11] |
| **Recovery (compulsive use)** | Want maximum strength during cravings. | Moderate, thin [U11][U12] |

The needs differ by group:

- ADHD users need protection against how fast the urge arrives, and automatic re-locking.
- Developers need mixed-use sites to keep working.
- Gamers need app blocking.
- Recovery users need locks that cannot be broken.

### 1.2 What distracts on Windows

- **Sites [Strong].** YouTube leads by far. Reddit, HN, news and politics, Facebook, Instagram, X and
  short-form video follow [U4][U5][U6][U8][U16][U17][U21].
- **Desktop apps [Moderate].** Steam games, Discord, small games, media players. For some, the PC is
  where they spend their whole day [U3][U6][U7][U9][U21].
- **The "quick check" loop [Strong].**
  - URLs get typed on autopilot.
  - One letter plus browser autocomplete reopens a site closed 20 minutes earlier [U4].
  - Users describe a game → YouTube → game cycle they don't even enjoy [U25].
- **Phone vs PC [Moderate, split].** Blocking only the PC pushes some people to the phone
  [U10][U12][U21]. Others deliberately move social media to the PC because it is clunkier there [U7].
- **Notifications [Thin].** Users rarely mention them; self-started drift dominates. Windows Focus only
  silences notifications and blocks neither apps nor sites [U36].

### 1.3 How it feels

- **Autopilot [Strong].** People describe not having chosen at all: "I didn't *decide* to do this"
  [U4], or their hands moving before their brain [U5].
- **A split self [Moderate].** A "weak moment" self overrides the planning self. Tools that hold in that
  moment get praised [U11][U26].
- **Guilt and dishonesty at work [Moderate].** People feel they are letting colleagues down [U22][U23].
  Regret after browsing is suggested as the honest measure [U17].
- **A vicious circle [Moderate].** Tired leads to gaming, which leads to more tiredness [U9].
  Procrastination is framed as a problem of managing emotions, not time [U16].
- **Shame about needing a tool [Moderate].** Some see a blocker as admitting defeat [U16]. Forum replies
  push "just be disciplined" [U24][U26]. Others find a locked block oddly freeing [U18].
- **Who gets blamed.**
  - When users bypass a block, they blame themselves.
  - When a tool breaks email, banking or the browser, they turn on the tool: one called a blocker malware
    after it locked them out of their work [U10].
  - Windows users also worry blockers look sketchy to download [U3].

### 1.4 What they tried and why it failed

- **Willpower** came first and failed for almost everyone [U16][U21].
- **Browser extensions [Strong].** Users disable them within minutes, or switch to incognito or another
  browser. They also break when sites change [U1][U2][U5][U15][U16][U21].
- **The hosts file.** Only technical users manage it. Anyone with admin rights can undo it, and the error
  pages are confusing [U17][U19][U26].
- **Paid blockers.**
  - Freedom users just close the program or switch to their phone [U12].
  - A Cold Turkey user got around it with Safe Mode plus deleting its folder [U10].
  - One gamer documented a whole escalation against Cold Turkey [U9]: uninstalling it, hidden uninstallers,
    copying the game folder elsewhere, moving the clock forward, deleting registry keys and editing its
    database.
- **Windows built-ins.** Focus doesn't block anything [U36]. Family Safety can only restrict an adult if
  someone else organizes it [U37]. We found no first-hand accounts of people using Family Safety on
  themselves.
- **Whack-a-mole [Strong].** Block one site and the brain finds the next one [U4][U6][U16][U17].
- **After bypassing.**
  - People describe the habit coming back worse [U4], or a tool that works for a day or two before a
    bigger binge [U16].
  - Beating the blocker can become a game of its own [U9].
  - HabitLab logs from 8,000+ users: people start strict, drift to easier settings, and keep believing they
    will tighten up again soon [U28].

### 1.5 What they want

| Want | Detail | Support |
|---|---|---|
| **Locks that hold in the moment** | Early unlock should cost time or effort: a 1–3 hour delay, typing a long random string, a code a friend holds, or a hardware key kept out of the house. | Strong [U1][U3][U9][U10][U11][U27] |
| **Gentle friction for reflexive opens** | Delays, hold-to-continue, pauses. Strict-only tools were "too strict" and got disabled for good [U20]. | Strong but contested [U2][U5][U20][U30][U31] |
| **Breaks that end by themselves** | Exceptions that expire and re-lock automatically. | Moderate [U1][U5] |
| **Blocking part of a site** | YouTube tutorials and search yes; feed, Shorts and autoplay no. Mixed-use sites are why people unblock and then forget. | Strong [U5][U8][U10][U14][U29] |
| **Apps and games on the same schedule as sites** | Putting .exe blocking behind a paywall is resented. | Moderate [U3][U9][U10][U14][U21] |
| **Fair price** | One-time prices get praise. Subscriptions and stingy free tiers (2 hours, 3 sites) get resentment. | Strong [U10][U11][U12][U13] |
| **Safety and trust** | Never lock me out of the whole system. Clean uninstall. Unease about the access extensions ask for. | Moderate [U3][U10][U11][U21] |
| **Easy setup** | Confusing interfaces are a common complaint. Start with mild locks. | Moderate [U10][U12][U15][U17] |
| **No shaming** | Repeated goal reminders can feel nagging. | Thin [U29] |
| **Accountability** | A friend holds the password. A place where opening sites feels watched. | Thin [U5][U27] |

### 1.6 Surprises

- **The same lock earns opposite reviews.** Cold Turkey's hard lock is called life-changing and also
  malware [U10][U11]. Strength without safety rails destroys trust.
- **What people say vs what studies measure.**
  - Users say only hard locks work [U11][U18], yet studies measure gentle friction working:
    - A one-second pause before opening an app cut opens of the target app by 57% over six weeks
      (N=280) [U30].
    - People accepted gentle slowdowns 61.6% of the time versus 36.3% for lockouts [U31].
  - Both studies used phones, but they suggest friction and locks each fit different moments.
- **People overestimate the strictness they want** at setup, then drift looser [U28].
- **Distraction is often coping.** It tracks boredom [U34]. Cutting a habit leaves a hole [U6]. Blocking
  without a substitute just moves the habit somewhere else.

### 1.7 Ranked insights

1. **The urge is the enemy, not the site.** An instant off switch becomes the default, so every unlock
   must cost time or effort.
2. **Two strengths per rule.** Gentle friction for reflexive opens, plus a hard lock for worst moments.
3. **Strong, but never system-breaking.** Never break email, banking or the OS. Uninstall cleanly when
   unlocked. Be transparent. On Windows, trust decides whether people install at all.
4. **Block parts of a site,** not just whole domains.
5. **Sites, games and Discord under one schedule,** with nothing behind a paywall.
6. **Every break and exception expires on its own.**
7. **Design for drift.** Loosening is slower than tightening, and the app re-offers stricter settings
   later.
8. **Plan for whack-a-mole.** Offer an allowlist ("only these") mode and category lists.
9. **Free, with no stingy tiers.**
10. **A non-judgmental tone.** Users already feel guilty, and distraction is often coping.

---

## 2. Existing tools

Reviews come from Trustpilot, AlternativeTo, Mozilla add-on reviews, Hacker News, Reddit (through an
archive mirror) and vendor pricing and support pages. Pages written by a vendor that sells a rival product
are marked.

### 2.1 Comparison

| Tool | How it blocks | Apps? | Price | Biggest strength | Biggest complaint |
|---|---|---|---|---|---|
| **Cold Turkey** (Win/Mac) | Desktop app plus a required browser extension. It closes any browser whose extension is missing. "Frozen Turkey" locks the whole PC. [C2] | Yes (Pro) [C1] | Free tier; Pro is a one-time payment, about $35–45 per reviews (the official page showed no price) [C1][C2] | Seen as the hardest to bypass [C3][C5] | Extension detection fails and locks users out of browsers; hard to uninstall [C2] |
| **Freedom** | Cross-device sync. On Windows: a system proxy or a browser-only mode [C9] | Yes [C6] | Free tier (no schedules or Locked Mode; sessions under 2 h). $8.99/mo, $3.33/mo yearly, $99.50 lifetime [C6] | Sync across Windows, Mac and phones | Easy to bypass on Windows; billing complaints [C7][C8] |
| **FocusMe** (Win/Mac/Linux/mobile) | Desktop app with a forced mode | Yes | $17.95/mo, $119.40/yr, $449 lifetime [C12] | Fine-grained rules, strong enforcement, good support [C13] | Steep learning curve; CPU spikes (acknowledged by the vendor) [C14] |
| **LeechBlock NG** | Browser extension only; open source (MPL-2.0) [C15] | No | Free | Very configurable: 30 block sets, time allowances, wildcards [C15] | Easy to turn off; confusing setup [C16] |
| **BlockSite** | Extension plus mobile apps [C17] | Mobile only | Subscription (current price not verified) | Simple; categories; sync | Paywall pop-ups; named in a 2018 spyware report [C18] |
| **StayFocusd** | Chrome/Edge extension | No | Free | "Nuclear option", time budgets, allow-only mode [C19] | Bypassed by switching browsers. Now maintained by a data company and discloses web-history collection [C19] |
| **Microsoft Family Safety** | Child-account controls. Web filter works in Edge only; other browsers get killed [C20] | Time limits | Free | Built in | Not meant for self-control. All-or-nothing filtering. A December 2025 bug kept approved sites blocked [C20] |
| **Windows Focus / Clock** | Silences notifications | No | Free | Built in | Blocks nothing [C21] |
| **Forest** | Extension plus mobile; no Windows app [C22] | No | Free core, paid "Plus" | Gamified motivation | Extension bugs: blocks everything, broken allowlist [C22] |
| **Plucky** (Win/Mac/Linux/Android) | Browser plus system component. Loosening settings means waiting out a delay, not entering a password [C23] | Partly (cuts network, doesn't stop launch) [C23] | Not confirmed | **Delay-to-loosen design**; very hard to bypass | Steep learning curve; its own docs list antivirus incompatibilities [C23] |
| **RescueTime** | Desktop app plus extensions; blocks whatever is rated "distracting" | Yes | $9/mo or $84/yr [C26] | Tied to tracked usage | A time tracker first; sessions can be ended after 5 minutes [C26] |
| **Qustodio** (parental, for comparison) | System-level filter | Yes | $59.95–109.95/yr [C28] | Remote, multi-device | Reports of slowing Windows; support complaints [C28] |

Others checked:

- **Mac or phone only, or defunct.** Opal has no Windows app [C24]. Serene was Mac-only and its site now
  redirects away [C25].
- **New on Windows.** Raycast for Windows added timed app and site blocking in June 2026 [C30].
- **Open source.**
  - **There is no mature open-source system-level blocker for Windows.** The closest attempts are a
    3-star prototype with no releases and a discontinued project [C31].
  - SelfControl is Mac-only. LeechBlock works only inside the browser.

### 2.2 Gaps across the market

These gaps are supported by several independent sources:

1. **Tools are either too strict or too weak, with nothing in between.**
   - Strict tools lock people out of things they need: whole browsers [C2], or meditation apps added by
     mistake [C32]. A FocusMe user says full locks backfire because they sometimes need flexibility [C13].
   - Weak tools get bypassed: Freedom through Safe Mode or by killing its process [C7][C8], and every
     extension-only tool [C16][C19].
2. **Depending on a browser extension causes problems either way.** Either users switch browsers to dodge
   the block, or the tool kills browsers to stop that.
   - Cold Turkey's extension detection misfires on Opera and gets some Chrome users stuck in a reinstall
     loop [C2].
   - Family Safety kills every browser except Edge [C20].
   - Freedom's proxy breaks VPNs, Office and Store apps [C9][C10].
3. **Users don't trust anti-tamper software.** It integrates deeply into the OS and is hard to uninstall
   [C2]. People can't find a Windows blocker that "seems safe to download" [C32]. Privacy worries attach to
   BlockSite and StayFocusd [C18][C19].
4. **Subscription fatigue and freemium traps.** Session caps, paywall pop-ups and $449 lifetime prices get
   resented. Paying once gets praised [C3][C7][C12][C18].
5. **Weak page-level control outside extensions.** Users want YouTube minus the feed and Shorts, or only
   certain channels, and end up building it themselves [C4][C13][C32].
6. **Hard to set up.** FocusMe, LeechBlock and Plucky all draw learning-curve complaints [C13][C16][C23].

Seen only once (don't build around these alone):

- A blocker corrupting a local dev environment, or disabling a trackpad [C13].
- Firefox ignoring hosts-file blocks, probably because of DNS-over-HTTPS [C33].
- Antivirus false positives: we found **no** user reports for Cold Turkey, Freedom or FocusMe. Freedom's
  and Plucky's own docs warn that security software can interfere [C10][C23].

### 2.3 Opportunities

1. **Graduated friction instead of on/off.**
   - A wait before unlocking (Plucky's best-liked idea), a typed reason, a few emergency passes a week, and
     breaks that end on their own.
   - Evidence: [C4][C13][C23][C32].
2. **Fail safely, never by locking the user out.**
   - Never leave someone without a usable browser because an extension check failed.
   - Show what is blocked and why. Offer recovery on a delay, not an instant bypass [C2][C10][C20].
3. **One rule system for apps and websites,** including page-level rules and allowlist work modes. Block
   apps from launching, not just from reaching the network [C4][C8][C13][C23][C32].
4. **Anti-tamper that people can trust.**
   - A signed installer, a plain list of what gets installed, and a clean uninstall outside locks.
   - Local-only data with no telemetry.
   - Open source, which fills a real gap [C18][C19][C31][C32].
5. **Honest pricing.** Free and open source, with no caps [C3][C7][C12].
6. **A trusted person can unlock,** without the machinery of parental controls. Some people already hand
   the password to a spouse [C5][C20].
7. **Coexist with VPNs, proxies and Office.** That rules out a proxy-based design [C9][C10].

---

## 3. Mechanics: how blocking works on Windows

### 3.1 Website blocking layers

| Layer | Pros | Cons | Verdict |
|---|---|---|---|
| **Hosts file** | Simple; covers every app on the PC. | Domain-level only, with no paths or wildcards. Needs IPv4 and IPv6 entries. Browsers cache DNS and open connections, so changes need `ipconfig /flushdns` and sometimes a browser restart [T1]. Huge hosts files can stall the DNS Client [T2]. Defender can flag entries as `PossibleHostsFileHijack` and reset the file [T3]. | **Use** as the coarse, all-apps layer. Test against Defender early. |
| **Local DNS resolver** | Wildcards, logging, no file rewrites. | Set per network adapter. VPNs push their own DNS. DNS-over-HTTPS bypasses it [T4][T5]. Catching all DNS needs a kernel driver [T6]. | **Defer.** Only if hosts proves insufficient. |
| **Windows Filtering Platform** | Block network access per app path, per user or per IP from user mode, no driver [T7]. Filters can survive restarts [T8]. | IP blocks hit CDNs and shared hosts [T1]. Filters are left behind unless removed on uninstall [T9]. | **v2:** per-app network cut (e.g. Discord), DNS-over-TLS on port 853, known DNS-over-HTTPS endpoints. |
| **HTTPS proxy with TLS interception** | Sees full URLs. | Installing a root certificate weakens TLS for the whole PC [T10][T11]. Freedom moved away from its proxy because it broke Store apps, VPNs and locked-down PCs [T12]. | **Never.** |
| **Browser extension** | The only layer that sees paths. `declarativeNetRequest` allows 30,000 dynamic block/allow rules (5,000 redirects; 1,000 regex rules per ruleset) [T13]. | Per browser. YouTube is a single-page app, so moving from the feed to a video makes no new page request; it needs `webNavigation.onHistoryStateUpdated` plus content scripts [T13][T14]. | **Use** for page-level rules. |
| **Native messaging** | Connects the extension to the desktop app. The host is registered under `HKCU`/`HKLM\...\NativeMessagingHosts`, and the browser launches it over stdin/stdout [T15]. | Messages to the browser are capped at 1 MB. The host must be a small relay to the service. | **Use.** It also keeps the extension's background worker alive (Chrome 105+) [T16]. |
| **Browser policies (registry)** | `DnsOverHttpsMode=off`, InPrivate/Incognito off, `ExtensionInstallForcelist` so users can't remove the extension, `URLBlocklist` with paths (Edge, 1,000 entries) [T14][T17][T18][T19]. Firefox has equivalents [T20]. | The browser shows "Managed by your organization". On PCs not joined to a domain, force-install only works for store-listed extensions [T17]. Some Chrome policies are reportedly ignored on unmanaged PCs [T21] (not verified per policy). | **Use** during locks. Verify Chrome behaviour in M3. |

How others handle a missing extension: Cold Turkey checks for its extension every 10 seconds, blocks a
browser that lacks it after a 60-second grace period, and blocks browsers it doesn't support [T22].

### 3.2 App blocking

- **Detecting new processes.**
  - WMI's `Win32_ProcessStartTrace` needs admin and can miss bursts [T23].
  - ETW `Microsoft-Windows-Kernel-Process` events usually arrive 1–3 seconds late [T24].
  - User mode can only stop a process *after* it starts. Stopping it before it starts needs a kernel driver,
    which requires an EV certificate and Microsoft signing [T25][T26]. Plan: ETW, plus a one-second polling
    sweep as a backstop.
- **Matching.**
  - Match on full path, folder, Authenticode signer, hash and the PE `OriginalFilename`, so renaming or
    copying the exe doesn't work.
  - Cold Turkey also locks blocked exes so they can't be moved during a block, and supports folder rules
    (Steam, Discord), window-title rules and Store apps [T22].
- **Store (UWP) apps.** Their windows belong to `ApplicationFrameHost`. Resolve the real app's package
  identity from the process instead [T27].
- **One window instead of the whole app.** Watch the foreground window and its title. For Discord, a WFP
  filter on its network access is an alternative.
- **Avoid.**
  - Image File Execution Options `Debugger` matches by exe name only, and is a known malware persistence
    technique [T28].
  - The `DisallowRun` policy only covers Explorer launches [T29].
- **Optional "hard mode".** AppLocker and App Control for Business (formerly WDAC) now apply on all
  editions, including Home [T30][T31]. They are powerful, but one mistake can block system software.

### 3.3 Tamper resistance

- **Architecture.** A LocalSystem auto-start service with restart-on-failure. Exit with a non-zero code so
  the service manager actually restarts it [T32]. The unelevated tray UI talks to the service over a named
  pipe.
- **Hard limit.** Without a kernel driver, an administrator can always win.
  - Denying "stop" in the service's permissions only adds friction, and security tools flag it as defense
    evasion [T33][T34].
  - A truly protected service (PPL) needs an early-launch anti-malware driver and can't have a UI [T35].
    The realistic goal is to **stop impulsive bypasses**, and to recommend a standard (non-admin) daily
    account.
- **Clock changes.** Measure remaining time with `QueryUnbiasedInterruptTime`, which ignores the wall
  clock (but excludes sleep). Persist the *remaining* duration, not an end timestamp, and handle
  `WM_TIMECHANGE` [T36][T37].
- **Safe Mode.** A `SafeBoot` registry key would start the service there, but it's flagged as a
  persistence technique [T38]. Not worth it.
- **Other measures.**
  - Watch the hosts file and policy keys, and restore them if changed.
  - Fail closed after a reboot: reapply the block, don't drop it.
  - Disable the uninstaller during a lock (Cold Turkey does this [T22]).

### 3.4 Risks

- **Looking like malware.** Hosts edits get flagged. IFEO, service-permission denial and `SafeBoot` keys
  are textbook malware techniques. Killing processes can look suspicious too.
- **Privilege escalation.** The service's pipe is an attack surface. Lock it down with an explicit access
  list [T39].
- **Code signing.**
  - EV certificates no longer give instant SmartScreen reputation [T40].
  - Azure Trusted Signing (about $9.99/month) is open to individuals only in the US and Canada. OV
    certificates cost about $150–300/year [T41].
- **Breakage.**
  - Never block Microsoft or Windows Update domains.
  - Remove WFP filters on uninstall.
  - A buggy fail-closed path becomes a permanent block. SelfControl's own source warns about this [T42].
- **Multi-user PCs and accessibility.**
  - Hosts and policies affect every account; WFP can match on the user's SID.
  - Warn before closing an app, because of unsaved work.
  - Never block assistive technology.
- **Installer.** MSIX can install a service, but it runs no custom uninstall code, so it **can't refuse an
  uninstall during a lock** [T43][T44]. Use Inno Setup or a WiX MSI instead.

### 3.5 Toolchain check (MSYS2 UCRT64, g++ 16.2.0)

A throwaway program compiled with `-std=c++20 -Wall -Wextra`, linked against `fwpuclnt`, `tdh`, `advapi32`,
`wintrust` and `rpcrt4`, and ran successfully. Every header and import library below exists in
`C:/msys64/ucrt64`:

| API | Purpose | Result |
|---|---|---|
| `FwpmEngineOpen0` (WFP) | Per-app network blocking | Session opened |
| `ConvertStringSecurityDescriptorToSecurityDescriptorW` | Access list for the service's pipe | OK |
| `CreateToolhelp32Snapshot` | Polling sweep for app blocking | 314 processes listed |
| `StartTraceW`, `TdhGetEventInformation` (ETW) | Instant process-start events | Linked |
| `WinVerifyTrust` | Match apps by signer | `explorer.exe` verified as signed |

Also in the MSYS2 repos: `qt6-base` 6.11, `wxwidgets3.2-msw` 3.2, `nlohmann-json` 3.12, `catch` 3.16,
`gtest` 1.18, `sqlite3` 3.53.

---

## Sources

### People (U)

- [U1] https://reddit.sentinel-team.org/posts/1rfe006/snapshots/2026-02-27T00%3A31%3A19.028263Z
- [U2] https://reddit.sentinel-team.org/posts/1qxdmdt/snapshots/2026-02-06T11%3A30%3A25.904263Z
- [U3] https://reddit.sentinel-team.org/posts/1s0h4ma/snapshots/2026-03-27T19%3A11%3A28.198901Z
- [U4] https://reddit.sentinel-team.org/posts/1s9dmfe/snapshots/2026-04-03T16%3A51%3A00.928384Z
- [U5] https://reddit.sentinel-team.org/posts/1t5og62/snapshots/2026-05-08T19%3A50%3A12.016367Z
- [U6] https://reddit.sentinel-team.org/posts/1qaq7hq/snapshots/2026-01-12T13%3A31%3A29.404481Z
- [U7] https://reddit.sentinel-team.org/posts/1r1uodp/snapshots/2026-02-11T18%3A50%3A45.408579Z
- [U8] https://reddit.sentinel-team.org/posts/1to50pe/snapshots/2026-05-26T20%3A45%3A05.212958Z
- [U9] https://lengrand.fr/on-gaming-addiction-and-working-around-coldturkey
- [U10] https://www.trustpilot.com/review/getcoldturkey.com
- [U11] https://www.trustpilot.com/review/getcoldturkey.com?page=2
- [U12] https://www.trustpilot.com/review/freedom.to
- [U13] https://chromewebstore.google.com/detail/blocksite-block-websites/eiimnmioipafcokbfikbljfdeojpcgbh/reviews
- [U14] https://alternativeto.net/software/cold-turkey/about
- [U15] https://addons.mozilla.org/en-US/firefox/addon/leechblock-ng/reviews/?score=1
- [U16] https://news.ycombinator.com/item?id=45085014
- [U17] https://news.ycombinator.com/item?id=32484920
- [U18] https://news.ycombinator.com/item?id=20169348
- [U19] https://news.ycombinator.com/item?id=2122697
- [U20] https://news.ycombinator.com/item?id=45329540
- [U21] https://syntopikon.substack.com/p/i-cant-focus-without-blocking-more
- [U22] https://www.teamblind.com/post/procrastination-guilt-5gmjno1w
- [U23] https://www.teamblind.com/post/started-a-new-job-cant-focus-while-wfh-eqrue7su
- [U24] https://steamcommunity.com/app/730/discussions/0/506200114838133753
- [U25] https://steamcommunity.com/discussions/forum/12/1636418037478755541
- [U26] https://forum.endeavouros.com/t/any-foolproof-app-to-block-domains-such-as-youtube/41905
- [U27] https://www.digitalzen.app/blog/why-most-website-blockers-dont-work-for-adhd/ (vendor)
- [U28] https://arxiv.org/abs/2101.11743 (HabitLab, CHI 2021)
- [U29] https://arxiv.org/abs/2001.04180 (Lyngs et al., CHI 2020, N=58)
- [U30] https://www.pnas.org/doi/10.1073/pnas.2213114120 (PNAS 2023, N=280)
- [U31] https://arxiv.org/html/2401.16668v2 (InteractOut, CHI 2024, N=42)
- [U32] https://www.psychologicalscience.org/news/releases/internet-use-in-class-tied-to-lower-test-scores.html
- [U33] https://aisel.aisnet.org/jise/vol21/iss2/11/
- [U34] https://www.psypost.org/study-finds-cyberloafing-can-help-employees-cope-workplace-boredom
- [U36] https://support.microsoft.com/he-IL/Windows/Experience/focus-stay-on-task-without-distractions-in-windows
- [U37] https://support.microsoft.com/en-US/family-safety/family-safety-settings-enforced-on-adults

### Existing tools (C)

- [C1] https://getcoldturkey.com/pricing/
- [C2] https://nz.trustpilot.com/review/getcoldturkey.com?page=3
- [C3] https://www.trustpilot.com/review/getcoldturkey.com?page=4
- [C4] https://alternativeto.net/software/cold-turkey/about
- [C5] https://hn.algolia.com/api/v1/search?query=%22cold%20turkey%22%20blocker&tags=comment (comments 19507860, 34385050, 24680892, 49540394)
- [C6] https://freedom.to/premium
- [C7] https://www.trustpilot.com/review/freedom.to
- [C8] https://alternativeto.net/software/macfreedom/about/
- [C9] https://support.freedom.to/en/articles/4523542-non-proxy-mode-for-windows
- [C10] https://support.freedom.to/en/articles/4529917-freedom-troubleshooting-for-windows
- [C12] https://focusme.com/pricing/
- [C13] https://www.trustpilot.com/review/focusme.com?page=2
- [C14] https://www.trustpilot.com/review/focusme.com?page=5
- [C15] https://www.proginosko.com/leechblock/ and https://github.com/proginosko/LeechBlockNG
- [C16] https://addons.mozilla.org/en-US/firefox/addon/leechblock-ng/reviews/?score=2
- [C17] https://blocksite.co/ and https://www.techradar.com/reviews/blocksite
- [C18] https://adguard.com/en/blog/big-star-labs-spyware.html
- [C19] https://chromewebstore.google.com/detail/stayfocusd-%E2%80%93-website-bloc/laankejkbhbdhmipfmgcngdelahlfoji and https://alternativeto.net/software/stayfocusd/about
- [C20] https://textslashplain.com/2025/06/12/family-safety-content-filtering/ and https://learn.microsoft.com/en-us/answers/questions/5655270/family-safety-blocks-sites-ive-approved
- [C21] https://support.microsoft.com/windows/focus-stay-on-task-without-distractions-in-windows-cbcc9ddb-8164-43fa-8919-b9a2af072382
- [C22] https://www.forestapp.cc/ and https://addons.mozilla.org/en-US/firefox/addon/forest-stay-focused-be-present/reviews/?page=2
- [C23] https://docs.pluckeye.net/overview and https://docs.pluckeye.net/incompatibility
- [C24] https://opalapp.com/
- [C25] https://alternativeto.net/software/serene/about/
- [C26] https://www.rescuetime.com/pricing and https://help.rescuetime.com/article/374-about-focus-sessions
- [C28] https://www.qustodio.com/en/plans/ and https://www.security.org/parental-controls/qustodio/
- [C30] https://www.raycast.com/changelog/windows/0-64
- [C31] https://github.com/Relevant47/Focus-lock and https://alternativeto.net/software/selfrestraint/about
- [C32] https://reddit.sentinel-team.org/posts/1u8nh8k/snapshots/2026-06-19T20%3A51%3A09.757437Z, https://reddit.sentinel-team.org/posts/1qghuu7/snapshots/2026-01-20T00%3A51%3A13.680313Z, https://reddit.sentinel-team.org/posts/1qa0vyr/snapshots/2026-01-12T08%3A21%3A18.428819Z
- [C33] https://hn.algolia.com/api/v1/search?query=website%20blocker%20windows%20bypass&tags=comment (comment 25125285)

### Mechanics (T)

- [T1] https://github.com/SelfControlApp/selfcontrol/wiki/FAQ
- [T2] https://github.com/StevenBlack/hosts/issues/93
- [T3] https://support.microsoft.com/en-us/servicing/os/windows/2017/01/hosts-file-is-detected-as-malware-in-windows-defender
- [T4] https://mayakron.altervista.org/support/acrylic/Windows10Configuration.htm
- [T5] https://chromium.googlesource.com/chromium/src/+/main/net/docs/adding_doh_providers.md
- [T6] https://docs.safing.io/portmaster/architecture/os-integration
- [T7] https://learn.microsoft.com/en-us/windows/win32/fwp/permitting-and-blocking-applications-and-users
- [T8] https://learn.microsoft.com/en-us/windows/win32/api/fwpmtypes/ns-fwpmtypes-fwpm_filter0
- [T9] https://github.com/henrypp/simplewall
- [T10] https://www.ndss-symposium.org/wp-content/uploads/2017/09/killed-proxy-analyzing-client-end-tls-interception-software.pdf
- [T11] https://jhalderm.com/pub/papers/interception-ndss17.pdf
- [T12] https://support.freedom.to/en/articles/4523542-non-proxy-mode-for-windows
- [T13] https://developer.chrome.com/docs/extensions/reference/api/declarativeNetRequest
- [T14] https://learn.microsoft.com/en-us/deployedge/microsoft-edge-browser-policies/urlblocklist
- [T15] https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging
- [T16] https://developer.chrome.com/docs/extensions/develop/concepts/service-workers/lifecycle
- [T17] https://learn.microsoft.com/en-us/deployedge/microsoft-edge-browser-policies/extensioninstallforcelist
- [T18] https://learn.microsoft.com/en-us/deployedge/microsoft-edge-browser-policies/inprivatemodeavailability
- [T19] https://learn.microsoft.com/en-us/deployedge/microsoft-edge-browser-policies/dnsoverhttpsmode
- [T20] https://mozilla.github.io/policy-templates/
- [T21] https://groups.google.com/a/chromium.org/g/chromium-bugs/c/iTr-lgIIjiE
- [T22] https://getcoldturkey.com/support/user-guide/
- [T23] https://kiwix.lema.org/stackoverflow.com_en_all_2023-11/a/10144137
- [T24] https://learn.microsoft.com/zh-tw/archive/blogs/vancem/using-traceevent-to-mine-information-in-os-registered-etw-providers
- [T25] https://learn.microsoft.com/en-za/windows-hardware/drivers/ddi/ntddk/ns-ntddk-_ps_create_notify_info
- [T26] https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-attestation
- [T27] https://learn.microsoft.com/en-us/windows/msix/detect-package-identity
- [T28] https://attack.mitre.org/techniques/T1546/012
- [T29] https://learn.microsoft.com/sv-se/windows/client-management/mdm/policy-csp-admx-shellcommandpromptregedittools
- [T30] https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/feature-availability
- [T31] https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/deployment/deploy-appcontrol-policies-with-script
- [T32] https://learn.microsoft.com/en-us/dotnet/core/extensions/windows-service
- [T33] https://forum.bigfix.com/t/preventing-users-from-stopping-the-besclient/8384
- [T34] https://detection.fyi/sigmahq/sigma/windows/process_creation/proc_creation_win_sc_sdset_deny_service_access/
- [T35] https://learn.microsoft.com/en-us/windows/win32/services/protecting-anti-malware-services-
- [T36] https://learn.microsoft.com/windows/win32/sysinfo/interrupt-time
- [T37] https://learn.microsoft.com/en-us/windows/win32/sysinfo/wm-timechange
- [T38] https://detection.fyi/sigmahq/sigma/windows/registry/registry_set/registry_set_add_load_service_in_safe_mode/
- [T39] https://learn.microsoft.com/en-us/dotnet/api/system.io.pipes.namedpipeserverstreamacl.create
- [T40] https://learn.microsoft.com/en-us/answers/questions/1846647/program-requirements-microsoft-trusted-root-progra
- [T41] https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options
- [T42] https://github.com/QiaoXingLi/selfcontrol/blob/master/BlockManager.m
- [T43] https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-desktop6-extension
- [T44] https://techcommunity.microsoft.com/discussions/msix-discussions/msix-custom-uninstall-processing/916781
