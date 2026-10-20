#include "MemSnapshot.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <algorithm>

#include "Settings.h"
#include "../engine/ptr32.h"
#include "port_sdl_vga_mouse.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
bool g_inTick = false;

#ifdef _WIN32
// File format: "MC1SNAP1", then records { uint32 base, uint32 size, bytes }, terminated by {0, 0}.
// Only committed, readable+writable memory that belongs to the process heap/allocations (MEM_PRIVATE)
// or to remc1.exe's own image (its .data/.bss) is written; of the game arena only the used part.
// 64-bit builds write the game's 32-bit offset space instead of addresses (engine/ptr32.h): the statics
// (at 4 MB + RVA), the arena (at 512 MB) and the game stack (at 1 GB); memory outside them is left out.
// To the harness (tools/emu.py) that looks like a 32-bit process with its image at 0x400000.
bool WriteDump(const std::string& path)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (!f)
		return false;
	fwrite("MC1SNAP1", 1, 8, f);
	HMODULE exe = GetModuleHandleA(nullptr);
	uint8_t* p = nullptr;
	MEMORY_BASIC_INFORMATION mbi;
	while (VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi))
	{
		uint8_t* next = (uint8_t*)mbi.BaseAddress + mbi.RegionSize;
		bool rw = (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
		bool keep = mbi.State == MEM_COMMIT && rw && !(mbi.Protect & PAGE_GUARD) &&
			(mbi.Type == MEM_PRIVATE || (mbi.Type == MEM_IMAGE && mbi.AllocationBase == (void*)exe));
		uint8_t* lo = (uint8_t*)mbi.BaseAddress;
		size_t size = mbi.RegionSize;
		uint8_t *arena, *arenaTop;
		Port_ArenaRange(&arena, &arenaTop);
		if (keep && arena && lo <= arena && arena < lo + size)
			size = ((arenaTop - lo) + 0xFFF) & ~(size_t)0xFFF;  // whole pages (the harness maps pages)
		uint32_t off;
		if (keep && size && Port_TryOff(lo, &off))
		{
			uint32_t hdr[2] = { off, (uint32_t)size };
			fwrite(hdr, 4, 2, f);
			fwrite(lo, 1, size, f);
		}
		if (next <= p)
			break;
		p = next;
	}
	uint32_t end[2] = { 0, 0 };
	fwrite(end, 4, 2, f);
	fclose(f);
	return true;
}
#else
bool WriteDump(const std::string&) { return false; }
#endif
} // namespace

extern void* MemSnapshot_State;
extern size_t MemSnapshot_StateSize;
void* MemSnapshot_State = nullptr;
size_t MemSnapshot_StateSize = 0;

namespace {
// One requested frame: <dir>. Several (--snapshot-frame N1,N2,..): <dir>/f<N> for the frame being taken.
std::string Dir()
{
	std::string base = g_settings.snapshotDir.empty() ? std::string("snapshot") : g_settings.snapshotDir;
	if (g_settings.snapshotFrames.size() <= 1)
		return base;
#ifdef _WIN32
	CreateDirectoryA(base.c_str(), nullptr);
#endif
	return base + "/f" + std::to_string(g_settings.snapshotFrame);
}

// Start a snapshot: create its directory and write meta.txt (the caller adds its own lines after the
// common ones). Returns the directory.
std::string Begin(const char* extra)
{
	std::string dir = Dir();
#ifdef _WIN32
	CreateDirectoryA(dir.c_str(), nullptr);
#endif
	FILE* m = fopen((dir + "/meta.txt").c_str(), "w");
	if (m)
	{
#ifdef _WIN32
		uint32_t image;
		Port_TryOff(GetModuleHandleA(nullptr), &image);
		fprintf(m, "image_base %#x\npointer_size %u\n", (unsigned)image, (unsigned)sizeof(void*));
		// statics, arena, stack window bases: turn real pointers held in statics into offsets
		fprintf(m, "bases %#llx %#llx %#llx\n", (unsigned long long)(uintptr_t)g_ptrBases[0],
			(unsigned long long)(uintptr_t)g_ptrBases[1], (unsigned long long)(uintptr_t)g_ptrBases[2]);
#endif
		fprintf(m, "frame %u\n%s", VGA_PresentedFrames(), extra);
		fclose(m);
	}
	if (!WriteDump(dir + "/before.bin"))
		fprintf(stderr, "snapshot: cannot write %s/before.bin\n", dir.c_str());
	return dir;
}

// Finish a snapshot; move on to the next requested frame, or exit after the last one.
void End(const char* what)
{
	std::string dir = Dir();
	WriteDump(dir + "/after.bin");
	fprintf(stderr, "snapshot (%s): frame %u written to %s\n", what, VGA_PresentedFrames(), dir.c_str());
	fflush(stderr);
	auto& v = g_settings.snapshotFrames;
	auto it = std::upper_bound(v.begin(), v.end(), g_settings.snapshotFrame);
	if (it == v.end())
		exit(0);
	g_settings.snapshotFrame = (std::max)(*it, (int)VGA_PresentedFrames() + 1);
}
} // namespace

void MemSnapshot_Call(const char* what, bool before, unsigned idaAddress, const int* args, int nargs)
{
	static bool inCall = false;
	if (g_settings.snapshotFrame < 0 || g_settings.snapshotWhat != what)
		return;
	if (before)
	{
		if ((int)VGA_PresentedFrames() < g_settings.snapshotFrame || inCall)
			return;
		inCall = true;
		char extra[512];
		int n = snprintf(extra, sizeof(extra), "state %#x %u\ncall %#x", (unsigned)OFF(MemSnapshot_State),
			(unsigned)MemSnapshot_StateSize, idaAddress);
		for (int i = 0; i < nargs && n < (int)sizeof(extra) - 16; i++)
			n += snprintf(extra + n, sizeof(extra) - n, " %d", args[i]);
		snprintf(extra + n, sizeof(extra) - n, "\n");
		Begin(extra);
	}
	else if (inCall)
	{
		inCall = false;
		End(what);
	}
}

void MemSnapshot_Tick(bool before, const void* state, size_t stateSize, const void* settings, size_t settingsSize)
{
	if (before && state)
	{
		MemSnapshot_State = (void*)state;
		MemSnapshot_StateSize = stateSize;
	}
	if (g_settings.snapshotFrame < 0 || g_settings.snapshotWhat != "tick")
		return;
	if (before)
	{
		if ((int)VGA_PresentedFrames() < g_settings.snapshotFrame || g_inTick)
			return;
		g_inTick = true;
		char extra[128];
		snprintf(extra, sizeof(extra), "state %#x %u\nsettings %#x %u\n", (unsigned)OFF(state),
			(unsigned)stateSize, (unsigned)OFF(settings), (unsigned)settingsSize);
		Begin(extra);
	}
	else if (g_inTick)
	{
		g_inTick = false;
		End("tick");
	}
}
