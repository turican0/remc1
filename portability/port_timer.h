#pragma once
// Replacement for the DOS timer interrupt (INT 8 / PIT channel 0).
//
// The game reprograms the PIT (divisor 9903 -> ~120.5 Hz) and installs an ISR that increments
// tick counters. The ISR is never run asynchronously: ticks are applied on the game thread at
// *sync points* - every presented frame (PortTimer_OnFrame) and every busy-wait iteration
// (PortTimer_Yield). How many ticks are applied at a sync point depends on the mode:
//   * real-time:     ticks that came due on a steady_clock since the last sync point
//   * deterministic: a fixed number per frame, one per yield (tests, --deterministic)
//   * replay:        exactly what a recording logged for that sync point (--input-script with
//                    "T" lines, written by --record-input) - replays are bit-exact
// Game code that busy-waits on a tick counter must call PortTimer_Yield() inside the loop.

#include <cstdint>
#include <cstdio>
#include <map>

using PortTimerCallback = void (*)();

// divisor: value programmed into PIT channel 0 (1193182 / divisor Hz). Restarts if already running.
void PortTimer_Start(uint32_t divisor, PortTimerCallback isr);
void PortTimer_Stop();
bool PortTimer_Running();
// Stops the clock thread (registered with atexit automatically).
void PortTimer_Shutdown();

// Mode selection (before PortTimer_Start).
void PortTimer_SetDeterministic(bool enabled, int ticksPerFrame);
bool PortTimer_IsDeterministic();
// sync point index -> ticks to apply there (from a recording)
void PortTimer_SetReplayLog(std::map<uint64_t, uint32_t> log);
// Writes "T <syncIndex> <ticks>" lines for every sync point that applied ticks.
void PortTimer_SetRecordFile(FILE* f);

// Total ticks applied so far, and the tick rate (for deterministic durations, e.g. sound channels).
uint64_t PortTimer_TickCount();
double PortTimer_TickHz();

// Called once per presented frame by the video layer.
void PortTimer_OnFrame();
// Called from busy-wait loops in game code.
void PortTimer_Yield();

// --- Frame pacing (real-time mode only; no-ops for deterministic runs and replays) -------------
// The DOS game ran as fast as the CPU allowed and the game logic advances once per frame, so the
// port caps the frame rate. The cap is switched by context: gameplay (GameLoop) vs menus/movies.
void PortTimer_SetFrameCap(int fps);
// Refresh rate when presents block on vsync (0 = no vsync). With vsync the limiter paces in whole
// refreshes when the cap is close to refresh/N, instead of fighting the blocking present.
void PortTimer_SetVsyncRefresh(int hz);
int PortTimer_FrameCap();
// Waits so that consecutive calls are at least 1/cap apart (steady cadence, resyncs after stalls).
void PortTimer_LimitFrame();
// Emulates waiting for the VGA vertical retrace: sleeps until the next tick of a free-running
// clock at hz (70 Hz for the 320x200 mode the menus use).
void PortTimer_WaitRetrace(int hz);
