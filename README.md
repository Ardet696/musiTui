# musiTui - Minimalist Music Player

#### Designed for great performance and low memory/cpu usage with C++20.

![Default TUI](images/MP3Player.png)

---

## Install

Quickest path (Arch):

```bash
yay -S minimalist-mp3-player
```

Otherwise grab a binary from [Releases](https://github.com/ardet696/musiTui/releases) or build from source. Full steps for every platform: **[Installation Guide](docs/INSTALL.md)**.

## First launch

The player is built around the concept of an "Album Player" or "Playlist Player". It scans your music root for directories containing MP3 files, and each subdirectory is treated as an album or playlist.

The file manager is empty until you set that root:

1. Type `RootConfig` in the command bar, press Enter.
2. Type the full path to your music directory (e.g. `/home/user/Music`), press Enter.

Left side shows detected albums, right side shows the contents of the selected one. Non-MP3 files (images, etc.) are ignored.

![RootExample](images/RootAlbums.png)

## Commands

The command panel works like a mini config terminal.

- Direct: `play`, `stop`, `next`, `prev`, `help`, `fileHelp`
- Interactive: `volume`, `output`, `visuals`, `themes`, `RootConfig` wait for a selection. Press `Esc` to cancel and return to normal input.

## Build and run

```bash
make run     # build release and launch
make test    # build and run the test suites
```

See the [Installation Guide](docs/INSTALL.md) for dependencies and platform notes.
