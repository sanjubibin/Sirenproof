# Sirenproof for macOS

**Planned. Nothing here yet.** The Windows app comes first; see [the roadmap](../windows/ROADMAP.md)
("Other operating systems", after v1.0).

What's expected to carry over:

- The rule engine in [`windows/core`](../windows/core) (rules, schedules, Nudge/Block/Lock, the
  tamper-resistant clock). It's plain C++ with no Windows headers, so it can be shared once a second
  platform needs it.
- The same promises: tighten at once, loosen slowly, locks that hold, and nothing leaving the device.

What would be new: a background helper in place of the Windows service, blocking through macOS's own
mechanisms (for example a Network Extension content filter for sites, and watching for apps), and a
menu-bar app in place of the tray.
