#include "ptr32.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <sys/mman.h>
#endif

// The executable's own load address. A link-time constant, so it is valid before any static initialiser runs.
#if defined(_WIN32)
extern "C" char __ImageBase;
#define IMAGE_START ((uint8_t*)&__ImageBase)
#elif defined(__APPLE__)
extern "C" char _mh_execute_header;
#define IMAGE_START ((uint8_t*)&_mh_execute_header)
#else
extern "C" char __executable_start;
#define IMAGE_START ((uint8_t*)&__executable_start)
#endif
// statics at offset 4 MB + RVA, the numbers a 32-bit exe at 0x400000 would have: clear of the original's
// addresses 0x10000..0x150000, so harness snapshots in offset space can hold both (MemSnapshot.cpp)
#define STATICS_BASE (IMAGE_START - 0x400000)
uint8_t* const g_ptrBase = STATICS_BASE;
#define NO_WINDOW ((uint8_t*)(uintptr_t)0xFFFF800000000000ull)  // matches no user-space address (OFF)
// [1]: the arena (set by ArenaInit), [2][3]: the game stack (Port_PtrOutOfRange), the rest unused
uint8_t* g_ptrBases[8] = { STATICS_BASE, NO_WINDOW, NO_WINDOW, NO_WINDOW, NO_WINDOW, NO_WINDOW, NO_WINDOW, NO_WINDOW };

static const intptr_t kStackOffset = 0x40000000;  // the stack window: 1 GB .. 2 GB
static uint8_t* s_stackLo;
static uint8_t* s_stackHi;

// The calling thread's stack [lo, hi).
static void ThreadStack(uint8_t** lo, uint8_t** hi)
{
#ifdef _WIN32
	ULONG_PTR l, h;
	GetCurrentThreadStackLimits(&l, &h);
	*lo = (uint8_t*)l;
	*hi = (uint8_t*)h;
#elif defined(__APPLE__)
	pthread_t t = pthread_self();
	*hi = (uint8_t*)pthread_get_stackaddr_np(t);
	*lo = *hi - pthread_get_stacksize_np(t);
#else
	pthread_attr_t a;
	void* addr = nullptr;
	size_t size = 0;
	pthread_getattr_np(pthread_self(), &a);
	pthread_attr_getstack(&a, &addr, &size);
	pthread_attr_destroy(&a);
	*lo = (uint8_t*)addr;
	*hi = *lo + size;
#endif
}

int32_t Port_PtrOutOfRange(const void* p)
{
	const uint8_t* q = (const uint8_t*)p;
	if (!s_stackLo)
	{
		// the first stack address: the thread running the game owns the stack window from now on
		uint8_t *lo, *hi;
		ThreadStack(&lo, &hi);
		if (q >= lo && q < hi && hi - lo < kStackOffset)
		{
			s_stackLo = lo;
			s_stackHi = hi;
			g_ptrBases[2] = g_ptrBases[3] = lo - kStackOffset;
		}
	}
	if (q >= s_stackLo && q < s_stackHi)
		return (int32_t)(q - g_ptrBases[2]);
	fprintf(stderr, "PORT: pointer %p is outside the game address windows (statics/arena at %p, game stack "
		"%p-%p; OFF in engine/ptr32.h): allocate it from the game arena or keep it out of 32-bit ints\n", p,
		(const void*)g_ptrBase, (const void*)s_stackLo, (const void*)s_stackHi);
	fflush(stderr);
	abort();
}

#include <vector>
static std::vector<void*> s_ext(1, nullptr);  // handle -> pointer (handle 0 = nullptr)

uint32_t Port_ExtHandle(void* p)
{
	if (!p)
		return 0;
	for (size_t i = 1; i < s_ext.size(); i++)
		if (s_ext[i] == p)
			return (uint32_t)i;
	s_ext.push_back(p);
	return (uint32_t)(s_ext.size() - 1);
}

void* Port_ExtPointer(uint32_t h)
{
	return h < s_ext.size() ? s_ext[h] : nullptr;
}

// Game memory arena. It sits at offset 512 MB (g_ptrBases[1] follows wherever the OS put it; the old 32-bit
// build had it at that fixed address), and allocation is deterministic, so game objects get the same 32-bit
// "addresses" on every platform and across code changes. That matters beyond OFF():
// the decompiled code turns addresses into numbers, e.g. (target - entity array) / 164 with a null target, and
// such values end up in the game state; with a fixed layout they are the same everywhere (regression traces,
// tools/regress.py). (A fixed distance from the executable is not possible: Linux puts the brk heap anywhere
// in the first GB after it.)
static const uintptr_t kArenaOffset = 0x20000000;
static const size_t kArenaSize = 0x10000000;  // 256 MB
static const size_t kHeader = 16;             // block size in front of every block, keeps 16-byte alignment
static uint8_t* s_arena;
static uint8_t* s_top;
static std::multimap<size_t, uint8_t*> s_free;  // freed blocks by size (best fit, deterministic)

static void ArenaInit()
{
#ifdef _WIN32
	s_arena = (uint8_t*)VirtualAlloc(nullptr, kArenaSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
	void* p = mmap(nullptr, kArenaSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	s_arena = p == MAP_FAILED ? nullptr : (uint8_t*)p;
#endif
	if (!s_arena)
	{
		fprintf(stderr, "PORT: cannot reserve the %u MB game memory arena\n", (unsigned)(kArenaSize >> 20));
		abort();
	}
	g_ptrBases[1] = s_arena - kArenaOffset;
	s_top = s_arena;
}

void Port_ArenaRange(uint8_t** lo, uint8_t** top)
{
	*lo = s_arena;
	*top = s_top;
}

bool Port_TryOff(const void* p, uint32_t* off)
{
	uintptr_t a = (uintptr_t)p;
	if (a - (uintptr_t)g_ptrBase < 0x20000000)
		*off = (uint32_t)(a - (uintptr_t)g_ptrBase);
	else if (a - (uintptr_t)g_ptrBases[1] - 0x20000000 < 0x20000000)
		*off = (uint32_t)(a - (uintptr_t)g_ptrBases[1]);
	else if (s_stackLo && (const uint8_t*)p >= s_stackLo && (const uint8_t*)p < s_stackHi)
		*off = (uint32_t)(a - (uintptr_t)g_ptrBases[2]);
	else
		return false;
	return true;
}

static bool InArena(const void* p)
{
	return s_arena && (const uint8_t*)p >= s_arena && (const uint8_t*)p < s_arena + kArenaSize;
}

void* Port_ArenaAlloc(size_t size)
{
	if (!s_arena)
		ArenaInit();
	size = (size + 15) & ~(size_t)15;
	uint8_t* block;
	auto it = s_free.lower_bound(size);
	if (it != s_free.end())
	{
		block = it->second;
		s_free.erase(it);
	}
	else
	{
		if ((size_t)(s_arena + kArenaSize - s_top) < size + kHeader)
		{
			fprintf(stderr, "PORT: game memory arena exhausted (%u MB)\n", (unsigned)(kArenaSize >> 20));
			abort();
		}
		block = s_top + kHeader;
		*(size_t*)(block - kHeader) = size;
		s_top += size + kHeader;
	}
	memset(block, 0, *(size_t*)(block - kHeader));
	return block;
}

void Port_ArenaFree(void* p)
{
	if (!p)
		return;
	if (!InArena(p))
	{
		free(p);  // not game memory (allocated with malloc elsewhere)
		return;
	}
	uint8_t* block = (uint8_t*)p;
	s_free.emplace(*(size_t*)(block - kHeader), block);
}
