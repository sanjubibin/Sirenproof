# Security policy

Sirenproof will run a Windows service with SYSTEM privileges, so security bugs in it can affect the whole
PC. Reports are taken seriously.

## Reporting a vulnerability

**Please don't open a public issue.** Report privately through GitHub instead:

1. Open the repository's **Security** tab.
2. Choose **Report a vulnerability**.

Please include:

- What the problem is, and what an attacker could do with it.
- Steps to reproduce, or a proof of concept.
- The Sirenproof version or commit you tested.

You'll get an acknowledgement within a week. Fixes for confirmed issues are released as soon as
practical, and you'll be credited in the release notes unless you'd rather not be.

## Supported versions

Sirenproof has no releases yet. Once v1.0 is out, security fixes will go into the latest release.

## In scope

- Privilege escalation through the service, its named pipe or its files.
- Anything that lets a signed-in user loosen a rule faster than its delay allows, or end a lock other
  than by its unlock method. (Until v1.x adds separate rules per Windows user, rules belong to the whole
  PC: anyone signed in can see them and *tighten* them, and that's by design.)
- Injection into the hosts file or browser policies.
- Data leaving the machine. Sirenproof must never send anything anywhere, nor be made to connect to
  another computer.

The latest review of these, for the Windows app, is in
[windows/docs/THREAT-MODEL.md](windows/docs/THREAT-MODEL.md).

## Out of scope

Bypassing a block or lock *as an administrator of your own PC*. This is a known limit of any blocker
without a kernel driver, described in [the plan](windows/docs/PLAN.md#51-against-your-own-impulses-tamper-resistance).
Ideas that make bypassing harder without using malware techniques are welcome as regular issues.
