#include "Settings.h"

#include "fcaseopen.h"
#include "InputScript.h"
#include "port_timer.h"
#include "port_filesystem.h"
#include "../engine/CommandLineParser.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

Remc1Settings g_settings;

static std::string CaseFixed(const std::string& path)
{
#if defined(_WIN32)
	return path;
#else
	return casepath(path);
#endif
}

bool IsValidDataPath(const std::string& path)
{
	if (path.empty())
		return false;
	std::error_code ec;
	return fs::is_directory(CaseFixed(path + "/CD/DATA"), ec);
}

std::string GetCdDataPath()
{
	return CaseFixed(g_settings.dataPath + "/CD");
}

std::string GetInstallRootPath()
{
	return g_settings.cDrivePath.empty() ? g_settings.dataPath : g_settings.cDrivePath;
}

static std::vector<fs::path> UserDirs()
{
	std::vector<fs::path> dirs;
#if defined(_WIN32)
	if (const char* appdata = std::getenv("APPDATA"))
		dirs.emplace_back(fs::path(appdata) / "remc1");
	if (const char* profile = std::getenv("USERPROFILE"))
		dirs.emplace_back(fs::path(profile) / "remc1");
#elif defined(__APPLE__)
	if (const char* home = std::getenv("HOME"))
		dirs.emplace_back(fs::path(home) / "Library" / "Application Support" / "remc1");
#else
	if (const char* xdg = std::getenv("XDG_DATA_HOME"))
		dirs.emplace_back(fs::path(xdg) / "remc1");
	if (const char* home = std::getenv("HOME"))
		dirs.emplace_back(fs::path(home) / ".local" / "share" / "remc1");
#endif
	return dirs;
}

static std::vector<fs::path> ConfigCandidates()
{
	std::vector<fs::path> c;
#if !defined(_WIN32) && !defined(__APPLE__)
	if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
		c.emplace_back(fs::path(xdg) / "remc1" / "config.json");
	if (const char* home = std::getenv("HOME"))
		c.emplace_back(fs::path(home) / ".config" / "remc1" / "config.json");
#endif
	for (auto& d : UserDirs())
		c.emplace_back(d / "config.json");
	c.emplace_back(fs::path(get_exe_path()) / "config.json");
	return c;
}

static void LoadConfigFile(const fs::path& path)
{
	std::ifstream in(path);
	if (!in)
		return;
	nlohmann::json j;
	try {
		in >> j;
	}
	catch (const std::exception& e) {
		std::cerr << "remc1: ignoring malformed config " << path.string() << ": " << e.what() << "\n";
		return;
	}
	g_settings.configPath = path.string();
	auto& s = g_settings;
	if (j.contains("paths")) {
		auto& p = j["paths"];
		if (p.contains("data") && !p["data"].get<std::string>().empty()) {
			fs::path d = p["data"].get<std::string>();
			if (d.is_relative())
				d = path.parent_path() / d;
			s.dataPath = d.string();
		}
	}
	if (j.contains("graphics")) {
		auto& g = j["graphics"];
		s.fullscreen = g.value("fullscreen", s.fullscreen);
		s.windowWidth = g.value("windowWidth", s.windowWidth);
		s.windowHeight = g.value("windowHeight", s.windowHeight);
		s.maintainAspectRatio = g.value("maintainAspectRatio", s.maintainAspectRatio);
		s.vsync = g.value("vsync", s.vsync);
		s.maxGameFps = g.value("maxGameFps", s.maxGameFps);
		s.menuFps = g.value("menuFps", s.menuFps);
	}
	if (j.contains("audio")) {
		s.sound = j["audio"].value("sound", s.sound);
		s.music = j["audio"].value("music", s.music);
	}
	if (j.contains("input")) {
		s.grabMouse = j["input"].value("grabMouse", s.grabMouse);
		s.wasdAsArrows = j["input"].value("wasdAsArrows", s.wasdAsArrows);
		s.tabAsEnter = j["input"].value("tabAsEnter", s.tabAsEnter);
		s.bothButtonsSpellMenu = j["input"].value("bothButtonsSpellMenu", s.bothButtonsSpellMenu);
	}
	if (j.contains("debug")) {
		s.debugLog = j["debug"].value("log", s.debugLog);
	}
}

