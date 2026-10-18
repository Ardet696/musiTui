# musiTui for agents

musiTui is a terminal music player (MP3, FLAC, WAV). A running player can be controlled from the shell with `musiTui ctl`, which is how an agent should play, pause, skip, or configure it. Do not try to type into the TUI.

## Install

See [docs/INSTALL.md](docs/INSTALL.md). On Arch: `yay -S musitui`. Check with `musiTui --version`.

## Start the player

`ctl` needs a running player. Exit code 2 from any `ctl` command means none is running.

- The user already has `musiTui` open in a terminal: just use `ctl`.
- Nothing is running and you have no terminal to give the user: start it headless with `musiTui --daemon &`. It logs to stderr and stops on `musiTui ctl quit`, SIGINT or SIGTERM.

Only one instance owns the control socket at a time.

## First run

If `musiTui ctl albums` reports no albums, set the music root (each subdirectory is an album):

```bash
musiTui ctl root ~/Music
```

The root is saved, so this only has to be done once.

## Commands

```
musiTui ctl [--json] <command> [args...]
```

| Command | Effect |
|---|---|
| `status` | Current song, album, track, volume, playing state, theme, visual, root |
| `albums` | List albums with track counts (`loaded:false` means still scanning, retry) |
| `songs <album>` | List the tracks of an album |
| `play` | Resume playback |
| `play <album> [track]` | Play an album from track N (1-based, default 1) |
| `stop` / `pause` | Pause |
| `next` / `prev` | Skip forward / back |
| `volume [N\|+N\|-N]` | Show, set, or adjust volume (0-100) |
| `output [list\|N]` | List output devices or switch to device N |
| `theme [N\|name]` | Show or set the theme: Fire, BW, PurpleRain, Forest |
| `visual [N\|name]` | Show or set the visualizer: Spectrum, Oscilloscope, Mirrored, Rolling, Wave |
| `root [path]` | Show or set the music root directory |
| `help` | List commands |
| `quit` | Close the player |

Album names match exactly first, then case-insensitively. Quote names with spaces in the shell as usual: `musiTui ctl play "Abbey Road" 3`.

## Output and exit codes

Without `--json` the human-readable message goes to stdout on success and to stderr on failure. With `--json` a single line is printed:

```json
{"ok":true,"message":"Playing: Something (Abbey Road, track 2), volume 40%","data":{"playing":true,"album":"Abbey Road","track":2,"song":"Something","next":"Octopus's Garden","volume":40,"theme":"Fire","visual":"Spectrum","root":"/home/me/Music"}}
```

`data` is present only for commands that return structured results (`status`, `albums`, `songs`, `volume`, `output`, `theme`, `visual`, `root` queries).

| Exit code | Meaning |
|---|---|
| 0 | Command accepted |
| 1 | Command rejected (bad argument, unknown album, ...) or bad usage |
| 2 | No running player, or the socket is unreachable |

`play`, `next`, `prev`, `volume` and `output` are queued, so run `status` afterwards if you need to confirm the new state.

## Socket

Commands travel over a Unix socket that only the owning user can access: `$MUSITUI_SOCKET`, otherwise `$XDG_RUNTIME_DIR/musitui.sock`, otherwise `${TMPDIR:-/tmp}/musitui-<uid>/musitui.sock`. The protocol is one text line in (the same command syntax as above) and one JSON line out.

## Working on the code

Build and test with `make` and `make test` (see [tests/tests.md](tests/tests.md)). All commands go through `src/commands/CommandDispatcher`, which both the TUI command bar and the socket (`src/ipc/ControlServer`) use, so new commands only need to be added there.
