# REMC1 Developer Guide

Entry point for anyone (human or agent) picking up work on the Magic Carpet 1 port.
**Keep this file current**: when you learn a pattern, hit a gotcha or finish a milestone, update it.

**Never include identifying information** in this file or in anything else committed to the repo: no
names, user names, e-mail addresses or machine names, and no real paths or files from anyone's
machine. Use placeholders (`<data>`, `<repo>`, `<GOG Magic Carpet Plus dir>`) or the tools' defaults
(`C:/mc1data`) instead. Game data, recording and save file names are fine; where they live is not.

---

## 1. What this project is

A port of Bullfrog's *Magic Carpet* (1994/95, DOS, "Magic Carpet Plus" GOG release) to modern
Windows / Linux / macOS, built from a Hex-Rays decompilation of `CARPET.EXE` (originally by
turican0, upstream `turican0/REMC1`). The architecture follows the sister project
**magic-carpet-2-hd** (`remc2`), which did the same for Magic Carpet 2 — use it as a reference
(local checkout next to this repo: `../magic-carpet-2-hd`).

Approach: keep the decompiled game logic as-is (bug-for-bug), and replace DOS hardware/OS access
(VGA, keyboard ISR, INT 33h mouse, PIT timer, HMI sound drivers, DOS file APIs) with a portable
platform layer on SDL2.

### Status / roadmap

