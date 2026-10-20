# Building on macOS

REMC1 builds with Apple Clang on macOS (Apple Silicon and Intel) against Homebrew packages: CMake
(3.21+), Ninja, SDL2, SDL2_mixer, nlohmann-json and GoogleTest.

## Install the dependencies

1. The Xcode command line tools (compiler, git):
   ```bash
   xcode-select --install
   ```
2. [Homebrew](https://brew.sh), then:
   ```bash
   brew install cmake ninja pkg-config python sdl2 sdl2_mixer nlohmann-json googletest
   ```

## Build

```bash
git clone <repository url> remc1 && cd remc1
cmake --preset macos
cmake --build --preset macos-release    # -> build/macos/RelWithDebInfo/remc1
cmake --build --preset macos-debug      # -> build/macos/Debug/remc1
```
The font folder is copied next to the executable. CMake finds the Homebrew packages through `brew`'s
prefix on `PATH` (`/opt/homebrew` on Apple Silicon, `/usr/local` on Intel); if it does not, add
`-DCMAKE_PREFIX_PATH="$(brew --prefix)"` to the first command.

## Tests

```bash
REMC1_DATA_DIR=<datadir> ctest --preset macos-release     # or macos-debug
```
`<datadir>` is the folder made by `tools/extract_data.py` (see the README). Without `REMC1_DATA_DIR` the
tests that need the game data (data hashes, replays) are skipped.

Recordings of whole levels can be replayed and compared frame by frame with `tools/regress.py`
(see [recordings.md](recordings.md)).

## Notes

- File names are matched case-insensitively, as on DOS, so the data can keep its upper-case names
  even on a case-sensitive volume.
- The differential harness in `tools/` (`diff_tick.py` and friends) needs Windows: it reads the
  Visual Studio debug information of the Windows build.
