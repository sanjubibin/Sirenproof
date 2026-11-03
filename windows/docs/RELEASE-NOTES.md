Sirenproof blocks the websites and apps you choose, on your own rules, on Windows 10 and 11. It's free,
open source (GPL-3.0-or-later), and nothing it knows ever leaves your PC.

## Install

1. Download `Sirenproof-<version>-setup.exe` below and check it against `SHA256SUMS.txt`:

   ```powershell
   Get-FileHash .\Sirenproof-<version>-setup.exe -Algorithm SHA256
   ```

2. Run it. It needs administrator rights once, to install the background service.
3. For blocking single pages (like YouTube Shorts) in Chrome or Edge, add the browser extension. Until
   it's in the stores, the [user guide](https://github.com/sanjubibin/Sirenproof/blob/main/windows/docs/USER-GUIDE.md#the-browser-extension)
   says how to load it from `C:\Program Files\Sirenproof\extension`.

## What's new

See the [changelog](https://github.com/sanjubibin/Sirenproof/blob/main/windows/CHANGELOG.md).

## Good to know

- A lock can't be undone while it holds, except by the unlock method you chose when you started it.
  That includes uninstalling: the installer refuses until the lock ends.
- If Windows warns that the publisher is unknown, the build isn't signed yet; check the SHA-256 above.
