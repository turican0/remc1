# Bug recordings

A recording captures your input (keys, mouse, timer ticks) so a bug can be replayed exactly. Most
sessions start by loading a saved game, so each recording is a bundle that also keeps the saves you
started from:

```
<name>.recording/
    input.txt   the input (made with remc1 --record-input)
    saves/      copy of <data>/CARPET.CD/SAVE taken when the session started
```

## Record
```bash
python tools/rec.py record recordings/castle_bug        # creates recordings/castle_bug.recording/
```
The game starts normally on your install; play until the bug shows, then quit. Saving and loading
during the session works as usual. `--force` replaces an existing bundle.

## Play back
```bash
python tools/rec.py play recordings/castle_bug.recording              # window, normal speed
python tools/rec.py play recordings/castle_bug.recording --headless   # no window
```
Playback runs on a temporary copy of the install with the bundle's saves (your own saves are not
touched) and ignores live input. If the game drifts from what was recorded, it prints
`DESYNC at frame N`.

For frame dumps and contact sheets use `tools/run_recording.py <bundle> --out <dir>`; it uses the
bundle's saves the same way. Older recordings that are a plain `.txt` still work (`--keep-saves`
uses your install's current saves).

Both tools take `--data <dir>` (default `$REMC1_DATA_DIR`, else `C:/mc1data` on Windows and
`~/mc1data` elsewhere) and `--exe <path>` (default `$REMC1_EXE`, else the newest build under
`build/`, release builds first); arguments after `--` go to remc1.
