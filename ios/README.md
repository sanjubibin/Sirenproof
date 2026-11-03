# Sirenproof for iOS

**Planned. Nothing here yet.** The Windows app comes first; see [the roadmap](../windows/ROADMAP.md)
("Other operating systems", after v1.0).

What's expected to carry over:

- The ideas of the rule engine in [`windows/core`](../windows/core) (rules, schedules,
  Nudge/Block/Lock), and possibly its C++ code.
- The same promises: tighten at once, loosen slowly, locks that hold, and nothing leaving the device.

What would be new: iOS only lets apps block other apps and sites through Apple's Screen Time APIs
(Family Controls, Managed Settings and Device Activity), so an iOS version would be built around those
rather than its own enforcement.
