# Minimum requirements — Navalha 2 on Linux

## Internal Debian package

The `navalha2-deb-ubuntu-22-amd64` artifact is built on Ubuntu 22.04 for
**amd64 / x86_64** computers. It is intended for Ubuntu 22.04 or newer, Linux
Mint based on those releases, and compatible Debian/Ubuntu systems.

It is not an ARM, Fedora, Arch or openSUSE package. Use a platform-specific
future distribution or build from source on those systems.

## Computer

| Resource | Practical minimum | Recommended for musical work |
| --- | --- | --- |
| Processor | 2 cores, 2 GHz | modern 4-core processor |
| Memory | 4 GB RAM | 8 GB RAM or more |
| Storage | 300 MB for app/libraries, plus WAVs | SSD and several GB for takes/projects |
| Audio | ALSA, PipeWire or PulseAudio stereo output | USB interface with ALSA driver |
| Graphics session | X11 or Wayland with XWayland | current desktop session |

Long WAV files and large libraries use memory and storage in proportion to the
source material.

## Display

| Use | Resolution |
| --- | --- |
| `PERFORM` window | 900 × 560 minimum |
| Main interface | 1480 × 900 practical minimum |
| Comfortable use | 1920 × 1080 |
| Detached performance | optional second 1920 × 1080 monitor |

The second monitor is optional; it separates `PERFORM` from the main editor.

## Before installing

1. Run `uname -m`; the expected result is `x86_64`.
2. Download the `.deb` artifact from a successful **Actions** run.
3. Follow [English installation instructions](INSTALLATION_DEB_INTERNAL_EN.md).
