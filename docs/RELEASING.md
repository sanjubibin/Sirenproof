# Releasing Sirenproof

How a version goes from `main` to a download on GitHub Releases. The plan's bar for v1.0 (M5): a
**signed** installer on GitHub Releases, and install → use → uninstall passing on a fresh virtual machine.

## Checklist

1. Every check in the [test tracker](TEST-TRACKER.md) for this version is ✅, or has a note saying why
   it can wait. Anything ❌ is fixed first.
2. Set the version in the top-level `CMakeLists.txt` (`project(Sirenproof VERSION x.y.z)`), and the
   same in `extension/manifest.json`. The programs, the installer and the file names take it from
   CMake; the packaging script checks the two agree.
3. In `CHANGELOG.md`, move what's under **Unreleased** into a new `## [x.y.z] - YYYY-MM-DD` section.
4. Update `docs/RELEASE-NOTES.md` for this version (it becomes the GitHub Release's text).
5. Commit, then tag and push:

   ```bash
   git tag v0.1.0
   git push origin v0.1.0
   ```

6. The **Release** workflow builds, tests and packages, then opens a **draft** release with
   `Sirenproof-x.y.z-setup.exe`, `Sirenproof-extension-x.y.z.zip` and `SHA256SUMS.txt`.
7. Download the draft's installer, check its checksum (below), and run section 3 of the test tracker
   on it in the VM.
8. Publish the draft.

To make the same files on your own PC instead (for example to test the installer before tagging):

```powershell
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

They land in `build\package`. Inno Setup 6.3 or later has to be installed for the installer.

## Checksums

`SHA256SUMS.txt` lists a SHA-256 for each file. To check a download:

```powershell
Get-FileHash .\Sirenproof-0.1.0-setup.exe -Algorithm SHA256
```

The hash must match the line for that file. The user guide tells people the same.

## Code signing

Unsigned, the installer gets a SmartScreen "Windows protected your PC" warning, and the service runs
as SYSTEM from programs nobody vouched for. v1.0 is signed.

**Getting a certificate.** Options, as of November 2026 (plan §10):

- **SignPath Foundation** signs open-source projects for free, from CI, with the key kept in their
  hardware. Apply at [signpath.org](https://signpath.org) with the GitHub repository; they check the
  project's license (GPL-3.0 qualifies) and that releases are built in CI from public sources, which the
  Release workflow already does.
- **Azure Trusted Signing** only takes individuals in the US and Canada.
- **A commercial OV certificate** on a hardware token, from a certificate authority.

**Signing locally**, with a certificate in your Windows certificate store and the Windows SDK installed:

```powershell
powershell -ExecutionPolicy Bypass -File tools\package.ps1 -Sign -CertThumbprint <thumbprint>
```

That signs the five Sirenproof programs, then the installer and its uninstaller, with SHA-256 and a
timestamp (`-TimestampUrl` to change the server). Qt's and MSYS2's DLLs are left as their makers built
them. Set `SIRENPROOF_CERT_THUMBPRINT` to skip typing the thumbprint.

**Signing in CI** with SignPath: once the project is accepted, add their GitHub Action to
`.github/workflows/release.yml` between *Package* and *Draft the GitHub Release*. It uploads
`build/package/stage` (the programs) and then the installer, and gets them back signed. SignPath gives
the exact steps and the secrets to add (`SIGNPATH_API_TOKEN` and the organization and project ids).

**Checking a signature:**

```powershell
Get-AuthenticodeSignature 'C:\Program Files\Sirenproof\sirenproof-service.exe' | Format-List
```

`Status` should be `Valid`, with the expected signer.

## The browser extension

The release includes `Sirenproof-extension-x.y.z.zip`, ready to upload:

- **Chrome Web Store:** a one-time $5 developer registration, then upload the zip on the
  [developer dashboard](https://chrome.google.com/webstore/devconsole). The listing text, privacy
  answers and screenshots to use are in [STORE-LISTING.md](STORE-LISTING.md).
- **Edge Add-ons:** free, on [Partner Center](https://partner.microsoft.com/dashboard/microsoftedge).
  The same zip and text.

The zip leaves out the manifest's `key`, which the stores don't accept; it only pins the id
(`cgpgilkcgaojdaclgkllhndfgdphgdje`) for loading the extension unpacked. Each store gives the extension
its own id. After the first listing in each store, add that id to `kExtensionIds` in
`core/include/sirenproof/core/browser.hpp` (the relay only talks to the ids listed there), and to the
test in `tests/core/browser_tests.cpp`, then make a new release. Until then, the store builds can't
reach the service.

The version in `extension/manifest.json` has to match `CMakeLists.txt`; `tools\package.ps1` stops if it
doesn't. Each store upload needs a higher version than the last.
