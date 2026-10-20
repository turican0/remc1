# Building on Windows

REMC1 builds as a 64-bit program (x64; ARM64 with the ARM64 build tools). The build uses CMake (3.21+)
and vcpkg, which fetches and builds SDL2, SDL2_mixer, nlohmann-json and GoogleTest the first time.

## Requirements

- **Visual Studio 2022** (Community is fine) with the *Desktop development with C++* workload. It ships
  CMake and vcpkg. For ARM64 builds also add the *MSVC ARM64 build tools* component.
- **Python 3** for the tools (data extraction, replays, regression checks).
- **Git for Windows** (Git Bash) for the helper scripts. Optional: everything also works from a
  *Developer PowerShell for VS 2022*.

## Build

From Git Bash in the repository folder:
```bash
./build.sh                    # Debug   -> build/win-x64/Debug/remc1.exe
./build.sh win-x64-release    # Release -> build/win-x64/RelWithDebInfo/remc1.exe
```
`build.sh` uses the CMake and vcpkg bundled with Visual Studio (found with `vswhere`). If that fails,
set `VS_ROOT` to the Visual Studio folder, e.g.
`VS_ROOT="/c/Program Files/Microsoft Visual Studio/2022/Community" ./build.sh`.

From a *Developer PowerShell for VS 2022*:
```powershell
$env:VCPKG_ROOT = "$env:VSINSTALLDIR\VC\vcpkg"
cmake --preset win-x64
cmake --build --preset win-x64-release
```
Opening the folder in Visual Studio also works (it reads `CMakePresets.json`). The SDL2 DLLs and the
font are copied next to the executable.

Other presets:

| Preset | |
|--------|---|
| `win-x64-asan` | x64 Debug with AddressSanitizer (used by `tools/asan_run.sh`) |
| `win-arm64` (`win-arm64-debug`, `win-arm64-release`) | Windows on ARM; needs the MSVC ARM64 build tools |

## Tests

```bash
REMC1_DATA_DIR=<datadir> ctest --preset win-x64-debug     # or win-x64-release
```
`<datadir>` is the folder made by `tools/extract_data.py` (see the README). Without `REMC1_DATA_DIR` the
tests that need the game data (data hashes, replays) are skipped. Use the CTest bundled with Visual
Studio if the one on `PATH` is older than 3.21.

Recordings of whole levels can be replayed and compared frame by frame with `tools/regress.py`
(see [recordings.md](recordings.md)).
