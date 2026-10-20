#include "port_timer.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace {

constexpr double kPitHz = 1193181.666;

// The port clock always runs (from the first sync point); the game's ISR is only called while the
// game has installed it (PortTimer_Start .. PortTimer_Stop). Port services (sound channel timing)
// use the clock's tick count, which therefore advances even while the game's timer is off.
std::thread worker;
std::atomic<bool> workerRunning{ false };
std::atomic<bool> isrInstalled{ false };
std::atomic<uint64_t> dueTicks{ 0 };   // incremented by the real-time worker
uint64_t appliedTicks = 0;             // consumed on the game thread
PortTimerCallback callback = nullptr;
std::atomic<double> tickSeconds{ 9903.0 / kPitHz };

bool deterministic = false;
int ticksPerFrame = 5;

bool replaying = false;
std::map<uint64_t, uint32_t> replayLog;
FILE* recordFile = nullptr;
uint64_t syncIndex = 0;
uint64_t totalTicks = 0;

void Worker()
{
	using clock = std::chrono::steady_clock;
	auto next = clock::now();
	while (workerRunning.load()) {
		const auto period = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(tickSeconds.load()));
		next += period;
		auto now = clock::now();
		if (next > now)
			std::this_thread::sleep_until(next);
		else if (now - next > period * 30)
			next = now; // fell far behind (debugger break, etc.): don't burst hundreds of ticks
		dueTicks.fetch_add(1);
	}
}

void EnsureClock()
{
	if (!deterministic && !replaying && !workerRunning.load()) {
		workerRunning = true;
#if defined(_WIN32)
		timeBeginPeriod(1); // 1 ms sleep granularity for the 120 Hz worker (default is 15.6 ms)
#endif
		appliedTicks = dueTicks.load();
		worker = std::thread(Worker);
		static bool registered = false;
		if (!registered) {
			registered = true;
			std::atexit(PortTimer_Shutdown);
		}
	}
}

void SyncPoint(bool isFrame)
{
	if (replaying && syncIndex > replayLog.rbegin()->first) {
		// Recording ended (e.g. the game was closed mid-FMV): continue deterministically instead of
		// starving tick-waiting loops forever.
		replaying = false;
		deterministic = true;
	}
	EnsureClock();
	uint32_t n;
	if (replaying) {
		auto it = replayLog.find(syncIndex);
		n = it == replayLog.end() ? 0 : it->second;
	}
	else if (deterministic) {
		n = isFrame ? ticksPerFrame : 1;
	}
	else {
		uint64_t due = dueTicks.load();
		n = (uint32_t)(due - appliedTicks);
		appliedTicks = due;
	}
	totalTicks += n;
	if (isrInstalled.load() && callback)
		for (uint32_t i = 0; i < n; i++)
			callback();
	if (recordFile && n)
		fprintf(recordFile, "T %llu %u\n", (unsigned long long)syncIndex, n);
	syncIndex++;
}

} // namespace

void PortTimer_SetDeterministic(bool enabled, int perFrame)
{
	deterministic = enabled;
	ticksPerFrame = perFrame > 0 ? perFrame : 1;
}

bool PortTimer_IsDeterministic()
{
	return deterministic || replaying;
}

void PortTimer_SetReplayLog(std::map<uint64_t, uint32_t> log)
{
	replayLog = std::move(log);
	replaying = !replayLog.empty();
}

void PortTimer_SetRecordFile(FILE* f)
{
	recordFile = f;
}

void PortTimer_Start(uint32_t divisor, PortTimerCallback isr)
{
	if (divisor == 0)
		divisor = 0x10000; // PIT semantics: 0 means 65536
	tickSeconds = divisor / kPitHz;
	callback = isr;
	isrInstalled = true;
}

void PortTimer_Stop()
{
	isrInstalled = false;
	callback = nullptr;
}

bool PortTimer_Running()
{
	return isrInstalled.load();
}

