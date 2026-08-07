# Installation Guide

- [Pre-built binaries](#pre-built-binaries)
- [Arch Linux (AUR)](#arch-linux-aur)
- [Build from source (Linux)](#build-from-source-linux)
- [Build from source (macOS)](#build-from-source-macos)

---

## Pre-built binaries

Binaries for Linux (x86_64) and macOS (arm64) are on the [Releases](https://github.com/ardet696/musiTui/releases) page. The only runtime dependency is SDL2:

| Platform | Install SDL2 |
|----------|--------------|
| Arch | `sudo pacman -S sdl2` |
| Ubuntu / Debian | `sudo apt install libsdl2-2.0-0` |
| Fedora | `sudo dnf install SDL2` |
| macOS | `brew install sdl2` |

```bash
chmod +x mp3player-linux-x86_64
./mp3player-linux-x86_64
```

## Arch Linux (AUR)

```bash
yay -S minimalist-mp3-player
```

## Build from source (Linux)

Dependencies:

| Distro | Command |
|--------|---------|
| Arch | `sudo pacman -S sdl2 cmake gcc` |
| Ubuntu / Debian | `sudo apt install libsdl2-dev cmake g++` |
| Fedora | `sudo dnf install SDL2-devel cmake gcc-c++` |

```bash
git clone https://github.com/ardet696/musiTui.git
cd musiTui

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

./build/MP3Player
```

> Note: avoid `sudo cmake --install build` if you plan to use the AUR package later. Both install to different paths and the manual install takes priority.

## Build from source (macOS)

Apple Clang supports `std::jthread` (with `-std=c++20`) as of Apple Clang 21, so the system toolchain is enough. Check your version:

```bash
clang++ --version
```

### Apple Clang 21 or newer (macOS 26.2+)

SDL2 is downloaded and built automatically.

```bash
brew install cmake

./scripts/build-ftxui.sh

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

./build/MP3Player
```

### Older Apple Clang

Update the Command Line Tools (System Settings > General > Software Update, or `xcode-select --install` / `softwareupdate --list`), then re-check `clang++ --version`.

If you cannot upgrade, fall back to GCC via Homebrew:

```bash
brew install gcc cmake

./scripts/build-ftxui.sh

ls /opt/homebrew/bin/g++-*

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-14
cmake --build build -j$(sysctl -n hw.ncpu)

./build/MP3Player
```
