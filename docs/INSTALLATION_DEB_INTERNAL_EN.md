# Internal installation — Navalha 2 on Linux

Validated package: `navalha2_0.1.0_x86_64.deb`.

See [minimum Linux requirements](MINIMUM_REQUIREMENTS_LINUX_EN.md) before
installation.

## Before installing

- use a 64-bit `amd64` / `x86_64` Linux system;
- copy the `.deb` file to a local folder, such as `Downloads`;
- close any running Navalha 2 window.

The broad internal package is produced by the GitHub workflow **Package Debian
(Ubuntu 22.04)**. It is suitable for Ubuntu 22.04 or newer and compatible
Debian/Ubuntu-based systems. If `apt` reports an unavailable dependency, do not
force installation; report the distribution and version instead.

## Install

Open a terminal in the folder containing the package and run:

```bash
sudo apt install ./navalha2_0.1.0_x86_64.deb
```

Then open **Navalha 2** from the application menu or run:

```bash
"Navalha 2"
```

## Two-minute check

1. Confirm the name and icon appear in the application menu.
2. Open the app and select an output device under `AUDIO`.
3. Load a WAV into `SOURCE A`, press `PLAY`, then `STOP`.
4. Close and reopen the app once.

If the app does not open, run `"Navalha 2"` in a terminal and send the output.

## Remove

```bash
sudo apt remove navalha2
```

This removes the app, not the user's projects, takes or audio files.
