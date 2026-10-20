#pragma once
#include <cstddef>

// Memory snapshots for the differential test harness (tools/diff_tick.py): with --snapshot-frame N the
// game dumps its writable memory right before and right after the entity tick of presented frame N
// (with a list N1,N2,... one subdirectory f<N> per frame),
// then exits. The harness replays the tick with the ORIGINAL machine code (CARPET.EXE under Unicorn) on
// the "before" image and compares the result with the "after" image.

// Called at the start (before=true) and end of UpdateEntities_41780_41AC0. state/settings are the game
// state (str_AE400_AE3F0) and settings (str_AE408_AE3F8) blocks, recorded in meta.txt.
void MemSnapshot_Tick(bool before, const void* state, size_t stateSize, const void* settings, size_t settingsSize);

// Same for any other function selected with --snapshot-what <what> (e.g. "draw" = DrawWorld_30D90):
// dumps around one call of it in frame N and records its original address and arguments in meta.txt
// ("call <ida address> <arg>..."), for tools/diff_call.py.
void MemSnapshot_Call(const char* what, bool before, unsigned idaAddress, const int* args, int nargs);
