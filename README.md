# REMC1 - Magic Carpet 1 port

A port of Bullfrog's **Magic Carpet** (1994, the CD version as sold in GOG's *Magic Carpet Plus*) to
modern systems, built on [turican0's REMC1](https://github.com/turican0/REMC1) decompilation of the
DOS executable. The original game code runs natively; DOS services (VGA, keyboard/mouse, timer,
sound) are replaced by an SDL2 platform layer.

You need your own copy of the game - no game data is included.

## Features

- SDL2 window: windowed or fullscreen, resizable, 4:3 aspect, 320x200 and 640x480 game modes
- keyboard and mouse, sound effects (SDL2_mixer)
- settings in `config.json` and on the command line (`remc1 --help`)
- input recording and deterministic replay, used by the test suite and for bug reports
  ([docs/recordings.md](docs/recordings.md))
- tests: unit, game-data and replay tests (GoogleTest / CTest)

## Getting started

1. **Build** - REMC1 is a 64-bit program (x64 and arm64):

   | Platform | Guide |
   |----------|-------|
   | Windows (Visual Studio 2022) | [docs/building-windows.md](docs/building-windows.md) |
   | Linux (Ubuntu, Debian, Fedora, Arch) | [docs/building-linux.md](docs/building-linux.md) |
   | macOS (Homebrew) | [docs/building-macos.md](docs/building-macos.md) |

2. **Extract the game data** from a GOG install (Python 3, no other dependencies):
   ```bash
   python tools/extract_data.py --gog "<GOG Magic Carpet Plus folder>" --out <datadir>
   ```
   This creates `<datadir>/CD` (read-only CD contents) and `<datadir>/CARPET.CD` (the writable
   install folder with saves).
3. **Run**
   ```bash
   remc1 --data <datadir>
   ```
   (the executable is in `build/<preset>/<config>/`, e.g. `build/win-x64/RelWithDebInfo/remc1.exe` or
   `build/linux/RelWithDebInfo/remc1`), or set `paths.data` in `config.json` (looked up next to the
   executable).

Port controls that differ from the original (all configurable in `config.json`, section `input`):
W/A/S/D act as the arrow keys, Tab acts as Enter (opens the spell map), and pressing both mouse
buttons does *not* open the spell map (`bothButtonsSpellMenu`, off by default). `R` switches between
320x200 and 640x480 as in the original.

## Status

The game is playable on Windows. It has been tested **from the start through level 13**; later levels
have not been played through yet. Recordings of levels 1-11 replay identically, frame by frame (whole
game state and every image), on Windows x64 and on Linux with GCC and Clang (`tools/regress.py`);
interactive play on Linux has had little testing so far. Fixes are verified against the original executable with a differential harness
in `tools/` (the original machine code is run under Unicorn on a memory dump of the port and the
results compared; Windows only).

Bugs found in the original game itself, and how the port handles them, are listed in
[docs/original-bugs.md](docs/original-bugs.md).

### Remaining work

- **Testing:** levels 14 onwards; multiplayer/network play is untested.
- **Known bugs:**
  - 640x480: the sprite blitter draws occasional horizontal streaks the original does not;
  - the main-menu attract demo (after about 2 minutes idle) is unverified and may crash.
- **Music** is not implemented yet (sound effects are).
- **Platforms:** Windows on ARM builds but has not been run yet; macOS is untested; no CI yet.
- **Code structure:** the game code is still one large decompiled file (`sub_main.cpp`); splitting it
  into modules and naming the remaining `sub_*` routines is ongoing.
- An OpenGL renderer (the presenter interface is in place, only SDL2 exists today).

## Repository layout

| Path | Contents |
|------|----------|
| `sub_main.cpp`, `defs.h`, `engine/` | decompiled game code and its data structures |
| `portability/` | SDL2 platform layer: video, input, timer, sound, settings, record/replay |
| `tests/` | GoogleTest suite, replay recordings and golden hashes |
| `tools/` | data extraction, replay and debugging helpers, differential harness |

## Credits and license

Decompilation by [turican0](https://github.com/turican0/REMC1) (REMC1); the platform layer follows
[Magic Carpet 2 HD](https://github.com/thobbsinteractive/magic-carpet-2-hd) (remc2). Magic Carpet is
© 1994 Bullfrog Productions / Electronic Arts. This project is licensed under the GPL v3 (see
`LICENSE`).
