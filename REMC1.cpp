// REMC1 entry point: port settings are resolved here, then control passes to the decompiled game.
#include "sub_main.h"
#include "portability/Settings.h"
#include "portability/port_watchdog.h"
#include "portability/port_timer.h"

#include <string>
#include <vector>
#include <cstring>
#include <cstdio>

#if defined(_WIN32)
#include <crtdbg.h>
#include <stdlib.h>
#endif

static void ConfigureCrashReporting()
{
#if defined(_WIN32)
    // Never block on modal CRT/WER dialogs (heap corruption, asserts, abort): automated runs and
    // tests must fail fast and print the reason.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
}

int RncUnpack_62B60_63070(uint8_t* input, uint8_t* output);

// Developer utility: remc1 --unpack <in> <out> writes the RNC-decompressed file (or a copy if not packed).
static int UnpackFile(const char* in, const char* out)
{
    FILE* f = fopen(in, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", in); return 1; }
    std::vector<uint8_t> data;
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        data.insert(data.end(), buf, buf + n);
    fclose(f);
    std::vector<uint8_t> result = data;
    if (data.size() >= 18 && memcmp(data.data(), "RNC\x01", 4) == 0) {
        uint32_t len = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
        result.assign(len, 0);
        if (RncUnpack_62B60_63070(data.data(), result.data()) != (int)len) { fprintf(stderr, "decode failed\n"); return 1; }
    }
    FILE* o = fopen(out, "wb");
    if (!o) { fprintf(stderr, "cannot write %s\n", out); return 1; }
    fwrite(result.data(), 1, result.size(), o);
    fclose(o);
    printf("%s -> %s (%zu bytes)\n", in, out, result.size());
    return 0;
}

int main(int argc, char* argv[])
{
    ConfigureCrashReporting();
    if (argc == 4 && std::string(argv[1]) == "--unpack")
        return UnpackFile(argv[2], argv[3]);

    std::vector<std::string> gameArgs = InitSettings(argc, argv);
    if (gameArgs.empty())
        return 1;
    Watchdog_Start(g_settings.watchdogSeconds);
    PortTimer_SetFrameCap(g_settings.menuFps);

    std::vector<char*> gameArgv;
    for (auto& a : gameArgs)
        gameArgv.push_back(a.data());
    gameArgv.push_back(nullptr);

    sub_main((int)gameArgs.size(), gameArgv.data());
    return 0;
}
