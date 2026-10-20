#pragma once
// Freeze watchdog: if no frame is presented for `seconds`, print where the main thread is stuck
// (symbolised stack on Windows) and terminate. Enabled by --watchdog N, and by default (20 s) for
// automated runs (--input-script / --exit-after-frames / --headless) so hangs never block tooling.

#include <cstdint>

// Must be called from the main (game) thread.
void Watchdog_Start(int seconds);
// Called once per presented frame.
void Watchdog_Kick();
// Long legitimate stalls (e.g. level generation without blits) can call this.
void Watchdog_Pause(bool paused);