void PortTimer_Shutdown()
{
	workerRunning = false;
	if (worker.joinable())
		worker.join();
}

void PortTimer_OnFrame()
{
	SyncPoint(true);
}

void PortTimer_Yield()
{
	SyncPoint(false);
	if (!deterministic && !replaying)
		std::this_thread::sleep_for(std::chrono::microseconds(500));
}

uint64_t PortTimer_TickCount()
{
	return totalTicks;
}

double PortTimer_TickHz()
{
	return 1.0 / tickSeconds.load();
}

// --- Frame pacing ----------------------------------------------------------------------------

namespace {

using PaceClock = std::chrono::steady_clock;
int frameCap = 70;
int vsyncHz = 0;

// With vsync: number of refreshes per frame if refresh/N is within 3% of the cap (60/120 Hz displays),
// else 0 = use the time-based limiter (e.g. 144/165 Hz: locking would change the game speed).
int VsyncFramesFor(int fps)
{
	if (vsyncHz <= 0 || fps <= 0)
		return 0;
	int n = (int)(vsyncHz / (double)fps + 0.5);
	if (n < 1)
		n = 1;
	const double rate = vsyncHz / (double)n;
	return (rate > fps * 1.03 || rate < fps * 0.97) ? 0 : n;
}

// Windows sleeps in 15.6 ms steps unless the process asks for 1 ms timer resolution; without it the
// 120 Hz clock thread, Yield() and the frame limiter are all lumpy.
void EnsureFineSleep()
{
#if defined(_WIN32)
	static bool done = false;
	if (!done) {
		done = true;
		timeBeginPeriod(1);
	}
#endif
}

void WaitUntil(PaceClock::time_point t)
{
	EnsureFineSleep();
	for (;;) {
		const auto now = PaceClock::now();
		if (now >= t)
			return;
		const auto left = t - now;
		if (left > std::chrono::milliseconds(2))
			std::this_thread::sleep_for(left - std::chrono::milliseconds(1));
		else
			std::this_thread::yield(); // last ~2 ms: don't trust sleep granularity
	}
}

} // namespace

void PortTimer_SetFrameCap(int fps)
{
	frameCap = fps;
}

void PortTimer_SetVsyncRefresh(int hz)
{
	vsyncHz = hz;
}

int PortTimer_FrameCap()
{
	return frameCap;
}

void PortTimer_LimitFrame()
{
	static PaceClock::time_point next{};
	if (deterministic || replaying || frameCap <= 0)
		return;
	const auto now = PaceClock::now();
	if (const int n = VsyncFramesFor(frameCap)) {
		// Called right after a blocking present, i.e. just after a refresh: skip n-1 refreshes and
		// let the next present wait for the n-th. (A time-based wait here wakes just after a
		// refresh and misses it - every frame then takes one refresh too many.)
		if (n > 1)
			WaitUntil(now + std::chrono::duration_cast<PaceClock::duration>(
				std::chrono::duration<double>((n - 1) / (double)vsyncHz - 0.0015)));
		next = {};
		return;
	}
	const auto period = std::chrono::duration_cast<PaceClock::duration>(std::chrono::duration<double>(1.0 / frameCap));
	if (next.time_since_epoch().count() == 0 || now - next > period * 4)
		next = now; // first frame, or we stalled (loading, debugger): don't try to catch up
	else
		WaitUntil(next);
	next += period;
}

void PortTimer_WaitRetrace(int hz)
{
	static const PaceClock::time_point origin = PaceClock::now();
	if (deterministic || replaying || hz <= 0 || VsyncFramesFor(hz) == 1)
		return; // with vsync near hz, the present that follows waits for the retrace itself
	const auto period = std::chrono::duration_cast<PaceClock::duration>(std::chrono::duration<double>(1.0 / hz));
	const auto now = PaceClock::now();
	WaitUntil(origin + period * ((now - origin) / period + 1));
}