| Phase | Content | State |
|------|---------|-------|
| 0 | data extraction script, repo hygiene, this guide | done |
| 1 | CMake + vcpkg Windows build; bring-up to a playable level | done — levels 1-22 played (user recordings up to level 22); see §9 |
| 2 | platform layer cleanup: presenter seam (SDL2 now, OpenGL later), input, timing, headless | mostly (settings, timer, exact record/replay, `IPresenter`, watchdog); input cleanup + frame pacing left |
| 3 | split `sub_main.cpp` into `engine/*` modules (gated on golden tests) | todo |
| 4 | sound effects + music (SDL2_mixer; MIDI via libADLMIDI) | effects done (§6.4); AdLib, General MIDI (FluidSynth / Windows MIDI) and MT-32 (Munt) music done (§6.4b) |
| 5 | 64-bit conversion, Linux + macOS builds, CI | mostly — 64-bit done (x64, arm64; §7, merged as PR #2); Linux GCC/Clang builds and replays exactly; macOS does not build yet (§2); no CI |

Open items: §9 "Known open issues" and the README's "Remaining work".

**Port settings / controls:** gameplay controls the port changes go into `config.json`
(`portability/Settings.*`, section `input`): `wasdAsArrows`, `tabAsEnter`, `bothButtonsSpellMenu`
(left+right mouse opens the spell menu as in the original; default off). TODO: an in-game options page
for these, and once it exists default W/A/S/D = arrows and Tab = Enter (testing conveniences, on today)
back to the original controls.

**Audit against CARPET.EXE (2026-10-17):** §4 "Comparing with the original executable" is the main
verification tool now. Entity tick: 130/130 sweep snapshots (21 recordings) identical; `DrawWorld` and
whole-frame snapshots identical in 320x200 except level-start frames that load files; per-frame calls
(input `17C20`, `590D0`, `3C9D0`, `415C0`, `5A560`) identical. Bug patterns found: §6.2 #48-62.
Open: 640x480 sprite blitter streaks (§4, "Open finding"); minimap `v74` (C4701, unverified);
level-start frames (file loading) are not comparable.

---

## 2. Building

Requirements: CMake ≥ 3.21, a C++17 compiler, vcpkg (Windows/macOS) or system packages (Linux).

### Windows (current primary platform)
Visual Studio 2022 ships both a recent CMake and vcpkg; the CMake on `PATH` may be too old.

```bash
./build.sh                 # Git Bash: configures (first time) + builds preset win-x64-debug
./build.sh win-x64-release
```
`build.sh` sets `VCPKG_ROOT` to the VS-bundled vcpkg and uses the VS-bundled cmake. Equivalent manual
commands (Developer PowerShell):
```powershell
$env:VCPKG_ROOT = "<VS>\VC\vcpkg"
cmake --preset win-x64
cmake --build --preset win-x64-debug
```
Output: `build/win-x64/Debug/remc1.exe` (+ SDL2 DLL, `font/`). Opening the folder in Visual Studio
also works (it reads `CMakePresets.json`).

**64-bit only** (x64, arm64; CMake refuses 32-bit targets). See §7 for how the decompiled code's
32-bit addresses work. Linux/macOS: `docs/building-linux.md` / `docs/building-macos.md` (presets `linux`,
`macos`, Ninja Multi-Config; build presets `<os>-debug` / `<os>-release`).

Dependencies come from `vcpkg.json` (manifest mode; baseline pinned to the same commit as remc2).

### Linux / macOS
Linux builds with GCC or Clang (`docs/building-linux.md`, preset `linux`); verified in Docker on Ubuntu,
Debian, Fedora and Arch (§9, 64-bit port).
macOS: `docs/building-macos.md` describes the intended build (preset `macos`), but it does not build yet.

Known macOS build blockers (arm64, Apple clang; tried 2026-10-24, changes backed out for later):
1. **Compile:** `utilities/Compare.cpp` calls `_fseeki64` under `#ifdef __linux__ ... #else`, so macOS
   takes the MSVC-only branch. Fix: `#ifdef _WIN32` → `_fseeki64`, else `fseeko` (with `<sys/types.h>`).
2. **Link:** `ld: pointer not aligned in '_str_254A34'+0xBA`. Apple's arm64 linker rejects real pointers at
   unaligned addresses in initialised data. The port's handler and path tables are declared inside
   `#pragma pack (1)` regions, so their 8-byte pointers sit at odd offsets. Affected types include
   `Type_254A34`, `Type_254A34x`, `Type_96902`, `Type_97D12` (`sub_main.cpp`), `Type_99974`
   (`sub_main.h`) and `Pathstruct` (`engine/Basic.h`). The Windows exe has 544 unaligned DIR64
   relocations, and each one would be rejected the same way; they have not been mapped to symbols yet.
   - Idea: give those structs natural alignment on macOS only (`_Pragma("pack(push, 8)")` / `pack(pop)`
     around them).
   - Don't change the layout everywhere without fixing `tools/port_symbols.py` `project32`. It converts
     these globals to the original 32-bit layout and assumes they have no padding.

---

## 3. Game data

The game data is copyrighted and never committed. Extract it from a GOG install:

```bash
python tools/extract_data.py --gog "<GOG Magic Carpet Plus dir>" --out C:/mc1data
```

Produces:
```
<data>/CD/          contents of the CD's CARPET\ folder (read-only): DATA, LEVELS, INTRO, MOVIE, ...
<data>/CARPET.CD/   the writable DOS "C:\CARPET.CD" install folder: SAVE, LEVELS, DATA\TMAPS*, SNDSETUP.*, language.inf
<data>/manifest.sha256
```
- GOG's `CARPET.CD\game.gog` is a plain ISO9660 image (2048-byte sectors); the script has a tiny
  stdlib ISO reader, no 7-Zip needed.
- Level files in `CARPET.CD/LEVELS` are taken from the **CD**, not the GOG folder (GOG installs are
  sometimes modified by level editors).
- `tools/data_manifest.sha256` holds the reference hashes (hashes only, no content); extraction
  verifies against it. Regenerate with `--write-reference` only when deliberately changing the reference.

Running: `remc1 --data <data>`. Lookup order for the data folder: `--data` → `REMC1_DATA_DIR` env
→ `paths.data` in `config.json` → `<exe>/data` → user dir (`%APPDATA%/remc1/data`,
`~/.local/share/remc1/data`, `~/Library/Application Support/remc1/data`).

How the game sees it (`sub_main.cpp`, `FixPath`): `gameDataPath` = `<data>/CD` (the DOS `D:\CARPET`),
`gameDataPath2` = `<data>` (the DOS drive `C:`). Any path starting with `c:\carpet.cd` / `c:/CARPET.CD`
(any case/separator) is redirected to `gameDataPath2`; everything else is relative to `gameDataPath`.
Opening is case-insensitive on Linux/macOS (`portability/fcaseopen.cpp`, which also converts `\`).

---

## 4. Running, automation and debugging tools

### Python tools: use the repo's virtual environment
The tools in `tools/` need Python 3.9+. The everyday ones (`extract_data`, `rec`, `regress`, `replay_test`,
`run_recording`) use only the standard library. The harness, `exe_data.py disasm` and `contact_sheet`
need unicorn, capstone and Pillow, pinned in `tools/requirements.txt`. Install them into a virtual
environment in the repo root (`.venv/`, git-ignored), never into the system Python:
```bash
py -3.12 -m venv .venv                                    # Windows (any 3.9+; py -0 lists them)
.venv/Scripts/python -m pip install -r tools/requirements.txt
python3 -m venv .venv                                     # Linux / macOS
.venv/bin/python -m pip install -r tools/requirements.txt
```
Then activate it (`source .venv/Scripts/activate` in Git Bash, `.venv\Scripts\Activate.ps1` in
PowerShell, `source .venv/bin/activate` elsewhere), so `python tools/...` below uses it. Alternatively,
call `.venv/Scripts/python` directly.
- The ctest replay tests (tests/CMakeLists.txt) and `tools/debug_run.sh` pick `.venv` up on their own.
- A Python reinstall can leave CMake with a cached interpreter that no longer exists; with `.venv`
  present, the next configure replaces it.
- Recreate the venv after reinstalling its base Python: delete `.venv` and run the two commands again.

`remc1 --help` lists everything. Port options use `--long` form; original DOS options
(`-level N`, `-cheat N`, `-detail N`, `-network`, …) are passed through to the game's own parser
(`ProcessCommandLine_34DD0_35190`), which rejects anything it doesn't know — that is why port options
are stripped first (`portability/Settings.cpp`).

| Option | Use |
|---|---|
| `--data <dir>` | data folder (§3) |
| `--config <file>` | explicit `config.json` (see `config.json` example in repo root) |
| `--window WxH`, `--fullscreen`, `--no-grab`, `--no-vsync` | window (default 1280x960) / presenter |
| `--no-sound`, `--no-music` | disable sound effects / music (`--no-music` only silences replays) |
| `--music-device auto\|adlib\|gm\|mt32\|none` | music card; `auto` = the game's sound setup (sndsetup.inf). Also `audio.musicDevice` in config.json |
| `--music-volume N`, `--opl-emulator dosbox\|nuked` | music volume in percent; OPL3 emulator for the AdLib music |
| `--soundfont <file.sf2>` | SoundFont for the General MIDI music (`audio.soundfont`; default: system GM SoundFont on Linux) |
| `--midi-synth auto\|fluidsynth\|system\|opl` | synth for the General MIDI music (`audio.midiSynth`, §6.4b) |
| `--mt32-roms <dir>`, `--mt32-model <id>` | MT-32 / CM-32L ROM folder and Munt machine id for the Roland music (`audio.mt32Roms`, `audio.mt32Model`) |
| `--fps N` | in-game frame cap (default 30, `graphics.maxGameFps`); game speed follows it (§6.6) |
| `--no-wasd` | don't map W/A/S/D to the arrow keys (`input.wasdAsArrows`, default on for testing) |
| `--no-tab-enter` | don't map Tab to Enter (`input.tabAsEnter`, default on for testing - opens the spell map) |
| (config only) | `input.bothButtonsSpellMenu`: left+right mouse opens the spell map, as in the original (default off) |
| `--frame-stats` | print frame-time percentiles every 5 s (pacing work) |
| `--unpack <in> <out>` | utility: RNC-unpack one file and exit |
| `--debug-log` | write `debug.txt` (every file open/read) next to the exe |
| `--dump-frames <dir> [--dump-every N]` | save every Nth (default 30) presented frame as an 8-bit BMP `frame<N>.bmp`; windowed runs also write the presenter's read-back as `presented<N>.bmp` (what the window actually showed) |
| `--exit-after-frames N` | quit after N presented frames |
| `--input-script <file>` | scripted input (format in `portability/InputScript.h`) |
| `--record-input <file>` | record live input in the same format, **including timer ticks** (`T idx n` lines) so a real-time session replays exactly |
| `--deterministic [N]` | game timer advances N ticks per frame (no wall clock) |
| `--headless` | SDL dummy video driver, live input ignored (replays, CI) |
| `--no-render` | skip the 3D world view (`DrawWorld_30D90`), about 8x faster replays; ignored when frames are dumped, hashed or snapshotted. `rec.py play --headless` passes it (`--render` turns it off) |
| `--frame-hashes <file>` | log `frame WxH fnv1a64(framebuffer+palette)` per presented frame |
| `--state-trace <file>` | log `<frame> <state hash> <image hash>` per frame (`tools/regress.py`) |
| `--state-dump <frame>:<file>` | write the normalised game state of one frame (`tools/diff_state.py`) |
| `--trace-handlers <frame>` | print the game-state hash after every entity handler of that tick |
| `--snapshot-frame N[,M..] [--snapshot-dir D] [--snapshot-what W]` | memory snapshots for the diff harness ("Comparing with the original executable" below) |
| `--c-drive <dir>` | folder standing in for DOS `C:` (must contain `CARPET.CD`); default = data folder |
| `--watchdog N` | if no frame is presented for N s, print the main thread's stack and exit(3). Default 20 s for scripted/headless runs, off otherwise |

Env vars: `REMC1_DATA_DIR` (data folder), `REMC1_DEBUG_SOUND=1` (log every sample start),
`REMC1_DEBUG_MUSIC=1` (log song loads and the MIDI messages the game sends), `REMC1_MUSIC_WAV=<file>` (also write the music output to a WAV file),
`REMC1_ALERT_BREAK=1` (break into the debugger on `allert_error()`, see §10). Tests: `REMC1_UPDATE_GOLDENS=1`,
`REMC1_MT32_ROMS=<dir>`. Tools: `REMC1_EXE` (port exe for rec/regress/run_recording, else the newest build),
`REMC1_PORT_EXE` (exe/pdb the diff harness reads).

**Use `--deterministic` for hand-written scripts.** Recordings made with `--record-input` replay
exactly without it: the recorder logs how many timer ticks were applied at every sync point and the
player re-applies exactly those (`portability/port_timer.cpp`).

**Replaying user recordings:** a recording only replays exactly under the same sound setting it was
made with: enabling sound changes code paths (extra yields in the sample-done polling), hence tick
placement. Recordings from builds with sound disabled (e.g. `<data>/play2.txt`) need
`--no-sound`. Give each replay a fresh copy of `CARPET.CD` via `--c-drive` (saves/config are written);
`--c-drive` takes the folder that *contains* `CARPET.CD` - passing `CARPET.CD` itself silently starts
from a fresh install (language book) and the replay is meaningless.
Recordings made with sound **on** before the HMI timer fix (6.2.23) diverge after the first use of the
game clock; they are still useful as input scripts.

Headless replays exit 300 frames after the script's last event, and when the recorded tick log ends
the timer continues deterministically (5 ticks/frame) instead of starving tick waits.

### Driving the game without a human
This is how bring-up is done: script input, dump frames, look at them.
```bash
T=/tmp/run; mkdir -p $T/frames
printf '120 lclick 580 266\n400 key Escape\n' > $T/s.txt
build/win-x64/Debug/remc1.exe --data C:/mc1data --input-script $T/s.txt \
    --dump-frames $T/frames --dump-every 25 --exit-after-frames 1000
python tools/contact_sheet.py $T/frames --width 200 --cols 8     # -> $T/frames/sheet.png
```
Mouse coordinates in scripts are **game mouse space**: 640 wide, and twice the line count in
200-line modes (i.e. 640×400 for the 320×200 menus, 640×480 in 640×480 modes). To click a point at
`(x, y)` in a 320×200 screenshot use `(2x, 2y)`.

Useful scripted inputs so far:
- language book (first run only, writes `CARPET.CD/language.inf`): `lclick 180 120` (UK flag), then
  confirm stamp `lclick 584 256`.
- sound-setup book: confirm stamp `lclick 580 266`.
- intro FMVs: `key Escape` skips the current clip (the very first intro is unskippable until
  `CARPET.CD/intro.pld` exists).
- main menu (wizard's room): globe = new game `lclick 55 125`.
- in game (320×200): mouse centre is `(320, 200)`; `keydown Up` flies forward; `lclick`/`rclick` cast.
- See `tests/replays/*.txt` for complete, timed (deterministic) scripts.

### Memory bugs: `tools/asan_run.sh` (the main bring-up tool)
```bash
tools/asan_run.sh -- --headless --deterministic --data C:/mc1data --input-script s.txt --exit-after-frames 900
```
Builds the `win-x64-asan` preset (MSVC AddressSanitizer) and prints the first report: kind, stack
and the global/heap block involved. Most fixes in §9 were found this way. `--no-build` reuses the
last ASan build — careful, it is easy to run a stale binary (line numbers in reports then look off).

### Original data: `tools/exe_data.py`
`CARPET.EXE` (in `<data>/CD`) is a DOS4GW LE executable; initialised globals can be read by IDA
address (`python tools/exe_data.py --exe C:/mc1data/CD/CARPET.EXE dump 0x906DC 12`, or `carray` for a
C initializer). Use it whenever an initialised table looks truncated — the typical ASan symptom is a
global-buffer-overflow "N bytes after global variable X" where X has an initializer.

### User recordings: `tools/run_recording.py`
```bash
python tools/run_recording.py <data>/recordings/some_bug.recording --out /tmp/run --every 10 --from 400 --to 900 [-- --no-sound]
```
Fresh C: drive (as the replay tests), headless run, frame dumps, log tail (crash code, watchdog stack,
`allert_error`) and a contact sheet of the selected range. Start every user bug report here.

Recordings made with `--record-input` contain `C <frame> <hash>` lines every 30 frames (hash of the
simulation state: random seed + live entities, `Port_GameStateHash`). A replay compares them and logs
`DESYNC at frame N` at the first mismatch - check for it before trusting what a replay shows.
Headless replays skip the 3D view (`--no-render`): the game state never depends on `DrawWorld_30D90`.
The rest of the frame draw does run and can matter: the spell menu works out which spell is under the
mouse while it draws (`var_u8_22` of `str_AE408_AE3F8`), as in the original, so skipping all of
`DrawGameFrame_20FB0` desyncs at the first spell pick.
Frame dumps still draw every frame: the renderer keeps animation/texture state across frames, so drawing
only from N frames before a dump does not give the same image (tested up to 300 frames of warm-up), and
`/Ob2` instead of `/Ob1` gained nothing (2026-11-04). A fully drawn replay runs at ~220 frames/s.
While an input script plays, live SDL input is ignored (also windowed). `run_recording.py --real-audio`
uses the real audio device (recordings made with sound on can depend on it). Old recordings (no `C` lines) cannot be verified.
Key names are SDL's (`SDL_GetScancodeName`) and may contain spaces (`keydown Left Shift`) - the parser
reads the rest of the line; reading one word turned "Left Shift" into Left arrow and desynced replays.

Bundles (`<name>.recording/`, `docs/recordings.md`): `tools/rec.py record|play <name>` records/watches one;
`run_recording.py`, `regress.py` and the harness take bundles too. Arguments after `--` go to remc1.
State of the user's bundles in `<data>/recordings` (2026-10-25): `trace.txt` (for `regress.py`) exists
for levels 1-11 only. Since the start-position fix (2026-10-22, pattern #64) most of them desync early
(levels 1 and 3-15 within ~1100 frames; level 2 still replays exactly): expected after a gameplay fix
(harness gotcha 7). Regenerate traces with `regress.py --update` once the user re-records.

### Ground truth: `tools/exe_data.py disasm` / `strptrs`
`disasm ADDR LEN` disassembles the original code (needs capstone, from the venv in §4) - use it whenever a
decompiled line looks suspicious; `strptrs ADDR N` turns a table of `char*` in the data object into
a C initializer with the strings (pointers in the file are offsets from 0x90000, fixups unapplied).

### Comparing with the original executable (no human needed)
**Quick start** (the original CARPET.EXE code runs under Unicorn on a memory dump of the running port;
both versions of a function get the same memory, every write is compared):
```bash
source .venv/Scripts/activate                  # the venv with tools/requirements.txt (§4); tools run from the repo root
./build.sh win-x64-release                     # tools read build/win-x64/RelWithDebInfo/remc1.exe + .pdb
# 1. dump memory around the entity tick of presented frame N (list = one subdir f<N> each)
build/win-x64/RelWithDebInfo/remc1.exe --headless --data C:/mc1data --c-drive C --input-script rec.txt \
    --snapshot-frame 2000,2400 --snapshot-dir S [--snapshot-what tick|draw|frame|17C20|590D0|...]
# 2. compare
python tools/diff_tick.py S/f2000              # whole tick: "N bytes differ"
python tools/diff_tick.py S/f2000 --stepwise   # handler by handler; first differing handler + bytes
python tools/diff_tick.py S/f2000 --stepwise --trace-step K   # calls both versions make in step K
python tools/diff_funcs.py S/f2000 --func sub_132B0 --entity 657   # one function on one entity
python tools/diff_tick.py S/f2000 --watch 77403   # which original instruction writes state+77403
python tools/diff_call.py S/f2000 [--png out]  # draw/frame/per-frame-call snapshots (end to end)
python tools/diff_step.py S/f2000 0x2C410      # inside a draw: compare every call of an inner function
python tools/orig_disasm.py 0x2C410 --max 400  # read the original
```
Gotchas: (1) replay against the C: drive the recording was made with - a recording that starts by
loading a saved game desyncs (`DESYNC at frame ...`) without that save; copy the user's `CARPET.CD`
(the data folder is the default C: drive) minus saves made later. (2) Snapshots belong to the exe/pdb
that took them - rebuilding invalidates them; `REMC1_PORT_EXE=<copy>/remc1.exe` points the tools at a
saved copy. (3) Snapshot frame = presented-frame counter: a crash "after frame N" is snapshot N
(only `before.bin` gets written - enough for `diff_call.py`/`diff_step.py`). (4) ~100 MB per snapshot - delete them
after use. (5) `image_base` below 0x200000 = ASLR overlap with CARPET.EXE: copy
the exe to a new folder. (6) A difference may be a harness artifact: check `SKIP_GLOBALS`,
`LAYOUT_IGNORE_IDA/PORT`, `CLIP_GLOBALS`, port-only counters in `tools/emu.py`; confirm stepwise
findings end to end; if the ORIGINAL faults too, it is an original bug (§6.2 #59). The harness
lays globals out linker-style (a global ends where the next address-named one starts), so a stray
port declaration named one byte into a table cuts the table short: the unused `byte_B5D41` cut
`byte_B5D40_B5D30` (terrain-type table of `sub_33B90_33F80`) to 1 byte, and every tick that edited
terrain "differed" in `mapTerrainType`/`mapAngle` (the old "mapTerrainType stepwise artifacts").
Removed 2026-10-19. Check: load a snapshot with `Machine` and compare `mu.mem_read(ida, n)` with the
port's dump of that global; list cut-short globals with a span audit over `PortSymbols`. (7) Desyncs of
the user's recordings after a gameplay fix are expected - verify with the harness, not the replay.
(8) Recordings made with music on: the original tick calls the HMI MIDI driver (song done/start/stop,
raw MIDI, driver timer events), which faults under Unicorn; those entry points and the port_music /
port_timer calls that replaced them are in `SKIP_FUNCS` (both sides return 0). (9) A single
`--snapshot-frame N` writes straight into the snapshot dir; a list writes `D/f<N>` subdirs.

Static audits (repo root, after a RelWithDebInfo build; all read `<data>/CD/CARPET.EXE`):
- `tools/le_image.py` - LE loader with fixups applied (IDA addresses; `fixup_sites` = pointer dwords).
- `tools/audit_calls.py` - per function: calls of the original vs calls in the port body.
- `tools/audit_data.py` - initial value of every address-named port global vs the original bytes,
  pointers compared by target (the port PDB maps port addresses back to names/IDA addresses).
- `tools/audit_tables.py` - entity dispatch tables walked to the original terminator (truncations).
- `tools/audit_consts.py` - numbers/offsets the original uses that the port body never mentions (noisy).
- `tools/audit_signedness.py` - entity fields the original widens with the other signedness (§6.2 #50).
- `tools/audit_clobber.py` - call sites reading a register the callee overwrote (§6.2 #51).

Differential execution (Unicorn and capstone, from the venv in §4):
1. `remc1.exe ... --input-script rec.txt --snapshot-frame N --snapshot-dir D` dumps the process's
   writable memory before and after the entity tick of frame N (`portability/MemSnapshot.cpp`).
2. `python tools/diff_funcs.py D` runs every class/state handler, ORIGINAL code and PORT code, on the
   live entities of the snapshot and compares all writes (`--func NAME --entity I` for one function).
   `tools/diff_tick.py D` runs the original whole tick and compares with the port's "after" dump;
   `--stepwise` stops the original tick at every handler dispatch (0x41A71), runs original and port
   handler on the same memory, keeps the original's result (mirrored into the port globals) and goes
   on - the first differing step names the culprit with its exact inputs (`--trace-step N` logs the
   calls both versions make). `--watch OFF,...` (whole-tick mode) prints the original instruction
   that writes those game-state offsets - for differences outside the handlers.
   `--snapshot-frame 300,700,1100` takes several snapshots in one run (`D/f300`, ...).
   Writes to port-only instrumentation globals (`counter_*`, `compare_*`) are ignored.
   Pitfall found the hard way: never translate "pointers" by value alone - packed table words
   (atan/sin tables) spell port addresses; translate only at the pointer offsets the PDB types give.
3. Sweep: snapshot every 400 frames of every recording, whole-tick diff each, stepwise on mismatch
   (a script along the lines of `for rec; do remc1 --snapshot-frame 300,700,...; for f in D/f*; do
   python tools/diff_tick.py $f; done; done`). Snapshots are ~100 MB; delete them after the diff, and
   use the exe/pdb they were taken with (`REMC1_PORT_EXE=...` points the tools at a copy) - any rebuild
   invalidates old snapshots. With patterns 48-53 fixed, the sweep over all 21 recordings in `<data>`
   matches the original tick byte for byte except where listed in the roadmap.
4. `--snapshot-what draw` + `tools/diff_call.py D --png out` does the same for one `DrawWorld_30D90`
   call (original/port/diff images; the original takes the screen pointer as a hidden first argument).
   `tools/diff_step.py D 0x2C410 [--nargs N]` stops the original call at every entry of the given inner
   functions, runs original and port version on the same memory and reports the first calls that write
   differently (e.g. one sprite of the frame). Caveat: it mirrors the original's global writes into
   the port, which is wrong for pointers into data laid out differently (`sprData_B5CB0`) - confirm a
   finding with `diff_call.py` end to end.
   More snapshot points: `--snapshot-what frame` (the whole `DrawGameFrame_20FB0`: world, HUD, minimap)
   and `--snapshot-what 17C20` / `590D0` / `3C9D0` / `415C0` / `5A560` / `30B30` (the other per-frame
   calls of `DrawAndEventsInGame_34530`, hooked with `SNAP_CALL`). `30B30` presents the frame (platform
   blit) and cannot be compared. If a snapshot's `image_base` is below 0x200000, ASLR put the exe where
   the harness maps CARPET.EXE - copy the exe to a new directory (new base) and take it again.
   Open finding: in 640x480 (`<data>/recording.txt`, `--snapshot-what draw --snapshot-frame 4500`)
   the sprite blitter `sub_2C410_2C450` draws horizontal streaks (pixel 201) that the original does not
   (`diff_step.py D 0x2C410`: 4th call, a1 = 0). Every 320x200 draw/frame snapshot of the 21 recordings
   matches except level-start frames that load files. The renderer is the place where port globals are members of
   structs (`strPal.fog_B7934`): the harness maps address-named struct members too.
`tools/emu.py` builds the machine: snapshot regions at their own addresses (heap pointers stay valid),
CARPET.EXE at its IDA addresses, port `.text/.rdata` relocated to the snapshot's image base, CRT
imports as Python stubs, address-named globals copied to their original addresses (bigger first,
then specific ones; a global whose static bytes disagree with the original at that address loses;
pointers translated only at relocation sites/aligned dwords). Sound (`sub_55370_558A0`) is skipped on
both sides. Known layout differences the comparison cannot map: `word_12CD20/22/24/26/28` (the
original's 10-byte record array split into parallel arrays), `trig_9074C`'s first 1 KB (small
variables, not a table).

### Crashes: `tools/debug_run.sh`
```bash
tools/debug_run.sh -- --data C:/mc1data --input-script s.txt --exit-after-frames 3000
```
Runs under `cdb` (Windows SDK debugger) and prints a symbolised stack + registers on the first
access violation, privileged instruction or call to `exit`. It passes **`-hd`** (no debug heap):
under a debugger Windows otherwise uses a debug heap and many out-of-bounds reads stop reproducing.

### Finding decompilation bugs mechanically
- `python tools/scan_ptr_arith.py` — lists "byte offset added to a typed pointer" sites (see §6.2).
  Should report 0; re-run after touching types in `engine/Basic.h`.
- `python tools/scan_wide_consts.py` — 8/16-bit struct fields combined with constants that do not fit
  them (no-op `|=`, never-true `&`, §6.2 #55). Should report 0.

---

## 5. Repository map

```
REMC1.cpp                 main(): InitSettings() then sub_main()
sub_main.cpp/.h           THE decompiled game (~112k lines, one file — split planned in Phase 3)
defs.h                    Hex-Rays helper macros (_BYTE/_WORD/_DWORD, LOBYTE, __ROL2__, ...)
engine/Basic.h            packed game structs (Type_str_AE400_AE3F0, Type_AE400_29795 entity, TypeTab, ...)
engine/ptr32.*            32-bit offsets in a 64-bit program: PTR/OFF, ptr32<T>, the game arena (§7)
engine/CommandLineParser  REMC2 legacy debug flags (defaults only; port options are in Settings)
portability/
  Settings.*              port options: CLI, config.json, data path resolution
  port_sdl_vga_mouse.*    VGA emulation: palette, blit, keyboard + mouse event pump, frame dumps
  Presenter.h, SdlPresenter.cpp  IPresenter + SDL2 renderer / headless null presenter (§6.5)
  port_timer.*            PIT/INT 8 replacement (real-time thread or deterministic), frame pacing
  port_sound.*            HMI SOS digital driver on SDL_mixer (§6.4)
  port_music.*, midi_sequencer.cpp  HMI MIDI driver: libADLMIDI, FluidSynth, Windows MIDI, Munt (§6.4b)
  MemSnapshot.*           --snapshot-frame dumps for the diff harness
  port_watchdog.*         hang detector (prints the main thread's stack)
  InputScript.*           scripted/recorded input
  port_filesystem.*       DOS file API shims (myopen, mycreate, findfirst, ...), debug_printf
  fcaseopen.*             case-insensitive fopen for Linux/macOS
  port_time.*, port_outputs.*, dirent*  misc shims
utilities/
  DataFileIO.*            file wrappers + an RNC unpacker
  Compare.*               memory comparison against DOSBox dumps (regression workflow)
  rnc_1fm.*               (not compiled; a copy lives in sub_main.cpp)
tools/                    Python + shell tools (§4): data extraction, recordings, regression, diff harness, audits
tests/                    GoogleTest target remc1-tests + replay tests (ctest, §8)
AGENT_DEV_GUIDE.md        this guide
docs/                     building-{windows,linux,macos}.md, recordings.md, original-bugs.md
build.sh, CMakePresets.json, vcpkg.json   build entry points (§2)
config.json               example port settings
assets/font/              debug font required by VGA_Init (copied next to the exe)
dev-info*.txt             upstream author's scratch notes (Czech/English) — addresses, open issues
```

---

## 6. Working with the decompiled code

### 6.1 Naming and conventions
- Functions/globals carry their original addresses: `sub_<IDA addr>_<addr in other build>`,
  e.g. `RncUnpack_62B60_63070`. When renaming, **keep the address suffix** (remc2 convention), so the
  function can still be found in IDA/DOSBox: `DrawWorld_30D90`, `TopProcedure_340B0_34470`.
- **Rename as you learn.** Whenever you work out what a `sub_*` routine does, rename it in the same
  change (remc2 README "Development Guide"): PascalCase verb phrase + the original address id(s),
  e.g. `sub_37920_37CE0` -> `CreateCastle_37920_37CE0`, and put a `///` doc comment above the
  definition saying what it does, what the parameters/entity offsets mean and how sure you are
  ("inferred, not verified" is fine). Update every reference. Variables camelCase, fixed-width types
  (`uint16_t`, ...) in new code.
- **The same for every other symbol** once its meaning is certain: globals (`dword_9AD98` ->
  `mouseInstalled_9AD98`, `unk_98F38` -> `creatureTypes_98F38`) keep the address suffix, struct
  fields keep the offset suffix (`Type_156::v_14` -> `vertSpeed_14`); add a short comment at the
  definition. Field names are not unique across structs - rename through the access path
  (`var_u32_29951_156->v_14`) or the struct block, never with a global `\bv_14\b`, then rerun
  `tools/gen_layout_tests.py` (the layout tests use field names). Guesses go in comments, not names.
- `//SYNCHRONIZED WITH REMC1` marks functions the upstream author verified against DOSBox.
- `str_AE400_AE3F0` is the big game-state struct (`Type_str_AE400_AE3F0`), `str_AE408_AE3F8` the
  settings struct; `str_AE400_AE3F0->str_29795[1000]` is the entity array (164 bytes each).
- Mark every port-specific change in game code with a `// PORT:` comment explaining why.
- `sub_main.cpp` has Latin-1 bytes (and is LF in the repo, see below). When editing with scripts, open it in
  binary mode (`open(p,'rb').read().decode('latin-1')`) or the line endings get rewritten. The Edit
  tool is the safest way to change it.

### 6.1b Entities: fields, classes and types
Add every class/type you identify here (and say how sure it is).

Entity record (`str_29795[i]`, game state + 29795 + 164*i; entity 0 unused): +8 max life, +12 life,
+16 flags dword (`var_29811_16`), +24 id, +42 owner entity (spells: the wizard holding them), +52/+54
leader/follower (worm segments), +64 class, +65 type (called model in places), +70 state, +72 position
(`axis_3d`, x/y in 1/256 tile, z), +132 mana delta, +136/+140 mana max/current, +160 wizard data
(`Type_1103`). Free list top: game state +40 (`var_u32_40`); recycle list top `var_u32_4593`.
Game state +36 / +38: the active volcano eruption (10/18) and its 10/19 entity (both can be stale,
original-bugs #11).

Tables: `dword_96902[class]` holds the state handlers (`.str_0[state].data6`, called every tick by
`UpdateEntities_41780`) and the constructors (`.str_4[type].data6`, `CreateEntity_373F0_377B0(pos,
class, type)`). The state index is global within a class, not per type.

| Class | Type | What | Notes |
|---|---|---|---|
| 2 | 0 | tree (palm) | scenery, on the recycle list (original-bugs #10); state 2 = burnt (inferred) |
| 2 | 1 | stone (kind not checked) | scenery, recyclable |
| 2 | 3 | standing stone (the stone rings that mark triggers) | scenery, recyclable |
| 3 | 0 | wizard on a carpet (player 0 = local human) | model 0 human, 1 computer in the wizard list; `Type_1103` at +160 |
| 3 | 2 | castle | inferred (level 26: life 40000, owner = wizard) |
| 3 | 3 | balloon | original-bugs #8 |
| 3 | 4..11 | wizard start positions only | `sub_44D30_45070` |
| 5 | 3 | worm segment | chain via +52/+54, original-bugs #1 |
| 5 | 5 | crab (big ones shoot lightning) | seeks mana balls, raises more through 10/52 |
| 5 | 16 | wyvern | life 100000, mana 50000 (level 26) |
| 9 | 3 | missile that starts an explosion ring (10/17) when it lands | `sub_3AC70` called from 0x52AAA |
| 9 | 9 | lightning bolt piece | `LightningBoltTick_535E0_53920` |
| 10 | 0 | explosion | `sub_24F60` |
| 10 | 1 | created with the mana ball when a creature dies | `sub_1A800` |
| 10 | 6 | spawned when a spell-7 jar in state 3t+2 is picked up | `SpellJarSpawnTick_55D30_56260` |
| 10 | 16 | lava from a volcano eruption | created by 10/18 |
| 10 | 17 | expanding explosion ring: one 10/0 explosion per ring point per tick | `sub_25CE0` |
| 10 | 18 | volcano eruption emitter (128 ticks, then removes itself) | `sub_25EC0` |
| 10 | 19 | created by each eruption start, index in game state +38 | |
| 10 | 27 | terrain-raising level-build step | `RaiseTerrainStrip*` |
| 10 | 39 | mana ball | mana at +140; +144 = owner (adds its mana to the owner's +136 every tick) |
| 10 | 40 | death marker of a wizard: holds its mana balls until it respawns (state 42) | `sub_275C0`, original-bugs #12 |
| 10 | 52 | crab spawn timer (state 0x38) | |
| 11 | 2 | trigger (marked by standing stones on level 26; not recyclable) | life 300 |
| 12 | 0..23 | spell, type = spell number (0 fireball, 2 speed, 16 castle) | states below |

Spells (class 12), spell t: state 3t = owned, handler = the spell's cast routine (`CastFireball_56090`,
`SpeedSpellTick_56380`, `CastCastle_57610`, ...); 3t+1 = jar on the ground (`SpellJarTick_55DB0_562E0`);
3t+2 = jar that also spawns the spell's `off_987DE` entity when picked up (`SpellJarSpawnTick_55D30_56260`).
Picking a jar up (`sub_55A40_55F70`) sets the owner and state 3t; when the owner dies every owned spell goes
3t -> 3t+1 and scatters around them (in `level26_spell_bug.recording` they were back at their start
positions, unowned, 45 frames after a death; the code that does that is not identified yet).

### 6.2 Recurring decompilation bug patterns (check for these first)
1. **Byte offset added to a typed pointer.** IDA's `*(_BYTE *)(ptr + 392)` assumed `ptr` was an
   int; once the variable/field was given a struct type the addition is scaled by `sizeof(*ptr)`.
   Example: `a1x->var_u32_29955_160` is `Type_160*` (946 bytes), so `+ 392` wrote 370 KB away.
   Fix: `*(_BYTE *)((uint8_t*)ptr + 392)`. Found mechanically by `tools/scan_ptr_arith.py`.
2. **`_UNKNOWN` globals are 1 byte.** `defs.h` makes `_UNKNOWN` a `char`; several were really
   buffers (`fscanf("%s", &musicCard_CBF74)`, `strcpy(&unk_A05D1, ...)`) or ints, silently overwriting
   neighbours. Give them real types/sizes (marked `// PORT:`).
3. **Debug probes reading out of bounds.** The upstream author left breakpoint anchors like
   `if (input[0x1fe2+15]==0xff) { counter++; counter--; }`. Harmless in DOSBox comparisons, fatal on
   small heap buffers. Delete them when they crash.
4. **Debug switches at the top of `sub_main.cpp`.** `MODIFY_SETTINGS` injects key presses at level
   start and `FIX_MOUSE` pins the mouse to the centre — both must stay off for normal play.
5. **Live x86 inline asm / port I/O.** `in al,dx` etc. fault with "privileged instruction". Search
   `__asm` and `inp(`/`outp(`; DOS hardware is to be emulated in `portability/`, not executed.
6. **Busy-waits on counters written by the timer thread.** E.g. `while (dword_AC5D4_AC5C4 < x) {}`.
   The counter must be `volatile` and the loop must call `PortTimer_Yield()` (otherwise deterministic
   mode hangs and real-time mode burns a core). The watchdog (`--watchdog`) pinpoints these.
7. **One DOS object split into several C globals.** IDA names every address it sees; code that walks
   past one name into the next was correct in DOS memory. Examples fixed: the colour lookup
   `byte_AD167_AD157[4097]` whose last entry is `byte_AE167_AE157`; `dword_9074C`/`sin_90B4C`/
   `cos_9134C` = one trig table (`trig_9074C`); the FMV command tables (`fmvTables_4A17C/4A568`).
   Symptom: ASan global-buffer-overflow just past a global. Fix: one array + `#define` aliases.
8. **Truncated initializers.** `int x[2] = {...}` where the real table is longer (FMV tables,
   `byte_906DC[6]`). Recover the bytes with `tools/exe_data.py`.
9. **One DOS stack struct split into adjacent locals.** `int v9; __int16 v10;` used as `(axis_3d*)&v9`.
   Fix: declare an `axis_3d` and alias the names with function-scoped `#define`/`#undef`.
10. **Missing `&` / value-vs-address mixups.** e.g. `begBscreenAdress = tempBegBscreen[i]` where every
    sibling branch stores an address. Compare sibling `case`s — they are usually symmetric.
11. **Source/destination merged.** Hex-Rays sometimes maps `esi` and `edi` to one variable (the sprite
    blitter read from the screen it was writing). Compare with remc2's equivalent function.
12. **Signedness.** Pointers declared `uint32*` where the asm compares signed (`jg`/`jl`): `a - b > 0`
    is then true whenever `a != b`. remc2's versions use signed `x_DWORD`.
13. **Wrong control flow around `goto`.** Labels placed inside a `case` that then `break`s may re-enter
    loops; check against remc2 (e.g. the sprite clipper must `goto LABEL_181` after `LABEL_325`).
14. **Pool overruns.** DOS allocated buffers from one contiguous pool, so writing a little past a
    buffer only touched the next one. Enlarge the buffer (with a `// PORT:` note) when the overrun
    is by design (e.g. `*WScreen` 71040 → 76800, `var_u32_36462` list heads).

15. **Game-state fields shown as globals indexed by the state pointer.** `&byte_38C93[dword_AE400_AE3F0()]`
    is *not* a global array: it is field `0x38C93` of the game-state block (IDA added the base pointer
    to an absolute address). Fix: `(uint8_t*)dword_AE400_AE3F0() + 0x38C93` (the castle-spell crash).
16. **Struct field width/offset drift.** A field declared 4 bytes where the code uses 2 shifts every
    later field (`Type_160::var_48` uint32 to int16; padding `stub_aa` 246 to 248). The disabled test
    `LayoutNames.AllNameImpliedOffsets` (`--gtest_also_run_disabled_tests`) lists fields whose
    name suffix disagrees with their real offset, a good place to look.
17. **Statement order "assign, then use the old value".** Hex-Rays sometimes emits `x = y; d = x - y;`
    where the asm computed the delta first (sprite clipping, 3 sites). Check against remc2.
18. **16-bit carry idioms.** `par3 += par5; if (par5 > (par5 + par3) % 0x10000)` was meant to test the
    carry out of a 16-bit add of the *old* value; decompiled wrong, it rotates rolled sprites
    (`PrepRot_29C30_29C70`, all 8 octants). Write it with explicit `uint16_t` temporaries.
19. **uint16 vs int16 for angles.** Pitch (`Type_160::u16_329`, `word_0x6_6`, `pitch_8`) is signed; the
    unsigned version made the camera spin past vertical. Roll (`roll_10`) really is unsigned.
20. **Uninitialised return values.** Many functions `return result;` on paths that never assign it (the
    DOS code returned whatever was in `eax`). Debug builds therefore use `/RTCs` instead of `/RTC1`
    (no `/RTCu` aborts); initialise `result` when a caller actually uses it.
21. **`__inbyte`/`__outbyte` are real instructions.** They are MSVC intrinsics, so they compiled fine and
    faulted at run time (joystick probe reached from the controls menu). All are now routed through
    `Port_InByte`/`Port_OutByte` (§6.3).
22. **Hex-Rays byte/word macros on retyped variables.** `HIBYTE(x)`/`HIWORD(x)`/`SHIWORD(x)` index from
    the *end* of `x`. IDA applied them to a scalar; when the port retyped `word_AE454_AE444` as
    `axis_3d`, `HIBYTE(word_AE454_AE444)` started reading the high byte of `.z` and castles were built
    at x tile = z>>8 (far out at sea). `defs.h` now `static_assert`s that these macros only see 1/2/4/8
    byte objects, so any new instance fails to compile - fix by naming the field (`.x`).
23. **Missing timer registration with sound on.** With a sound driver present the game registers its
    120 Hz clock (`HmiTimerTick_357A0`) with the HMI timer service instead of the PIT ISR. That call
    was commented out, so `dword_AC5D4_AC5C4` never advanced with sound enabled and every wait on it
    (post-level info FMV) hung. Now `PortTimer_Start(1193182/120, HmiTimerTick_357A0)`.
24. **Signed fields declared unsigned in structs.** `axis_4d` (entity collision box at +78) had its
    z offset as `uint16_t`; castles use -8192, so the box test compared against z+57344 and nothing
    ever hit a castle (castle-upgrade missiles flew through). When a comparison involving a struct
    field never succeeds, print the raw values - a 0xE000-ish number is a negative in disguise.
25. **Hand-edited list/array names.** The per-tick list builder (`UpdateEntities_41780_41AC0`) stored
    creature list heads in `var_u32_36462[model]` instead of `str_36382x[model]`, leaving the creature
    lists empty (no spell auto-aim, no AI targeting) and overwriting the wizard/missile/effect heads.
    An earlier port "fix" had enlarged `var_u32_36462` to absorb the overflow - when an array needs
    to grow to stop a crash, first check whether the code is writing the right array.
    The same function family had `str_36382x[model + 36382]` (byte offset added to a typed index).
26. **Truncated pointer tables.** `char* dword_999B8[] = { 0 }` (level names on the score screen) was
    one entry; the screen after a completed level read garbage and crashed. Regenerate with
    `exe_data.py strptrs`. Other `[] = { 0 }` tables are suspects (list: `grep "\[\] = { 0 }"`).
27. **Byte offsets added to a typed pointer in "int + ptr" order.** `2049 * i + str_AE400_AE3F0 + 13323`
    (new game after abandoning one). `scan_ptr_arith.py` only catches `ptr + n`; grep for
    `+ str_AE400_AE3F0 +` style expressions too.
28. **Read-modify-write that is really a copy.** `a->speed += b->accel` was `a->speed = b->speed +
    b->accel` (worms accelerating forever and flying). When a value grows without bound, disassemble.
29. **Texture over-reads.** The rasteriser reads up to 64 KB past a texture; loaded data files are
    padded with zeros (`sub_634E0_639F0`) so this is defined and identical in Debug and Release - a
    Debug/Release golden mismatch is the symptom of such reads (find them with ASan).
30. **Operator precedence in hand-edited expressions.** `features & 0xffff >= 0` is `features & 1`
    (comparison binds tighter than `&`): every other terrain cell lost the water mode - checkerboard
    sea (asm: `test ch, 0x80`). `a * b + x << 16` is `(a * b + x) << 16` (top-of-screen glitch).
    Write `(x & m) == v` and `a * b + (x << 16)`.
31. **Use experiments to localise render bugs.** Force a flag/branch on a single frame
    (`VGA_PresentedFrames() == N`), paint triangles in a solid colour (`byte_967E1 = 0`, colour in
    `byte_967E0`), dump per-vertex values, then reproduce in `tests/test_render.cpp` with captured data.
32. **`rep stosd` counts are dwords.** `sub_411D7/411FD` cleared `80 * rows` *bytes* (a quarter of
    the screen) where the asm stores `80 * rows` dwords: the spell map left cursor trails.
    When an asm loop uses `stosd`/`movsd`, the C size is 4x the count.
33. **Inverted queue conditions.** Control commands 23/24 (spell hotkey assign/use) were queued only
    if a command was already pending; the asm queues when the slot is empty or holds the same command.
34. **Null/garbage pointers DOS tolerated.** DOS4GW maps low memory, so reads/writes through null or
    stale pointers silently succeeded (animation sequence at level end, player slots of freed or empty
    wizards in the menu demo). Guard them with a `// PORT:` comment; prefer guards that mirror the
    data's meaning (e.g. "is a live wizard", class 3) over bare null checks.
    Also `AiApproachTarget_15470` (AI wizard: distance to target `a2 + 0x48` measured before `a2` is checked; `a2 = 0`
    from `AiBuildCastle_138F0`, level 7 crash).
    Related, **dummy leading parameter**: `RaiseTerrainStrip*_26560/26670/26760` (level-build steps, class 10
    type 27) were typed `(int a1, T a2)` and used `a2`, but the dispatch table passes only the entity
    (original: `[esp+1Ch]` = first argument). The entity was never removed and level loading
    (`sub_36620_369E0`) looped forever (level 8 freeze). All 431 table handlers were checked for >1 parameter.
    Split stack structs again: `LightningBoltTick_535E0_53920` passed `&v27` (int) as the `axis_3d` step of
    `sub_41EC0_42200`; the z step went past `v27`, so bolts never climbed/dived to their target (no lock-on).
    No other `int` local is passed as `axis_3d*` (scanned).
    Signedness (#50 family): `NextRingOffset_114B0` read the ring tables' (dx, dy) as unsigned bytes (asm
    `movsx`), so every area effect built on the ring iterator (meteor crater, ...) only reached one side
    of its centre - smaller, off-centre explosions.
35. **Relocation after loading.** `sub_416B0` re-bases stored type pointers (+156) after loading a
    saved game/demo; the delta must be computed once before the loop (it was recomputed from an
    already-relocated entity).
36. **Pass-by-value vs pass-address.** `push offset dword_X` before a call means the callee gets the
    *address* of the global (and usually nulls it: `mov [ebx], 0`). Hex-Rays sometimes types it as the
    value, so the global dangles after the free (`DestroySpriteCache_36C10_36FD0`,
    `FreeAnimSequences_11010`). Check the caller's push in the disassembly.
37. **Statement order around a move.** `FreeSpriteCacheBlock_36A90_36E50` compacts the sprite cache by
    sliding blocks down; the asm loads the old data pointer (`mov esi,[eax]`) *before* storing the new
    one, the C stored first and then copied the block onto itself. Every sprite moved by compaction
    kept stale bytes - mostly-transparent worm segments in level 2 after playing level 1 (a loaded save
    starts with a fresh cache, so it looked fine). Use `memmove` for such overlapping moves.
38. **Loop bound recomputed from the index.** `RelocateFontGlyphs_519E0_51D20` looped
    `i < eTab - &bTab[i]` (the original compares against a fixed end pointer), so only half the glyphs
    were relocated - the score board drew no text.
39. **Calls through `_UNKNOWN locret_*` objects.** Hex-Rays declares a bare `ret` it did not turn into a
    function as `_UNKNOWN locret_X; // weak` and calls it via a cast - in the port that is a call to
    address 0 (the speed spell crashed on cast, `locret_455C0_45900`). Check the address with
    `exe_data.py disasm`; if it is just `ret`, delete the call.
40. **Wrong index into a typed data table.** Creatures get their movement profile (`Type_156`: min/max
    height above ground, vertical speed, ...) as a pointer into `unk_98F38` (32-byte entries at
    0x98F38). `CreateGuard_386E0` used entry 0 (a hovering type) instead of 16 (the original stores
    0x99138), so level-3 guards floated off the ground. All other uses were checked against the
    immediates in the original code; repeat that check (scan the code object for aligned addresses
    inside the table, `tools/check_creature_types.py`) whenever a table pointer looks suspicious.
41. **DOS mouse-driver units.** In 640x480 (`typeResolution & 8`) the game set the INT 33h range to
    0..5120 x 0..3840 and divides positions by 8 (`UpdateMouseEventData_5BE72_5C382`); setting the
    position multiplies by 8 (`sub_59750`). The port feeds pixels, so `MouseEvents` scales by 8 in that
    mode and `VGA_Set_mouse` is given pixels. Without it the cursor was stuck in the top-left 80x60 and
    mouse steering died after pressing R.
42. **Guards commented out during cleanup.** `sub_2C410_2C450` had its `if ( a1 != 2 )` around the
    rolled-sprite path commented out, so pass 2 ran it and wrote outside the screen buffer (crash in
    640x480; a few stray pixels in 320x200). When a decompiled condition is commented out, check the
    asm/remc2 (`DrawSprite_41BD3`) before trusting the edit.
43. **One original object split into separate globals.** IDA named `dword_96894` (glyph table) and
    `word_96898` (flags) separately; the code passes `&dword_96894` as a font object and reads the
    flags at +4. In the port the two globals are not adjacent, so `DrawText` saw the wrong flags and
    the level performance screen (`ShowLevelPerformance_4E5B0_4E8F0`) drew black dots instead of text.
    When a global's address is cast to a struct pointer, make it that struct (`perfFont_96894`) and
    take the initial bytes from `exe_data.py dump`.
44. **One register, two C variables.** Hex-Rays split `eax` into `result` and `v4` in `sub_28D10`;
    one switch case created the entity into `v4` but set its state through `result` (null on that
    path), crashing in level 4 when an entity spawned that creature type (after the player died and the
    level reloaded). Watch for `if ( vN ) other->...` right after a create.
45. **Writes the original's memory layout absorbed.** The rotated-sprite path of `sub_2C410_2C450` starts
    rows up to a view width above/below the screen and clips them with tables in the BScreen scratch
    area (36960/45920/59360). A sprite right in front of the camera in 640x480 overruns those tables,
    so rows are drawn outside the screen; in DOS that hit neighbouring pool blocks. The port gives
    `*WScreen`/`*BScreen` a guard band (`MallocGuarded`, freed through `free_426E0_42A20`); remc2
    keeps the same tables. Symptom: intermittent crash (heap-layout dependent) - use `tools/asan_run.sh`.
    Related: `ShowLevelPerformance_4E5B0_4E8F0` freed its font and then drew one more pass with it.
46. **Half-converted typed offsets (again).** `sub_13BA0` (AI wizard, mode 6: chase target) computed its
    heading towards `((uint8_t*)v1) + 36` with `v1` a `_WORD*` - 36 *bytes* into the target instead of
    its position at byte 72 (`v1 + 36`, asm `lea edi,[esi+48h]`). The AI wizard flew to the map corner
    and circled there. When one call uses `(uint8_t*)v + N` and a sibling uses `v + N`, check the asm.
47. **Constants shown as addresses.** IDA turns immediates that happen to be valid code addresses into
    `(int)&loc_13880` (= 80000), `(int)&loc_270FA + 6` (= 160000) etc. In the port these are addresses
    of `_UNKNOWN` placeholders. The castle upgrade cost from level 4 on (`sub_47C60_47FA0`) was such an
    address, so castles stopped at level 4. Grep `(int)&loc_` / `(uint32)&loc_` and replace them with
    the hex value of the name (plus the offset).
48. **Wrong helper macros in defs.h.** `__CFSHL__(x, y)` (carry flag of `x << y`) was defined as
    `(x<<y)`. Hex-Rays writes signed division by 2^y as `(v - (__CFSHL__(v >> 31, y) + (v >> 31 << y))) >> y`,
    so all 87 such divisions were wrong for negative numbers (player pitch, headings, ...). Found by
    `tools/diff_tick.py --stepwise` (one byte of player state); after the fix the entity tick of the
    level-5 snapshot matches the original call for call. The other flag helpers (`__CFADD__`, `__OFSUB__`,
    ...) are IDA's reference templates. When a whole class of results is off, check defs.h first.
49. **Wrong field width/sign in a struct.** Entity +132/+136/+140 (mana delta, capacity, current) are
    signed dwords (signed compares, `idiv`), the port had uint16 + stub and two uint32: a spell's
    negative cost was added as ~65000 and a mana value below 0 wrapped and was clamped to the capacity,
    so mana never ran out. Also: `sub_55E80_563B0` had lost the store of the spell cost into +132.
50. **Unsigned struct fields the original sign-extends.** `axis_3d.x/.y` are `uint16_t`, but the
    original loads entity x/y with `movsx` 75 times and `movzx` once. Wherever the value feeds something
    sign-sensitive the port differs for positions >= 0x8000 - half of every map: `abs(x1 - x2) < 256`
    neighbour tests with an `(__int16)` other side (`UpdateFollower_1A390`, `sub_20480`), signed
    divisions by 256 (`sub_120B0`, `sub_127E0`: scanned a window one cell off), `(x + 128) >> 8` passed
    on as an int (`sub_40A10` tests `!a1`). Fixed with `(__int16)` casts at those sites; the type itself
    stays unsigned (map indexing like `x >> 8` relies on it). `tools/audit_signedness.py` lists, per
    8/16-bit entity field, the functions whose original widens it with the other signedness; only x/y
    ever hold values with the sign bit set (checked on snapshots), the rest are small enums.
    **Variant: sign tests on unsigned byte arrays.** The original tests a flag bit with `test b, 80h`;
    Hex-Rays writes that as `arr[i] < 0` / `>= 0`, which is constant once the port declares the array
    `uint8_t`. `mapAngle_FC1E0_FC1D0` bit 0x80 = impassable tile: `IsCastleSiteFree_12F70`,
    `CanCastleGrow_12D10` and `AddTileHeight_40A10_40D50` never saw walls (castles built on and grew into
    walls, level 12). Fixed with `(arr[i] & 0x80)`. MSVC C4296 does NOT catch these (the byte is
    promoted to int); scan with a script for `NAME[...] < 0` / `>= 0` where NAME is declared unsigned
    (done 2026-10-19: no other sites).
51. **Registers a callee leaves behind.** Watcom code preserves everything but eax and Hex-Rays assumes
    so; a few asm helpers don't. `sub_42410` (squared distance) returns dx*dx + dy*dy in eax and leaves
    dy*dy in edx; `sub_54F80` then pushes edx as the argument of the square root 0x410CE, so the original
    stores |dy| in entity +48 - Hex-Rays showed the caller's stale edx (the player index).
    `tools/audit_clobber.py` lists call sites that read a register the callee overwrote; in game code
    this was the only real one (the rest: far pointers returned in dx, joystick asm, DOS calls).
52. **Hand-"fixed" idioms.** `sub_27030` had `-((v - __CFSHL__(v >> 31, 2) + (v >> 31)) >> 2)` - someone's
    rewrite of the signed division idiom that computes `(v - 2) >> 2` for v < 0 instead of `v / 4`.
    Write such divisions as plain C `v / 4` (truncates toward zero, same as the `sbb` idiom).
53. **Wrong list / wrong field in a hand-converted loop.** `sub_16540` (missile hits feeding the wizard
    AI counters) walked list 0 (wizards) instead of list 3 (missiles, `[settings + 8E7Ah]`) and one
    branch switched on +63 instead of the model byte +65. Only the whole-tick diff sees this kind
    (the function runs from `UpdateEntities`, not from a handler).
54. **DOS VGA window addresses compared with host pointers.** The 640x480 cursor code
    (`sub_5B650_5BB60` draw, `sub_5BB60_5C070` restore) wrapped its write pointer at the 64K bank end
    `>= 0xB0000`; a host pointer always exceeds that, so every pixel stepped back 64K - memory
    corruption / crash in high-res. `tempScreenBuffer_A0000` is linear in the port: drop the wrap.
55. **Constants wider than the field.** `triangleFeatures_38` (uint16, terrain vertex flags) was given
    `|= 0x100000` / `0x10000..0x80000` and tested `& 0x100000` - all no-ops; the asm sets bits of the
    byte at +39 (`or byte [+27h], 10h` = 0x1000). Tiles of the `byte_90168` terrain types never took
    their own render path. Grep for flag constants that cannot fit the field's type.
56. **Hand-narrowed branch conditions.** The sprite blitter `sub_2C410_2C450` took its rotated path for
    `!byte_B5D3E && a1 != 2`; the asm takes it for every a1 when `byte_B5D3E == 0`, with a1 == 2 sharing
    the a1 == 0 offset - billboard sprites (`DrawSprite_2FC50`, a1 = 2) were not rotated with the carpet's
    roll. Found with `tools/diff_step.py` (see §4).
57. **Statements left commented out.** `sub_590D0_595E0` had the flag reset `//*result = v3 & 0xF7;` as
    a comment: the sprite drawers set bit 8 on a sprite when they draw it and this per-frame pass clears
    it and advances the animation of the drawn ones - without the reset every sprite drawn once kept
    animating forever. Commented-out assignments inside decompiled bodies deserve a look at the asm.
58. **Uninitialised locals standing for registers.** `sub_30B30_30B70` (blur mode, `mod3D_8603 == 1`)
    blended the frame with `v1`, never assigned; the asm loads ebp from `blurBuffer_AE404` (0x30B82).
    Same in `sub_23AE0` (blended sprite overlay of the spell icon under the cursor): the low-res loop's
    write pointer started uninitialised (asm 0x23BCB: row pointer and write pointer are one register).
    To list candidates build once with `/w14701 /w14703` added to `remc1_options` (CMakeLists.txt):
    ~90 sites, mostly false positives (`result` on unused paths, `_old` copies, DOS runtime code).
59. **Bugs of the original that DOS forgave.** The rotated sprite path of `sub_2C410_2C450` does not clip
    rows at the screen bottom: in 640x480 with a strong roll a sprite far below the view (yRot 1536) is
    drawn ~1000 rows past the frame buffer. The emulated original writes the same address (diff harness),
    so it is not a port bug - DOS4GW let it scribble over other memory, the port crashed (level 6,
    `level_5_to_6_crash.txt`). The blitter now writes/reads the frame through `FbPut`/`FbGet`, which drop
    anything outside the buffer. When the harness shows the original faulting too, guard instead of "fix".
    **Record every confirmed original-game bug in `docs/original-bugs.md`** (public, linked from the
    README): symptom, cause with function names/offsets, the observation that proved it (recording,
    frames, harness result), a possible fix, and its status (kept / worked around / fixed). Keep that
    table current when a status changes. Ask the user before changing original behaviour ("fixed").
60. **Wrong operator in a hand edit.** `sub_277D0` (two mana balls merge) had `mana *= other` in two
    branches; the asm adds. A merged ball reached 917M mana, its owner's capacity (+136, rebuilt every
    tick as the sum of everything owned) wrapped negative and the owner's mana was clamped to it: "mana
    ran out and would not regenerate" (level 7, `level_6_7_mana_bug.txt`, frame 24369).
61. **`_DWORD` arithmetic on signed fields.** Hex-Rays' `*(_DWORD *)(a1 + 136) / 200` is an UNSIGNED
    division (and `*(_DWORD *)a > b` an unsigned compare); the asm used idiv / jle. Harmless until a value
    goes negative. `sub_132B0` (AI wizard mana) - grep `(_DWORD \*)(... + 13[2-9|40])` with / or <, >.
62. **Code bytes used as data.** `sub_20480` (castle guards' wandering) copied a 4-word table from
    0x1FF40 - in the original a table inside the code segment ({7000, 7000, 10, 7000}, turn weights);
    the port read `*(_DWORD *)sub_1FF40`, the machine code of its own function. Guards picked garbage
    turn weights and bunched on one side. Grep `(_DWORD \*)sub_` / `(_DWORD \*)loc_`; read the bytes with
    `tools/le_image.py` (`LeImage(...).read(addr, n)`).
63. **The wrong one of the two state structs.** `str_AE400_AE3F0` (game state) and `str_AE408_AE3F8`
    (session flags) both start with flag bytes, and a hand conversion swapped them.
    - Pause: `UpdateEntities_41780_41AC0` tested `str_AE400 +2` for the pause bit. P toggles
      `str_AE408 +2`, the byte the original tests (`mov eax, [AE408h]; test byte [eax+2], 1` at 0x417A4),
      so the world never paused; only the input and the other pause checks did.
    - Command line: `ProcessCommandLine_34DD0_35190` set the -demo / -custom / -network flags in
      `str_AE400 +1`, which nothing reads; the original sets `[AE408h]+1` at 0x35506-0x355CD.
    - Sweeps missed both: they need P pressed or those options. Find such sites by scanning the original
      for every `[reg+off]` after `mov reg, [0xAE408]` (capstone over `LeImage.code_range()`), then
      compare with the port's `str_AE408_AE3F8->` uses. The port's `str_AE400_AE3F0->var_0` (its first
      4 bytes) is now only used by a whole-struct copy.
64. **Array element struct wider than the original's.** The players' start positions
    (`playerStart_9177`, AE400+9177) are 6-byte `axis_3d`s: the original indexes them as
    `lea esi, [ecx + eax*2 + 0x23d9]` with eax = 3 * player (0x44D6F), and the eight record handlers
    `SetPlayerStart0..7` write 9177, 9183, ..., 9219.
    - The port declared an 8-byte struct (`axis_3d` + an invented `uint16 v_9181`), so
      `sub_44D30_45070` read player n's start 2n bytes too far.
    - Player 0 (the human) was right. Every computer wizard started at a mix of its own y/z and the
      next entry (level 15: red at tile (186,16) instead of (42,186)), with its castle there too.
    - The same `memset(..., 48)` beside it gave it away: 8 x 6 = 48.
    - Check: an array's element size (`sizeof(T)` x count) against the original's index arithmetic
      (`imul`/`lea` scale) and any memset/memcpy of the whole array.
65. **Flag constant ORed into one byte of a flag dword.** `sub_25CE0` and `sub_29780` had
    `var_29811_16.byte[0] |= 0x10002u` where the original does `or dword ptr [ebx+0x10], 0x10002`:
    the byte store kept the 0x2 and dropped the 0x10000 (+18 bit 0). Found as a lone one-byte
    `diff_tick` difference (`entity N +18: orig 1 port 0`). Grep for `byte[k] |= 0x` / `&= ~0x` with
    constants wider than 0xFF.
66. **Upstream hand edits that reorder a guarded call.** F2 (music off, `ProcessKeys_16B00`) cleared
    `musicOn_939CD` before `StopMusic_20E60_20E60()`, which returns at once when it is 0, so the music
    went on (`level23.recording`). The original clears it after the call (0x17260); the line came from an
    upstream commit (`317b7c1`), not from the decompiler. Lines with odd indentation (spaces among tabs)
    are often such edits: check them against `tools/orig_disasm.py`.
67. **Write through the wrong object after a call.** `SpellJarSpawnTick_55D30_56260` did
    `a1x->state += 2` where the original does `add byte ptr [eax+46h], 2` with `eax` = the entity the
    constructor just returned. Picked-up spells 7 and 10 stayed in their jar state, and when the player
    died they moved to 3t+3 (the next spell's owned state): red jars on the map that could not be picked
    up (`level26_spell_bug.recording`). The same function ORed `byte[0] & 0x40000` (always 0) into one
    flag byte instead of the dword (pattern #65). Found with `diff_tick` at the pickup frame
    (`+70: before 23 orig 21 port 23`).
68. **Statements commented out upstream.** `HealSpellTick_56270_567A0` had `//fix v1[33] = -cost;` where
    the original sets the caster's mana delta (0x56359), so healing cost no mana while the delta was not
    already negative. Found by `diff_funcs` (`entity 720 +132: original 24 port 15`) after a 2-byte
    `diff_tick` difference in the player's mana. Grep for `//fix` / commented-out assignments and check
    them against `tools/orig_disasm.py`.

The MC2 renderer in remc2 (`GameRenderOriginal.cpp`, e.g. `DrawSprite_41BD3`) is structurally the
same code as MC1's `sub_2C410_2C450`/`DrawSprite_*` and uses the same scratch offsets in the BScreen
(36960, 45920, 59360) — use it as the reference for renderer fixes.

### 6.3 DOS → platform mapping

| DOS mechanism | Replacement |
|---|---|
| VGA mode 13h / 640×480 SVGA, `0x3C8/0x3C9` palette | `VGA_Resize`, `VGA_Set_Palette` (6-bit DAC ×4), `VGA_Blit(begWscreen…)` → SDL INDEX8 surface → RGB texture |
| INT 9 keyboard ISR filling `pressedKeys_12EEF0_12EEE0[128]` / `lastPressedKey_12EF70_12EF60` | `VGA_InjectKey()` ← SDL key events / input scripts; SDL scancode → DOS set-1 scancode in `fixchar()` |
| INT 33h mouse callback | `MouseEvents(buttons, x, y)` → `UpdateMouseEventData_5BE72_5C382`; buttons: 1 move, 2/4 left down/up, 8/0x10 right, 0x20/0x40 middle |
| PIT channel 0 (divisor 9903 ≈ 120.5 Hz) + INT 8 ISR `sub_5A3E3` | `PortTimer_Start(divisor, sub_5A3E3)` in `sub_5A459_5A969` |
| HMI SOS digital driver (`hmidrv.386`) | `portability/port_sound.cpp` on SDL_mixer (§6.4) |
| HMI MIDI driver (`hmimdrv.386`) | `portability/port_music.cpp`: libADLMIDI + BW_MidiSequencer (§6.4b) |
| `in`/`out` port I/O (`__inbyte`/`__outbyte`) | `Port_InByte`/`Port_OutByte` macros near the top of `sub_main.cpp`: VGA DAC to palette, joystick port 0x201 reports "no joystick", others logged |
| DOS file API (`open`, `_dos_findfirst`, …) | `portability/port_filesystem.cpp` (`myopen`, `mycreate`, `my_findfirst`, …) |

Presentation: the window may be any size; the SDL renderer's logical size is 640×480 and every game
mode is stretched to that 4:3 area (as on a CRT), with SDL letterboxing to the window. SDL therefore
reports mouse coordinates in 640×480 logical units; `LogicalToGameMouse()` converts to game mouse
space (§4).

---

### 6.4 Sound (Phase 4, effects done)
- Banks: `SNDSn-v.DAT/.TAB` (RNC packed; `sub_5D070` loads `snds%d-%d`, 14 sets). TAB entries are 32
  bytes: `name[18]`, `+18` offset into the DAT, `+26` length, `+30` word. Variant `-0` = 11025 Hz,
  `-1` = 22050 Hz (used: `byte_939EC = 1`), `-3` = low memory. Samples are unsigned 8-bit mono.
- The game talks to the HMI SOS API (`sub_651F8`, `sub_6535C`, `sub_65965`, `sub_66118`, `sub_661A4`,
  `sub_66324`); these forward to `port_sound` (32 SDL_mixer channels, volume/pan, looping when flag
  `0x4000` is set with the count at `+12`). The "sample done" state the game polls is driven by timer
  ticks, not by SDL callbacks, so goldens are identical with sound on or off.
  `PortSound_ClearCache` runs when banks are reloaded (`sub_5D138`).
- `sampleDesc_939F0[64]`, the bank globals (`dword_CC154/CC1C0/CC130`, `CBF60/64/68`) and
  `word_CBFF0/word_CBFF2` were several IDA names for single objects (pattern 6.2.7): they are macros
  aliasing `snds00Tab_2854A1` / `music00Tab_289F60` / one array.

### 6.4b Music (AdLib, General MIDI and MT-32 done)
- Data: `MUSIC<set>-<variant>.DAT/.TAB`, RNC packed, loaded by `LoadMusicSet_5CEF0_5D400(set)`; variant
  `musicVariant_CBFEE` = 0 AdLib, 1 Roland, 2 General MIDI (set in `InitMusicDriver_43B20_43E60` from the card). TAB: a
  32-byte header, then one 32-byte entry per song (`+18` data offset, made absolute by `FixMusicTabOffsets_5CFA4`, `+26`
  length; `musicTrackCount_939D6` = song count). The songs are **HMI HMP** files (`HMIMIDIP`; 120 ticks/s = the
  driver's timer rate at song+0x38). Set 0: GAME1-3 (levels, chosen with `pseudoRand % 3 + 1` - so
  **music on changes the game**) and SETUP (menus, track 4); set 1: INTRO4-6 (level script opcode `B`).
- AdLib instruments: `DATA/INST.BNK` / `DRUM.BNK`, AdLib BNK v1.0 with 128 entries each (melodic =
  program, percussion = note; the drum's name-record flags byte is the note it sounds at). They are
  loaded into libADLMIDI with `adl_setInstrument` (HMI volume model, deep tremolo/vibrato).
- `port_music` replaces the HMI MIDI API at the decompiled functions: `MidiLoadSong_5EE22` init (call site in
  `PlayMusicTrack_5D290_5D7A0`), `MidiStartSong_5F398` start, `MidiRewindSong_5F529` reset, `MidiStopSong_5F420`/`MidiUnloadSong_5F355` stop,
  `MidiSongDone_5D7F0_5DD00` song done, `MidiSendData_5EDA0` send MIDI, `MidiResetChannels_6091D` reset driver. Songs are parsed by
  BW_MidiSequencer (MIT, compiled from libADLMIDI's source tree, loops off: the game restarts songs
  itself) and rendered in the SDL_mixer music hook.
- Danger music: `UpdateDangerMusic_20D00(1|2)` (called from the player's entity update) fades MIDI channels 3-5 with
  CC 7 from the HMI timer event `DangerMusicFadeStep_20BD0` (60 Hz in, 20 Hz out, steps of 2 to 126/0); the songs start
  those channels at volume 0. HMI timer events are `PortTimer_AddEvent` (run as ticks are applied).
- Determinism: "song done" is tick driven (`PortMusic_SongTicks`, the longest track); the card comes only
  from the setup/settings, never from the audio device. Recordings store it (`music <card>` line, see
  `docs/recordings.md`); scripts without one run with music off, so all older recordings/goldens hold.
  Verified: music-on replays of levels 1 and 8 give identical state traces with audio, silent and twice.
- General MIDI (card GENERAL/SBAWE32/WBLAST, and ROLAND without MT-32 ROMs), `--midi-synth auto|fluidsynth|system|opl`
  (`audio.midiSynth`); auto takes the first that works:
  1. FluidSynth on a SoundFont, if built and a SoundFont is set or found. `REMC1_FLUIDSYNTH` defaults to
     OFF on Windows: vcpkg's `fluidsynth` manifest feature (selected before `project()`) builds GLib,
     ~30 min the first time. Elsewhere it uses pkg-config, because Ubuntu's CMake package for it is broken
     without the `fluidsynth` program. Gain 0.3 matches the AdLib loudness.
  2. The Windows MIDI synth (`WinMidiSynth`: winmm `midiOut` on MIDI_MAPPER, i.e. Microsoft GS
     Wavetable Synth), not in headless runs or on SDL's dummy/disk audio driver.
     - It renders nothing, so a `Realtime()` synth gets its own sequencer thread: `Tick()` at 1 ms
       granularity with `timeBeginPeriod(1)`, instead of the SDL_mixer hook, whose 1024-frame buffers
       would quantise events to ~23 ms.
     - The music volume scales CC7.
     - The thread is joined via `atexit`, because the game exits without `PortMusic_Shutdown`.
  3. OPL3 with libADLMIDI's GM bank.
- Roland (card ROLAND, `--music-device mt32`): `Mt32Synth` on Munt's libmt32emu.
  - Build: munt 2.8.2, `mt32emu` subproject only, fetched like libADLMIDI, built static, C interface
    (`MT32EMU_API_TYPE 1`). `REMC1_MT32` (ON).
  - ROMs: `--mt32-roms <dir>` (`audio.mt32Roms`). Munt identifies files by SHA1, so names don't matter,
    and `mt32emu_add_machine_rom_file` merges MAME's split halves.
  - Model: `--mt32-model` takes a Munt machine id. auto = MT-32 1.07..1.04, bluer, then CM-32L, then the rest.
  - Card mapping: InitMusicFromSetup_3C570 gives the game ROLAND only with a complete set (`PortMusic_Mt32Available`),
    else GENERAL as before. A ROLAND replay without ROMs plays the Roland songs on the General MIDI
    synth (wrong sounds, game state unaffected).
  - What ROLAND changes in the game: only the song file. A level 12 replay gives identical state
    traces with GENERAL and ROLAND (11035 frames).
  - Songs: the level/menu songs (`MUSIC0-1`) are their own Roland arrangements; the intros are nearly
    the same as GM. No song has SysEx (channels 2-10, the MT-32's default parts), so the built-in
    MT-32 sounds play.
  - Level: about the AdLib level, no gain needed. Munt's default report handler prints every debug
    line; ours keeps it quiet unless `REMC1_DEBUG_MUSIC`.
  - Test: `Music.Mt32NeedsRoms`, which also checks `REMC1_MT32_ROMS=<dir>` when set.

### 6.5 Presenter seam (Phase 2)
`portability/Presenter.h` defines `IPresenter` (init, palette, present an 8-bit frame, resize,
fullscreen, mouse mapping, read-back). `SdlPresenter` is the SDL2 renderer implementation and
`NullPresenter` is used headless. An OpenGL presenter (palette texture + shader) can be added as a
third implementation without touching game code; a world-renderer seam (`IGameRenderer`, cf. remc2
`GameRenderInterface.h`) comes with the Phase 3 split.

### 6.6 Frame pacing
The game logic advances once per frame (`DrawAndEventsInGame_34530_348F0` runs one
`UpdateEntities_41780_41AC0` per frame) and the DOS original simply ran as fast as the CPU allowed,
so the port caps the frame rate: `PortTimer_LimitFrame()` in the blit functions, cap switched by
context (`GameLoop_34610_349D0` sets `maxGameFps` = 30 like remc2, menus/movies use `menuFps` = 70),
and `WaitForBeam_5CC54_5D164` waits on an emulated 70 Hz vertical retrace (`PortTimer_WaitRetrace`).
With vsync, the limiter paces in whole refreshes when refresh/N is within 3% of the cap (60/120 Hz),
otherwise it uses time (144/165 Hz) - a time-based wait right after a blocking present wakes just
after a refresh and costs a whole extra refresh per frame. Windows needs `timeBeginPeriod(1)` or
every sleep is 15.6 ms. Pacing is a no-op for deterministic runs and replays.
Measure with `--frame-stats` (headless or `--no-vsync`: in a locked/hidden session Windows throttles
vsync'd presents to ~33 Hz, so windowed vsync numbers there are meaningless). Before this, fixed
100 ms/20 ms/10 ms "fix it" sleeps gave 8-10 fps.

## 7. 32-bit addresses in a 64-bit program

The decompiled code keeps addresses in 32-bit `int`s (~8300 casts), passes addresses of locals around
as ints, and packed DOS-layout structs embed 4-byte pointer slots (entity +156/+160, lists, `.TAB`
entries relocated in place). The 64-bit port (branch `port/64bit`, merged as PR #2, 2026-10) keeps all of that:

- **Ints are offsets** (`engine/ptr32.h`): `PTR(x)` turns an int into a pointer, `OFF(p)` a pointer into an
  int. 512 MB windows with their own bases, `PTR(x) = g_ptrBases[x >> 29] + x` (0 stays null): offsets
  0..512 MB from `g_ptrBase` (the executable's statics), 512 MB..1 GB the game arena wherever the OS mapped
  it, 1..2 GB the game thread's stack (registered on first use). `OFF()` aborts with a message for anything
  outside them. (The arena can't sit at a fixed distance from the
  executable: Linux randomises the brk heap start over the first GB after it, already mapped before main.)
- **Game memory comes from the arena** (`Port_ArenaAlloc/Free`: `malloc_42540_42880`, `malloc_425C0_42900`,
  `MallocGuarded`, the `(int)malloc` sites). It sits at offset 0x20000000 (where the old 32-bit build had it)
  with a deterministic best-fit allocator, so game objects get the same "addresses" in every build.
  That matters: the code turns addresses into numbers that end up in the game state (e.g.
  `(target - entity array) / 164` with a null target), so traces only match across builds with a fixed layout.
- **Pointer fields of packed structs are `ptr32<T>`** (4-byte offset, converts to/from `T*`; `extptr32<FILE>`
  for pointers outside the windows); `tools/port_symbols.py` recognises both by their PDB type names.
  Port-side tables accessed only by name (`Pathstruct`, `TypeStrFont`) keep real pointers.
- **Function-pointer tables**: `PORT_FN(fn, Sig)` puts a converting thunk in the table when `fn` takes/returns
  int addresses but the table type uses pointers (`tools/x64_thunk_tables.py`); the harness maps a thunk to the
  function it wraps.
- **Harness on 64-bit** (`tools/emu.py`): snapshots are written in the offset space (statics at 4 MB + RVA,
  the arena at 512 MB, the stack at 1 GB; `meta.txt` has `pointer_size 8` and the window bases), so the
  original runs in the 32-bit Unicorn machine as before. Port globals holding real 8-byte pointers are
  projected into the original's layout (`PortSymbols.project32`). Port code runs in a second, 64-bit machine
  with the same memory at the real addresses (relocated sections, x64 import stubs, a fake TEB for
  `__chkstk`, x64 calling convention with pointer args converted per the PDB signature); pages written in the
  32-bit machine are synced before each port call (tools must write through `Machine.write`), the port's
  writes come back in offset space.
- Mechanical tools: `tools/x64_rewrite_casts.py` (wraps the casts MSVC flags with C4312/C4311 in PTR/OFF),
  `tools/x64_thunk_tables.py`.

64-bit pitfalls found so far (each one broke a recording; grep for more of the same shape):
- `sizeof(T*)` as a data size: ring iterator stepped 8 bytes per 4-byte entry (`NextRingOffset_114B0`),
  memsets of ptr32 lists cleared 160 instead of 80 bytes.
- Reading a pointer from a 4-byte slot as `*(T **)PTR(x)` / `**(T **)p`: read the slot as uint32 and PTR() it.
- Buffers handed to game code must come from the arena (or statics/stack), not the C++ heap: tests use
  `remc1test::GameBytes` (`tests/support/test_support.h`).
- Unsigned 32-bit offsets added to real pointers: `ptr[uint32]` with 0xFFFFFF20 meaning -224 wraps on 32-bit
  but adds ~4 GB on 64-bit (`dword_B3EA0_B3E90x` screen offset table, now int32): the sprite vanished.
- Frame-pointer tricks: callees reading the caller's frame through an int (`sub_3F76D_3FAAD`, `sub_72652`):
  pass a real `uint8_t*` to a local buffer.
- Relocations (`Tab_0 += base - nullptr`): do them on offsets (`OFF()`), not raw pointers.
- DOS interrupt stubs (`int386`, `segread`) take the register blocks as pointers now.

Debugging 64-bit differences: `tools/regress.py` (first frame with a state/image difference), then
`--state-dump <frame>:<file>` from both builds + `tools/diff_state.py` (names the entity/field). For image-
only differences, hash the frame buffer at every draw call of that frame in both builds and compare (found
the vanishing sprite in one step). x64 crashes: run under the x64 cdb (`-c "sxe av; g; .ecxr; k"`).

**TODO before the code split/organisation phase (user decision 2026-10-19): replace the offset scheme with
real typed pointers (remc2 style)**: convert raw-offset accesses to named fields first, then give structs real
pointers, with serialisers for the DOS layouts read from/written to disk. The offset scheme is a bridge.

## 8. Testing

Run everything with `REMC1_DATA_DIR=C:/mc1data ctest --preset win-x64-debug` (VS-bundled ctest).
Without `REMC1_DATA_DIR`, tiers 2 and 3 report "skipped".

1. **Tier 1 — no game data** (`tests/test_layout.cpp`, `test_layout_names.cpp`, `test_platform.cpp`,
   `test_render.cpp`, `test_music.cpp`):
   - struct layout: `tests/generated/layout_cases.inc` is generated by `python tools/gen_layout_tests.py`
     from the offset facts in `engine/Basic.h` (field `name_<off>` + `//<off>` comment, `// len N`);
     plus hand-written offsets of the 232713-byte game-state struct. Regenerate after editing headers.
   - `FixPath`, SDL→DOS scancodes, key injection, deterministic/real-time timer, input scripts.
   - captured rasteriser cells (`test_render.cpp`), music (`Music.*`, e.g. `Music.Mt32NeedsRoms`).
2. **Tier 2 — game data** (`tests/test_data.cpp`): every `.DAT/.TAB/.PAL` on the CD is decoded with the
   original decompiled RNC decoder, the port decoder and the swars decoder; all must agree, and the
   output hash must match `tests/goldens/data_hashes.txt`.
3. **Tier 3 — replays** (`tests/replays/*.txt`, run by `tools/replay_test.py`, registered as
   `replay.<name>` in CTest): headless + deterministic run of an input script with a fresh copy of
   `CARPET.CD` (saves removed, English preset); per-frame framebuffer+palette hashes must equal
   `tests/goldens/replay_<name>.txt`. Debug and Release produce identical hashes. Replays: `boot_intro`,
   `level1_fly`, `castle_upgrade`, `pause`. These goldens (with tier 4) are what will prove the Phase 3
   split preserves behaviour; they already did for the 64-bit port.
4. **Tier 4 — state traces** (not in ctest; needs the user's bundles): `python tools/regress.py [-j N]`
   replays every bundle in `<data>/recordings` and compares whole-game-state + image hashes per frame
   with the bundle's `trace.txt` (first difference reported); `tools/diff_state.py` names the field.

**Updating goldens** (`REMC1_UPDATE_GOLDENS=1` for gtest, `replay_test.py --update` for replays) is
only legitimate when a change is *meant* to alter output (a decompilation fix). Look at the frames
first (`--dump-frames` + `contact_sheet.py`), say why in the commit message, and never update them
as part of a refactor (split, 64-bit) — there a golden mismatch is the bug.

Only hashes and input recordings are committed — never game data.

---

## 9. Bring-up log (Phase 1)

What worked at the end of Phase 1 (Windows x86 Debug/Release; today x64): language selection, sound-setup book, intro FMVs (skippable),
main menu → globe → level 1; flying forward, turning/banking, casting with both hands; ASan-clean
for the scripted runs in `tests/replays` (deterministic, 900 frames in level 1).

Fixed so far (all marked `// PORT:` or described here):
- RNC unpack crash: debug probe reading 8 KB past the buffer (pattern 6.2.3).
- Sound init crash: HMI driver executed `in al,dx` → drivers disabled (6.2.5); `_UNKNOWN` buffers (6.2.2).
- Timer: uninitialised `TimerClass*`, leaked threads, wrong tick rate → `port_timer` (120.5 Hz).
- Entity damage code wrote through `Type_160* + byte offset` (6.2.1); 70 more such sites on
  `TColor*`/`TypeTab*` globals (palette effects, sprite dimensions).
- `MODIFY_SETTINGS`/`FIX_MOUSE` debug switches disabled (6.2.4).
- `FixPath` missed lowercase `c:/carpet.cd`.
- In-place RNC decoder overran destination buffers → decode from a padded copy (verified by tier 2).
- ASan round (patterns 7–14): colour lookup / trig / FMV tables, `*WScreen` size, list-head array,
  `axis_3d` stack pairs, `begBscreenAdress` missing `&` (7 sites), sprite blitter source/destination,
  sprite clipper `while(hwIndex);`, control flow and signedness, `byte_906DC/E2` draw-mode tables,
  ~90 more typed-pointer byte offsets on parameters/locals, two timer busy-waits.

- Camera pitch spin (signed pitch, 6.2.19), minimap row table overflow (`word_12C1F0/F2` merged).
- Castle spell crash (6.2.15), `Type_160` layout (6.2.16), sprite clip order (6.2.17).
- Rolled sprites floating/rotating ("villagers fly into the air"): 16-bit carry (6.2.18).
- Debug-only aborts: `/RTCu` uninitialised `result` (6.2.20), `allert_error()` divide-by-zero probe
  on quit, joystick port I/O (6.2.21). User recordings `play2`/`play3` replay to the end
  (`--no-sound`, Debug and Release).
- Water checkerboard (6.2.30). Spell-map cursor trail (6.2.32), spell hotkeys (6.2.33), crashes at the
  end of level 1 with sound on (6.2.34, 6.2.15 again for `dword_38C9F/38CA3`).
- Continuing to level 2 crashed: the main-menu action table lacked entries 11-12 (truncated
  initializer, pattern 6.2.8); added `MenuContinueGame_4FF00`.
- Open: the main-menu attract demo (after ~2 min idle in the menu) still crashes.
- Score board without text (6.2.38); level-2 worm sprites broken after playing level 1 (6.2.37).
- Speed spell crashed on cast (6.2.39). Level 3 guards floated off the ground (6.2.40).
- 640x480 mode (R key): mouse dead (6.2.41), crash drawing rolled sprites (6.2.42).
- Level performance screen text drawn as black dots (6.2.43). Note it is drawn over pperf.dat, a
  still that looks like a video frame.
- Level 4 crash after dying (6.2.44). Level 5: intermittent crash near the end (6.2.45).
- AI wizard idle (circling the map corner) (6.2.46). Castles stopped at level 4 (6.2.47).

How the level's actors work (useful for "X never spawns" reports):
- Level records (`str_193795.str_1072[]`, `Type_1090`): `data_0` class, `data_2` type, `data_4/6` tile.
  `data_8 == 0` records are created at load (`sub_37560_37920`), `0xFFFF` class-10 records are features
  (`GenerateFeatures_36430_367F0`), other values are trigger groups spawned by `sub_37440_37800(group)`
  when the human wizard (model 0 in the wizard list) touches the matching trigger entity
  (`sub_59A80_59F90` & co., `sub_5A090_5A5A0`).
- Class 3 types 4..11 only store wizard start positions; wizards are created per player
  (`sub_44D30_45070`): model 0 = human, model 1 = computer (state 1, `sub_13170` AI, mode byte at
  player+415).
- Class 10 type 39 = mana ball (mana at +140); class 5 type 5 creatures (state 31, `sub_1BD20`) seek mana
  balls and turn mana into class 10 type 52 spawn timers (state 0x38) that raise another type 5.
- `level_2_bugs.txt` (user recording) desynchronises at frame 1590 in level 1 with every build tried
  (deterministic, independent of audio and of the 6.2.36-38 fixes; the only input around it is fast
  mouse motion). The replay still reaches level 2. Cause unknown - candidates: an uninitialised read
  that differs between live and scripted input paths, or input the recorder does not capture (middle
  button, special keys). Splitting `Port_GameStateHash` into seed/entity parts would narrow it.
- Top-of-screen glitch when pitching down: in the rasteriser's "second vertex lowest" path
  (`boolLabel124`) with two vertices above the top edge, `slope * rows + x3 << 16` meant
  `(slope * rows + x3) << 16` (4 mode groups) and one group tested the clip after modifying the row
  count. The triangle smeared spans across the whole top of the screen. Captured cells are in
  `tests/test_render.cpp` (`CapturedTopEdgeCells`).
- Castle upgrade missiles passed through the castle (6.2.24); spell auto-aim/possession found no
  targets (6.2.25); being hit flashed black instead of red (`byte_B700D/E/F` = `byte_B7010-3/-2/-1`,
  6.2.7). Regression replay: `tests/replays/castle_upgrade.txt` (a user recording).
- Castle spell built the castle at the wrong place (6.2.22); post-level info FMV hung with sound on
  (6.2.23); `byte_38C97[...]` castle-level reads in `sub_44D30_45070` now use the game-state field.

Known open issues:
- **Tools are platform-agnostic:** paths come from `tools/remc1_paths.py` (`find_port_exe()` = $REMC1_EXE or the
  newest `remc1[.exe]` under `build/<preset>/`, `default_data_dir()`, `original_exe()`). Never hard-code
  `build/win-x64/...` or `.exe` in new tools. Windows-only for now: `asan_run.sh` (win-x64-asan preset) and the
  diff harness (`port_symbols.py` reads the MSVC .pdb through dbghelp); `debug_run.sh` uses gdb/lldb elsewhere.
- **Original-game bug, kept on purpose (user decision 2026-10-18): stuck explosion after a worm segment
  dies** (`docs/original-bugs.md` #1). Worm segments (class 5, model 3) form a chain: +52 = leader index,
  +54 = follower index. `UpdateFollower_1A390(a1x, a2)` (segment tick, `a2` = the segment type's base state,
  18 for these) takes `v3x = entity[a1x+52]` as the leader without validating it. When the segment's life
  drops below 0 (or it is hit by a wizard, damager class 3) it sets `v3x+146`, clears `v3x+52` and calls
  `sub_424F0_42830(v3x, a2 + 2)` (state 20). If the real leader died earlier its slot can already hold
  another entity: in `stuck_explosion_sprite.txt` (level 15) segment 174 dies at frame 36216, an explosion
  (10/0, `sub_24F60`) reuses slot 174 at 36217, and at 36218 segment 157 (still +52 = 174) dies and puts
  the explosion into class-10 state 20 = handler 0x262C0, a bare `ret` (port: `sub_262C0`). The sprite
  never changes again. `diff_tick` (ticks 36216-36218, 36480-36650) and `diff_call` (draw at 36600) match
  CARPET.EXE exactly; `diff_tick --watch 58401` (entity 174 +70) shows the original writing 0x14 at
  eip 0x424F8 via 0x1A52B (UpdateFollower) <- 0x19614 (`sub_19550`) <- 0x1B57C (`sub_1B570`).
  Replay needs a build *without* the `CreatureShootAt_1AA40` fix (that fix changes level 15 from frame
  660): check out `sub_main.cpp` from before commit "Fix skeletons shooting into empty space", build,
  copy `build/win-x64/RelWithDebInfo` elsewhere and pass `--exe` / `REMC1_PORT_EXE`.
  **To fix later:** in `UpdateFollower_1A390`, before touching `v3x`, require `v3x->var_u8_29859_64 ==
  a1x->var_u8_29859_64 && v3x->var_u8_29860_65 == a1x->var_u8_29860_65` (a live segment of the same
  creature); otherwise only clear `a1x+52`. Apply it to both damage branches and to the `switch` on the
  leader state (it reads `v3x->var_u8_29865_70`). Mark it `// PORT:` as a deliberate deviation, update the
  status in `docs/original-bugs.md`, and expect `diff_tick` to show that one difference in such ticks.
- **Bug recordings are bundles** (`<name>.recording/` = `input.txt` + `saves/`, `tools/rec.py record|play`,
  `docs/recordings.md`). Replays of a bundle start from the saves the session started with.
- Fixed 2026-10-18: `CreatureShootAt_1AA40` aimed shots from the shooter to itself (heading/pitch 0) instead
  of at target `a2` (asm `ebp = a2 + 48h`): level 14 skeletons fired into empty space. Stepwise-only
  differences in tile-list links (+20/+22, `UpdateFireball_52B30_52E70`) after it were a stepwise artifact:
  the whole tick matched end to end.
- **64-bit port done (2026-10-19, branch `port/64bit`):** x64 and arm64 only. Before the fix switches
  were removed, Windows x64 (Debug + Release), Linux GCC (Ubuntu 22.04/24.04, Debian 12, Fedora, Arch in
  Docker) and Linux Clang reproduced the 32-bit reference traces exactly (state and images) on the
  user's 11 level recordings; the harness gives identical results on 32- and 64-bit snapshots;
  `win-arm64` builds and links (not run). The temporary switches are gone and their four fixes permanent
  (commit 9e9f068). Merged into `main` as PR #2. The user then re-recorded levels 1-11 as bundles with
  traces (`regress.py --update` from a win-x64 build); see §4 "User recordings" for their state now.
  - Moving the repository: the build folders keep the old source path in `CMakeCache.txt` and every
    build then fails ("CMakeCache.txt directory ... is different"); run `cmake --preset <preset> --fresh`.
    Claude Code sessions are stored per project path (`~/.claude/projects/<path-slug>/`).
  - Docker Desktop keeps its disk image on the system drive; if that drive fills up, Docker's storage
    turns read-only - recover with `wsl --shutdown` and a Docker Desktop restart.
  - Linux pitfalls found: `long` is 8 bytes (`_BitScanReverse` into an `unsigned int`); `(char*)"..."`
    rows in a `char[][]` initialiser become single chars under GCC; case-sensitive paths (`FixPath` runs
    `casepath`, which fixes the case of the existing leading components); data opened "rb+" fails on a
    read-only install (DOS 0x200 = read-only); Ubuntu's default `_FORTIFY_SOURCE` aborts on the deliberate
    cross-field writes (`-U_FORTIFY_SOURCE`); out-of-array reads that MSVC and GCC -O0/-O2 lay out
    differently (`sub_360C0_36480`, original-bugs.md #6) - a Debug/Release difference on one compiler
    points there. Container: `docker run -v <repo>:/src:ro -v <datadir>:/data:ro`, build in /build.
- Investigated 2026-10-25, kept (user decision: no deviation): the big crab at the end of level 22
  (`level22.recording`, entity 995, frame 12270) dropped no mana. Original bug, `docs/original-bugs.md`
  #9: its own lightning filled the entity table (277 class 9 type 9 bolt pieces, 66 new ones that
  tick), so the death code's `CreateEntity` calls (mana ball from `sub_27690` 0x276E5, class 10 type 1
  from `sub_1A800` 0x1A836) got null from `NewEvent_372C0_37680` and the mana was lost. Ticks
  12269-12274 match the original byte for byte. Method worth reusing: when something "should have
  spawned", hook `CreateEntity_373F0_377B0` (0x373F0) in the emulator and print its return value at
  the caller's return address; hook `NewEvent` (0x372C0) to count new entities per caller.
- Fixed 2026-10-22: crash on level 19 (`level19.recording`, divide by zero in the HUD, `sub_22E50`).
  Original bug, `docs/original-bugs.md` #8: with the entity table full, `NewEvent_372C0_37680` recycled
  stale recycle-list entries, here the player's castle balloons; the HUD then divided by the new
  entity's max mana 0. The port now skips stale entries (first triggers at tick 9127 of the recording,
  so the recording replays exactly only up to there). Also pattern #65 (flag OR truncated to a byte)
  and the harmless `JUMPOUT(0x36079)` in `sub_36080_36440` (a shared epilogue) removed.
- Fixed 2026-11-15: computer wizards chose other mana balls than the original (pattern #50):
  `sub_15080` and `sub_145B0` compare `50000 - maxMana/10*k/255` signed (`idiv`, `jge`/`jle`), the
  port unsigned via `_DWORD`, so targets owned by a wizard with a lot of mana took the other branch.
  Found in `possession_spell_bug.recording` (frames 56995, 59853), which now desyncs from frame 18360.
- Fixed 2026-11-04: the heal spell cost no mana while the caster's mana delta was not negative (pattern
  #68; `level30.recording`, frame 46500).
- Fixed 2026-11-04: flaky access violations when a replay or game ended with AdLib music: only realtime
  synths registered `StopPlayer` at exit, so the static destructors freed the libADLMIDI player while
  SDL's audio thread was still in `MusicHook`. Seen as 3 crashes in 66 parallel replays.
- Fixed 2026-11-04: crash (HUD divide by zero) at the end of `level30.recording`: a volcano eruption
  removed a castle balloon through a stale index (original-bugs #11).
- Fixed 2026-11-01: red spell jars that could not be picked up after the player died (pattern #67;
  `level26_spell_bug.recording`, spells 7 and 10). Pickup and death ticks (frames 2179, 2655, 2871, 13969)
  now match CARPET.EXE; the recording desyncs from frame 2190 on, as expected.
- Fixed 2026-11-01: F2 did not stop the music (pattern #66; `level23.recording`, frame 22453).
- Fixed 2026-10-22: computer wizards (players 1-7) started in the wrong place in every level (pattern
  #64; `level15.recording`: red wizard and castle at tile (186,16) instead of (42,186)).
- Fixed 2026-10-21: P paused only the input; the world went on (pattern #63).
  - Checked with the harness on `tests/replays/pause.txt` (new replay test):
    - the entity tick before, during and after the pause matches CARPET.EXE byte for byte (before the
      fix the original changed 4 bytes in a paused tick, the port 1065);
    - the calls `17C20` / `3C9D0` / `5A560` and the whole `frame` draw match while paused;
    - `590D0` / `415C0` are skipped while paused, in both.
  - While paused only the distant water's shimmer and the colour of the PAUSED! text change on screen.
- Fixed 2026-10-19: castles built on / grown into impassable walls (level 12,
  `<data>/recordings/level12wallbug.recording`): sign tests on `uint8_t mapAngle` (pattern #50
  variant). With the fix the castle missile fizzles at the wall as in the original (`diff_tick` ticks
  742-747 match; the pre-fix exe differs in 744-746: the original sets missile +16 = 1030 and spawns the
  fizzle effect). `tools/emu.py` now maps page 0 read-only (zeroed): the original reads DOS low memory
  through null entity pointers (`AiApproachTarget_15470`, original-bugs #3), which used to stop whole-tick
  diffs whenever an AI wizard was building a castle.
- Longer play (other levels, enemies, death, save/load) not yet exercised under ASan.
- During forward flight the carpet holds altitude while terrain drops away, so the view becomes mostly
  sky; probably original behaviour, unverified.
- `Type_156::v_22` is misnamed (it is at offset 24); harmless.
- `int386(0x33, …)` mouse calls are stubbed (return 0); mouse range/visibility calls do nothing.
- Areas the upstream author listed as unfinished (`dev-info.txt`): terrain/sky/particle drawing
  (`DrawSkyTerrainParticles_2A700`, `sub_2C410`, `sub_2FC50`, `DrawWorld_30D90`), entity movement
  (`sub_455D0`, `sub_41C70`), `sub_1BD20`.

---

## 10. Gotchas (append as you find them)
- `std::max`/`std::min` collide with `<windows.h>` macros (pulled in by `port_filesystem.h`): write
  `(std::max)(a, b)`.
- CMake's VS generator does not define `UNICODE`; call the `W` Win32 APIs explicitly.
- `CMakePresets.json` expects `VCPKG_ROOT`; `build.sh` sets it to the VS-bundled vcpkg.
- The game's own CLI parser aborts on unknown arguments — keep port options `--double-dash` and
  stripped in `InitSettings`.
- Frame-based input scripts are not time-exact in real-time mode (the game logic runs on the timer
  thread's ticks); use `--deterministic` when exact reproducibility matters.
- stdout is fully buffered when redirected, and ASan/crashes kill the process before it is flushed:
  temporary diagnostics must use `fprintf(stderr, …)`.
- MSVC intrinsics: the decompile defined `_disable`/`_enable` (CLI/STI) which conflicts with `/Oi` in
  Release — they are now `port_cli`/`port_sti` no-ops.
- Agent tooling: backslash escapes inside bash heredocs fed to Python may be mangled (`\n`, `\b`,
  `\x00` turned into real control bytes). Use the Edit/Write tools for C/C++ string literals.
- `sub_main.cpp` line endings may be LF or CRLF depending on which tool touched it last; scripts must
  detect (`'\r\n' in text`) rather than assume. Git normalises on commit.
- `allert_error()` (`utilities/Compare.cpp`) marks code paths the upstream author had not verified
  ("test it"). It used to divide by zero; it now prints the caller address once per site. Resolve the
  address with cdb (`ln <addr>`) when you want to know which path ran.
- Git Bash: convert job/temp paths with `cygpath -u`/`-m`; a raw Windows path in an unquoted variable
  loses its backslashes. A 0-byte log plus exit code 127 from `remc1.exe` meant a crash in a
  Debug-only check (stdout never flushed): rerun under `tools/debug_run.sh`.
- A hung game is detected by the watchdog in ≤20 s for automated runs — don't add long shell
  timeouts and wait; read the stack it prints.