static void PrintUsage()
{
	std::cout <<
		"remc1 - Magic Carpet 1 port\n"
		"Port options:\n"
		"  --data <dir>       game data folder (created by tools/extract_data.py)\n"
		"  --config <file>    config.json to use\n"
		"  --fullscreen       start fullscreen\n"
		"  --window WxH       window size (default 1280x960)\n"
		"  --no-grab          do not confine the mouse to the window\n"
		"  --no-vsync         present without waiting for vertical sync\n"
		"  --no-wasd          don't map W/A/S/D to the arrow keys\n"
		"  --no-tab-enter     don't map Tab to Enter\n"
		"  --fps N            in-game frame cap (default 30, 0 = uncapped; game speed follows it)\n"
		"  --no-sound / --no-music   disable sound effects / music\n"
		"  --debug-log        write debug.txt next to the executable\n"
		"  --dump-frames <dir> [--dump-every N]  save every Nth presented frame (default 30) as BMP\n"
		"  --exit-after-frames N                  quit after N presented frames\n"
		"  --snapshot-frame N[,N2,..] [--snapshot-dir D]  dump memory around the entity tick of frame N (tools/diff_tick.py)\n"
		"  --c-drive <dir>                        folder standing in for DOS C: (must contain CARPET.CD)\n"
		"  --watchdog N                           abort with a stack trace if no frame is shown for N s\n"
		"                                         (default 20 for scripted/headless runs, 0 = off)\n"
		"  --headless                           no visible window (SDL dummy driver), live input ignored\n"
		"  --frame-hashes <file>                  log a hash of every presented frame (replay tests)\n"
		"  --state-trace <file>                   log game-state + frame hashes of every frame (tools/regress.py)\n"
		"  --state-dump <frame>:<file>            write the normalised game state of one frame (tools/diff_state.py)\n"
		"  --trace-handlers <frame>               print the game-state hash after every entity handler of that tick\n"
		"  --frame-stats                          print frame-time percentiles every 5 s (smoothness)\n"
		"  --deterministic [N]                   game timer advances N ticks (default 5) per frame instead of real time\n"
		"  --input-script <file>                 play scripted input (format: portability/InputScript.h)\n"
		"  --record-input <file>                  record live input in the same format\n"
		"Original game options (-level N, -cheat N, -detail N, -network, ...) are passed through.\n";
}

