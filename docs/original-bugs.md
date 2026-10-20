# Bugs in the original game

Bugs found in Magic Carpet's own code (the DOS `CARPET.EXE`), not introduced by the port. Each was
confirmed by running the original machine code on the same game state (`tools/diff_tick.py`,
`tools/diff_call.py`): the original behaves the same way.

Status:
- **kept**: the port behaves exactly like the original;
- **worked around**: the port avoids a crash or memory corruption that DOS tolerated, and otherwise
  behaves like the original;
- **fixed**: the port deliberately behaves differently (marked `// PORT:` in the code).

| # | Bug | Where | Status |
|---|-----|-------|--------|
| 1 | Stuck explosion after a worm segment dies | `UpdateFollower_1A390` | kept |
| 2 | Rotated sprites drawn outside the frame buffer | `sub_2C410_2C450` | worked around |
| 3 | AI wizard measures its distance to a missing target | `AiApproachTarget_15470` | worked around |
| 4 | Reads/writes through null or stale pointers | several (see below) | worked around |
| 5 | Level performance screen draws with a freed font | `ShowLevelPerformance_4E5B0_4E8F0` | worked around |
| 6 | Terrain smoothing reads in front of the map on its first row | `sub_360C0_36480` | fixed |

## 1. Stuck explosion after a worm segment dies (kept)

**Symptom:** an explosion sprite stays in the air forever, e.g. near the end of a level where a worm
was killed (seen on level 15).

**Cause:** worm segments are a chain. Each segment keeps its leader's entity index at entity offset
+52 (`var_u16_29847_52`) and its follower at +54. When a segment's life drops below zero,
`UpdateFollower_1A390` (the segment's update) sets its *leader* to the dying state
(`sub_424F0_42830(leader, a2 + 2)`, state 20 for these segments) and clears the leader's +52. It
never checks that the leader still is a segment. If the leader died in an earlier tick, its slot is
free, and the game may have reused it, e.g. for an explosion (class 10, model 0) created in the same
fight. The explosion is then put into state 20, whose handler (`0x262C0`) is an empty `ret`: it
never changes again and stays drawn.

**Observed:** frame 36216 of the level 15 recording: segment 174 dies (life -2200) while segments
157 and 175 still name it as their leader. Frame 36217: slot 174 is reused by an explosion. Frame
36218: segment 157 dies and sets entity 174 (now the explosion) to state 20. Every tick and the
drawing match `CARPET.EXE`.

**Possible fix:** in `UpdateFollower_1A390`, only change the leader if it still is a creature of
the follower's class and model (and alive), and otherwise just clear the follower's +52. The same
applies to the second branch (follower hit by a wizard, `class == 3`) and to the leader link read in
the `switch` on the leader's state. Mark the change as a deliberate deviation with `// PORT:`.

## 2. Rotated sprites drawn outside the frame buffer (worked around)

The rotated-sprite path of the sprite blitter (`sub_2C410_2C450`) does not clip rows against the
frame buffer when a sprite is very close to the camera in 640x480, and writes outside the screen
buffer. DOS4GW let this scribble over neighbouring memory; in the port it crashed (levels 5 and 6).
The screen buffers have guard bands (`MallocGuarded`) and the blitter's frame accesses (`FbPut` /
`FbGet`) drop anything outside the buffer. What is drawn on screen is unchanged.

## 3. AI wizard measures its distance to a missing target (worked around)

`AiApproachTarget_15470` computes the distance to its target (`a2 + 0x48`) before checking whether
there is a target; `AiBuildCastle_138F0` calls it with none, so it reads address 0x48. DOS ignored
the read and the result is unused. The port skips the measurement without a target (level 7 crash).

## 4. Reads/writes through null or stale pointers (worked around)

DOS4GW maps low memory, so the original silently read/wrote through null or stale pointers, e.g. the
animation sequence at the end of a level and the player slots of freed or empty wizards in the
main-menu demo. The port guards these with `// PORT:` checks that mirror what the data means (e.g. "is
a live wizard") and otherwise behaves the same.

## 5. Level performance screen draws with a freed font (worked around)

`ShowLevelPerformance_4E5B0_4E8F0` frees its font (`sfont2`) when a key is pressed and then draws the
screen once more with it. DOS's memory pool kept the data, so nothing was visible; the port frees the
font after the loop instead.

## 6. Terrain smoothing reads in front of the map on its first row (fixed)

`sub_360C0_36480` smooths a tile's height only if the terrain types of the tiles at -257, -256 and -1
(the neighbours above-left, above and left) and of the tile itself are all "open" (type 0..5 or
above 0x22). The original computes those indices as `(uint16)tile - 257` etc. in 32 bits
(`mov dl, [ebx + 0xcc0df]` with `ebx` the 16-bit tile index), so for tiles on the map's first row
(tile < 257) it reads up to 257 bytes in front of the terrain map: DOS variables of the sound code.
The 3x3 averaging loop right after it wraps around the map with 16-bit arithmetic, which is clearly
what was meant. Found from the disassembly while porting to Linux: the bytes in front of the array
depend on the compiler's layout, so GCC builds smoothed different first-row tiles than MSVC builds.

The port wraps those three reads around the map, like the averaging loop. (During the 64-bit port it
read what the 32-bit MSVC build found there, the end of `mapEntityIndex`, to keep the old recordings
valid.)
