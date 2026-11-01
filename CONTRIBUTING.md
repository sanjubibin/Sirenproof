# Contributing to Sirenproof

Thanks for your interest! Sirenproof is early in development. Read [docs/PLAN.md](docs/PLAN.md) before
starting anything big, and open an issue first so we can agree on the approach.

## Ground rules

These come from the plan's principles and aren't up for debate in individual pull requests:

- **Local only.** No network code in the service, no telemetry, no accounts.
- **No techniques malware uses.** No kernel drivers, TLS interception, Image File Execution Options
  hijacks, `SafeBoot` registry keys or tricks with service permissions.
- **The service can only add blocking.** No client request may make the service run a program, write an
  arbitrary IP address, redirect a domain, or touch protected processes.
- **Never system-breaking.** Windows components, assistive technology and Windows Update are never
  blocked.
- **`core/` stays free of Windows headers,** so all the logic can be unit-tested.

## Building and testing

See [Building from source](README.md#building-from-source). Before sending a pull request, check that
both of these pass without warnings:

```bash
C:/msys64/ucrt64/bin/cmake.exe --build --preset debug
C:/msys64/ucrt64/bin/ctest.exe --preset debug
```

CI builds debug and release with warnings treated as errors (`ci-debug`, `ci-release` presets).

## Developing safely

Code that changes the system (hosts file, browser policies, services, closing processes) can lock you
out of your own PC if it has a bug:

- **Test real enforcement in a virtual machine,** not on your daily PC. On Windows Home, which has no
  Hyper-V or Windows Sandbox, use VirtualBox.
- **Debug builds will run in dry-run mode by default,** logging what they would block and changing
  nothing (from M2).

## Code style

- C++23. Formatting follows [.clang-format](.clang-format), and [.editorconfig](.editorconfig) covers the
  rest.
- Prefer small, pure functions in `core/` with tests next to every behaviour change.
- Comments explain *why*, not *what*.

## Commit messages

- Write a short imperative subject line (`Add domain matching`, not `Added…`), under about 70
  characters.
- Add a body when the reason for the change isn't obvious.
- Keep one logical change per commit.

## License

By contributing, you agree that your contributions are licensed under the
[GPL-3.0-or-later](LICENSE).
