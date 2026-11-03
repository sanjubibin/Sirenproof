# Sirenproof for Android

**Planned. Nothing here yet.** The Windows app comes first; see [the roadmap](../windows/ROADMAP.md)
("Other operating systems", after v1.0).

What's expected to carry over:

- The rule engine in [`windows/core`](../windows/core) (rules, schedules, Nudge/Block/Lock, the
  tamper-resistant clock). It's plain C++ with no Windows headers, so it could be built for Android
  with the NDK once a second platform needs it.
- The same promises: tighten at once, loosen slowly, locks that hold, and nothing leaving the device.

What would be new: blocking through Android's own mechanisms (for example a local VPN service for sites,
and usage access to see which app is in front), and an app with a home-screen widget in place of the
tray.
