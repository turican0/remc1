#include "port_watchdog.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace {

std::atomic<uint64_t> kicks{ 0 };
std::atomic<bool> paused{ false };
int timeoutSeconds = 0;

#if defined(_WIN32)
HANDLE mainThread = nullptr;

void DumpMainThreadStack()
{
	HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	SymInitialize(process, nullptr, TRUE);
	if (SuspendThread(mainThread) == (DWORD)-1) {
		fprintf(stderr, "WATCHDOG: cannot suspend main thread\n");
		return;
	}
	CONTEXT ctx = {};
	ctx.ContextFlags = CONTEXT_FULL;
	if (!GetThreadContext(mainThread, &ctx)) {
		fprintf(stderr, "WATCHDOG: GetThreadContext failed\n");
		return;
	}
	STACKFRAME64 frame = {};
	DWORD machine;
#if defined(_M_X64)
	machine = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = ctx.Rip; frame.AddrStack.Offset = ctx.Rsp; frame.AddrFrame.Offset = ctx.Rbp;
#elif defined(_M_IX86)
	machine = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = ctx.Eip; frame.AddrStack.Offset = ctx.Esp; frame.AddrFrame.Offset = ctx.Ebp;
#else
	machine = IMAGE_FILE_MACHINE_ARM64;
	frame.AddrPC.Offset = ctx.Pc; frame.AddrStack.Offset = ctx.Sp; frame.AddrFrame.Offset = ctx.Fp;
#endif
	frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
	alignas(SYMBOL_INFO) char symBuf[sizeof(SYMBOL_INFO) + 256];
	for (int i = 0; i < 40; i++) {
		if (!StackWalk64(machine, process, mainThread, &frame, &ctx, nullptr, SymFunctionTableAccess64,
			SymGetModuleBase64, nullptr) || frame.AddrPC.Offset == 0)
			break;
		SYMBOL_INFO* sym = (SYMBOL_INFO*)symBuf;
		sym->SizeOfStruct = sizeof(SYMBOL_INFO);
		sym->MaxNameLen = 255;
		DWORD64 disp = 0;
		const char* name = SymFromAddr(process, frame.AddrPC.Offset, &disp, sym) ? sym->Name : "?";
		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(line);
		DWORD lineDisp = 0;
		if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line))
			fprintf(stderr, "  #%02d %s  %s:%lu\n", i, name, line.FileName, line.LineNumber);
		else
			fprintf(stderr, "  #%02d %s +0x%llx\n", i, name, (unsigned long long)disp);
	}
}
#endif

void Worker()
{
	uint64_t last = kicks.load();
	auto lastChange = std::chrono::steady_clock::now();
	for (;;) {
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
		uint64_t now = kicks.load();
		auto t = std::chrono::steady_clock::now();
		if (now != last || paused.load()) {
			last = now;
			lastChange = t;
			continue;
		}
		if (t - lastChange > std::chrono::seconds(timeoutSeconds)) {
			fprintf(stderr, "\nWATCHDOG: no frame presented for %d s (frames so far: %llu) - game is hung\n",
				timeoutSeconds, (unsigned long long)now);
#if defined(_WIN32)
			DumpMainThreadStack();
#endif
			fflush(stderr);
			fflush(stdout);
			std::_Exit(3);
		}
	}
}

} // namespace

void Watchdog_Start(int seconds)
{
	if (seconds <= 0 || timeoutSeconds > 0)
		return;
	timeoutSeconds = seconds;
#if defined(_WIN32)
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &mainThread, 0, FALSE,
		DUPLICATE_SAME_ACCESS);
#endif
	std::thread(Worker).detach();
}

void Watchdog_Kick()
{
	kicks.fetch_add(1, std::memory_order_relaxed);
}

void Watchdog_Pause(bool p)
{
	paused = p;
}
