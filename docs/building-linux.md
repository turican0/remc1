# Building on Linux

REMC1 builds with GCC or Clang on 64-bit Linux (x86-64 and arm64) against the distribution's own
packages: CMake (3.21+), Ninja, SDL2, SDL2_mixer, nlohmann-json and GoogleTest.

## Install the dependencies

**Ubuntu** (22.04 or newer) and **Debian** (12 or newer):
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config git python3 \
    libsdl2-dev libsdl2-mixer-dev nlohmann-json3-dev libgtest-dev
```

**Fedora**:
```bash
sudo dnf install gcc-c++ cmake ninja-build pkgconf-pkg-config git python3 \
    SDL2-devel SDL2_mixer-devel json-devel gtest-devel
```

**Arch Linux**:
```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf git python \
    sdl2 sdl2_mixer nlohmann-json gtest
```
(On current Fedora and Arch, SDL2 is provided by `sdl2-compat` on top of SDL3; that works too.)

Clang instead of GCC: install `clang` and configure with `CC=clang CXX=clang++ cmake --preset linux`.

## Build

```bash
git clone <repository url> remc1 && cd remc1
cmake --preset linux
cmake --build --preset linux-release    # -> build/linux/RelWithDebInfo/remc1
cmake --build --preset linux-debug      # -> build/linux/Debug/remc1
```
The font folder is copied next to the executable.

## Tests

```bash
REMC1_DATA_DIR=<datadir> ctest --preset linux-release     # or linux-debug
```
`<datadir>` is the folder made by `tools/extract_data.py` (see the README). Without `REMC1_DATA_DIR` the
tests that need the game data (data hashes, replays) are skipped.

Recordings of whole levels can be replayed and compared frame by frame with `tools/regress.py`
(see [recordings.md](recordings.md)).

## Notes

- `<datadir>/CD` may be read-only (e.g. installed system-wide). `<datadir>/CARPET.CD` holds the saves
  and must be writable; otherwise pass `--c-drive <dir>`, a writable folder that stands in for the DOS
  `C:` drive (it gets its own `CARPET.CD`).
- File names are matched case-insensitively, as on DOS, so the data can keep its upper-case names.
- The differential harness in `tools/` (`diff_tick.py` and friends) needs Windows: it reads the
  Visual Studio debug information of the Windows build.