std::vector<std::string> InitSettings(int argc, char* argv[])
{
	CommandLineParams.Init(1, argv); // REMC2 legacy flags: defaults only

	std::vector<std::string> passthrough;
	passthrough.emplace_back(argc > 0 ? argv[0] : "remc1");

	std::string cliData, cliConfig;
	bool cliFullscreen = false, cliNoGrab = false, cliDebugLog = false;
	int cliW = 0, cliH = 0;

	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto next = [&](const char* what) -> std::string {
			if (i + 1 >= argc) {
				std::cerr << "remc1: " << a << " needs " << what << "\n";
				return {};
			}
			return argv[++i];
		};
		if (a == "--data") cliData = next("a directory");
		else if (a == "--config") cliConfig = next("a file");
		else if (a == "--fullscreen") cliFullscreen = true;
		else if (a == "--no-grab") cliNoGrab = true;
		else if (a == "--no-vsync") g_settings.vsync = false;
		else if (a == "--no-wasd") g_settings.wasdAsArrows = false;
		else if (a == "--no-tab-enter") g_settings.tabAsEnter = false;
		else if (a == "--fps") g_settings.maxGameFps = atoi(next("frames per second").c_str());
		else if (a == "--no-sound") g_settings.sound = false;
		else if (a == "--no-music") g_settings.music = false;
		else if (a == "--debug-log") cliDebugLog = true;
		else if (a == "--dump-frames") g_settings.dumpFramesDir = next("a directory");
		else if (a == "--dump-every") g_settings.dumpEvery = (std::max)(1, atoi(next("a number").c_str()));
		else if (a == "--exit-after-frames") g_settings.exitAfterFrames = atoi(next("a number").c_str());
		else if (a == "--snapshot-frame")
		{
			// "N" or "N1,N2,...": several frames go to <dir>/f<N> each, the game exits after the last
			std::string list = next("a frame number or list");
			for (size_t p = 0; p < list.size();)
			{
				size_t c = list.find(',', p);
				if (c == std::string::npos) c = list.size();
				g_settings.snapshotFrames.push_back(atoi(list.substr(p, c - p).c_str()));
				p = c + 1;
			}
			std::sort(g_settings.snapshotFrames.begin(), g_settings.snapshotFrames.end());
			g_settings.snapshotFrame = g_settings.snapshotFrames.empty() ? -1 : g_settings.snapshotFrames[0];
		}
		else if (a == "--snapshot-dir") g_settings.snapshotDir = next("a directory");
		else if (a == "--snapshot-what") g_settings.snapshotWhat = next("tick, draw or frame");
		else if (a == "--frame-hashes") g_settings.frameHashFile = next("a file");
		else if (a == "--state-trace") g_settings.stateTraceFile = next("a file");
		else if (a == "--trace-handlers") g_settings.traceHandlersFrame = atoi(next("a frame").c_str());
		else if (a == "--state-dump")
		{
			std::string v = next("<frame>:<file>");
			size_t c = v.find(':');
			if (c == std::string::npos || c == 0) { fprintf(stderr, "--state-dump needs <frame>:<file>\n"); exit(2); }
			g_settings.stateDumpFrame = (uint32_t)strtoul(v.substr(0, c).c_str(), nullptr, 10);
			g_settings.stateDumpFile = v.substr(c + 1);
		}
		else if (a == "--frame-stats") g_settings.frameStats = true;
		else if (a == "--headless") g_settings.headless = true;
		else if (a == "--watchdog") g_settings.watchdogSeconds = atoi(next("seconds").c_str());
		else if (a == "--c-drive") g_settings.cDrivePath = fs::path(next("a directory")).generic_string();
		else if (a == "--deterministic") {
			int perFrame = 5;
			if (i + 1 < argc && argv[i + 1][0] != '-')
				perFrame = atoi(argv[++i]);
			PortTimer_SetDeterministic(true, perFrame);
		}
		else if (a == "--input-script") { if (!InputScript_Load(next("a file"))) return {}; }
		else if (a == "--record-input") { if (!InputScript_StartRecording(next("a file"))) return {}; }
		else if (a == "--window") {
			std::string v = next("WxH");
			if (sscanf(v.c_str(), "%dx%d", &cliW, &cliH) != 2) { cliW = cliH = 0; }
		}
		else if (a == "--help" || a == "-h") { PrintUsage(); return {}; }
		else if (a.rfind("--", 0) == 0) {
			std::cerr << "remc1: unknown option " << a << " (see --help)\n";
			return {};
		}
		else passthrough.push_back(a);
	}

	if (!cliConfig.empty())
		LoadConfigFile(cliConfig);
	else
		for (auto& c : ConfigCandidates()) {
			std::error_code ec;
			if (fs::exists(c, ec)) { LoadConfigFile(c); break; }
		}

	if (const char* env = std::getenv("REMC1_DATA_DIR"))
		g_settings.dataPath = env;
	if (!cliData.empty())
		g_settings.dataPath = cliData;
	if (cliFullscreen) g_settings.fullscreen = true;
	if (cliNoGrab) g_settings.grabMouse = false;
	if (cliDebugLog) g_settings.debugLog = true;
	if (cliW > 0 && cliH > 0) { g_settings.windowWidth = cliW; g_settings.windowHeight = cliH; }
	if (g_settings.watchdogSeconds < 0) {
		bool automated = g_settings.headless || g_settings.exitAfterFrames > 0 || !g_settings.frameHashFile.empty() || !g_settings.stateTraceFile.empty();
		for (int i = 1; i < argc; i++)
			if (std::string(argv[i]) == "--input-script")
				automated = true;
		g_settings.watchdogSeconds = automated ? 20 : 0;
	}
	if (g_settings.headless) {
#if defined(_WIN32)
		_putenv_s("SDL_VIDEODRIVER", "dummy");
		_putenv_s("SDL_AUDIODRIVER", "dummy");
#else
		setenv("SDL_VIDEODRIVER", "dummy", 1);
		setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif
		g_settings.grabMouse = false;
		g_settings.fullscreen = false;
	}

	if (g_settings.dataPath.empty()) {
		std::vector<fs::path> search;
		search.emplace_back(fs::path(get_exe_path()) / "data");
		for (auto& d : UserDirs())
			search.emplace_back(d / "data");
		for (auto& s : search)
			if (IsValidDataPath(s.string())) { g_settings.dataPath = s.string(); break; }
	}

	if (!IsValidDataPath(g_settings.dataPath)) {
		std::cerr << "remc1: no game data found"
			<< (g_settings.dataPath.empty() ? std::string() : " at '" + g_settings.dataPath + "'") << ".\n"
			"Extract it from your GOG install with:\n"
			"  python tools/extract_data.py --gog \"<GOG Magic Carpet Plus folder>\" --out <datadir>\n"
			"then run with --data <datadir> (or set REMC1_DATA_DIR, or paths.data in config.json).\n";
		return {};
	}
	// normalise separators; game code concatenates with '/'
	g_settings.dataPath = fs::path(g_settings.dataPath).generic_string();
	while (g_settings.dataPath.size() > 1 && g_settings.dataPath.back() == '/')
		g_settings.dataPath.pop_back();
	return passthrough;
}
