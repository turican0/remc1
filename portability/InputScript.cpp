#include "InputScript.h"

#include "port_sdl_vga_mouse.h"
#include "port_timer.h"
#include "../engine/sub_main_mouse.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>
#include <map>
#include <vector>

namespace {

enum class Ev { KeyDown, KeyUp, Mouse, LDown, LUp, RDown, RUp, Quit };

struct ScriptEvent {
	uint32_t frame;
	Ev type;
	int a = 0, b = 0; // scancode, or x/y
};

std::vector<ScriptEvent> events;
size_t nextEvent = 0;
int mouseX = 320, mouseY = 240;

FILE* recordFile = nullptr;
std::map<uint64_t, uint32_t> tickLog;
std::map<uint32_t, uint64_t> checksums; // frame -> expected game-state hash ("C" lines)
bool desyncReported = false;

const uint32_t kAutoRelease = 3;

void Add(uint32_t frame, Ev type, int a = 0, int b = 0)
{
	events.push_back({ frame, type, a, b });
}

} // namespace

bool InputScript_Load(const std::string& path)
{
	std::ifstream in(path);
	if (!in) {
		std::cerr << "remc1: cannot open input script " << path << "\n";
		return false;
	}
	std::string line;
	int lineNo = 0;
	while (std::getline(in, line)) {
		lineNo++;
		auto hash = line.find('#');
		if (hash != std::string::npos)
			line.resize(hash);
		std::istringstream ss(line);
		if (!line.empty() && line[0] == 'C') { // state checksum: "C <frame> <hex hash>"
			char c;
			uint32_t f;
			std::string hex;
			if (ss >> c >> f >> hex)
				checksums[f] = std::stoull(hex, nullptr, 16);
			continue;
		}
		if (!line.empty() && line[0] == 'T') { // timer log: "T <syncIndex> <ticks>"
			char t;
			unsigned long long idx;
			uint32_t n;
			if (ss >> t >> idx >> n)
				tickLog[idx] = n;
			continue;
		}
		uint32_t frame;
		std::string cmd;
		if (!(ss >> frame >> cmd))
			continue;
		auto scancode = [&]() -> int {
			// the rest of the line: SDL key names can contain spaces ("Left Shift", "Keypad Enter") -
			// reading one word turned "Left Shift" into the Left arrow and desynchronised replays
			std::string name;
			std::getline(ss >> std::ws, name);
			while (!name.empty() && isspace((unsigned char)name.back()))
				name.pop_back();
			SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
			if (sc == SDL_SCANCODE_UNKNOWN)
				std::cerr << "remc1: " << path << ":" << lineNo << ": unknown key '" << name << "'\n";
			return (int)sc;
		};
		int x, y;
		if (cmd == "key") { int sc = scancode(); Add(frame, Ev::KeyDown, sc); Add(frame + kAutoRelease, Ev::KeyUp, sc); }
		else if (cmd == "keydown") Add(frame, Ev::KeyDown, scancode());
		else if (cmd == "keyup") Add(frame, Ev::KeyUp, scancode());
		else if (cmd == "mouse") { ss >> x >> y; Add(frame, Ev::Mouse, x, y); }
		else if (cmd == "lclick" || cmd == "rclick") {
			if (ss >> x >> y)
				Add(frame, Ev::Mouse, x, y);
			bool left = cmd == "lclick";
			Add(frame, left ? Ev::LDown : Ev::RDown);
			Add(frame + kAutoRelease, left ? Ev::LUp : Ev::RUp);
		}
		else if (cmd == "ldown") Add(frame, Ev::LDown);
		else if (cmd == "lup") Add(frame, Ev::LUp);
		else if (cmd == "rdown") Add(frame, Ev::RDown);
		else if (cmd == "rup") Add(frame, Ev::RUp);
		else if (cmd == "quit") Add(frame, Ev::Quit);
		else std::cerr << "remc1: " << path << ":" << lineNo << ": unknown command '" << cmd << "'\n";
	}
	std::stable_sort(events.begin(), events.end(),
		[](const ScriptEvent& l, const ScriptEvent& r) { return l.frame < r.frame; });
	nextEvent = 0;
	if (!tickLog.empty()) {
		// a recording with timer log: replay ticks exactly (bit-exact replay of a live session)
		PortTimer_SetReplayLog(std::move(tickLog));
	}
	return true;
}

void InputScript_OnFrame(uint32_t frame)
{
	while (nextEvent < events.size() && events[nextEvent].frame <= frame) {
		const ScriptEvent& e = events[nextEvent++];
		switch (e.type) {
		case Ev::KeyDown: InputScript_RecordKey(frame, true, e.a); VGA_InjectKey(true, e.a); break;
		case Ev::KeyUp: InputScript_RecordKey(frame, false, e.a); VGA_InjectKey(false, e.a); break;
		case Ev::Mouse: mouseX = e.a; mouseY = e.b; InputScript_RecordMouseMove(frame, mouseX, mouseY); MouseEvents(1, mouseX, mouseY); break;
		case Ev::LDown: InputScript_RecordMouseButtons(frame, 2, mouseX, mouseY); MouseEvents(2, mouseX, mouseY); break;
		case Ev::LUp: InputScript_RecordMouseButtons(frame, 4, mouseX, mouseY); MouseEvents(4, mouseX, mouseY); break;
		case Ev::RDown: InputScript_RecordMouseButtons(frame, 8, mouseX, mouseY); MouseEvents(8, mouseX, mouseY); break;
		case Ev::RUp: InputScript_RecordMouseButtons(frame, 0x10, mouseX, mouseY); MouseEvents(0x10, mouseX, mouseY); break;
		case Ev::Quit:
			printf("input script: quit at frame %u\n", frame);
			fflush(stdout);
			if (recordFile) fclose(recordFile);
			std::_Exit(0);
		}
	}
}

bool InputScript_Exhausted(uint32_t frame, uint32_t graceFrames)
{
	return !events.empty() && nextEvent >= events.size() && frame >= events.back().frame + graceFrames;
}

bool InputScript_StartRecording(const std::string& path)
{
	recordFile = fopen(path.c_str(), "w");
	if (!recordFile) {
		std::cerr << "remc1: cannot write " << path << "\n";
		return false;
	}
	fprintf(recordFile, "# remc1 input recording (frame-based, see portability/InputScript.h)\n");
	fprintf(recordFile, "# T lines: timer ticks applied at each sync point (see portability/port_timer.h)\n");
	PortTimer_SetRecordFile(recordFile);
	return true;
}

// A pending mouse position is kept in memory and written only when something else is recorded or
// the frame changes, so motion is collapsed to one line per frame without rewriting the file.
namespace {
bool pendingMouse = false;
uint32_t pendingFrame = 0;
int pendingX = 0, pendingY = 0;

void FlushPendingMouse()
{
	if (pendingMouse && recordFile)
		fprintf(recordFile, "%u mouse %d %d\n", pendingFrame, pendingX, pendingY);
	pendingMouse = false;
}
} // namespace

void InputScript_RecordKey(uint32_t frame, bool down, int sdlScancode)
{
	if (!recordFile)
		return;
	FlushPendingMouse();
	fprintf(recordFile, "%u %s %s\n", frame, down ? "keydown" : "keyup",
		SDL_GetScancodeName((SDL_Scancode)sdlScancode));
	fflush(recordFile);
}

void InputScript_RecordMouseMove(uint32_t frame, int x, int y)
{
	if (!recordFile)
		return;
	// every move is kept (collapsing them per frame lost intermediate positions live play applied)
	FlushPendingMouse();
	pendingMouse = true;
	pendingFrame = frame;
	pendingX = x;
	pendingY = y;
}

void InputScript_RecordMouseButtons(uint32_t frame, uint32_t buttonBits, int x, int y)
{
	if (!recordFile)
		return;
	InputScript_RecordMouseMove(frame, x, y);
	FlushPendingMouse();
	if (buttonBits & 2) fprintf(recordFile, "%u ldown\n", frame);
	if (buttonBits & 4) fprintf(recordFile, "%u lup\n", frame);
	if (buttonBits & 8) fprintf(recordFile, "%u rdown\n", frame);
	if (buttonBits & 0x10) fprintf(recordFile, "%u rup\n", frame);
	fflush(recordFile);
}

void InputScript_FlushRecording()
{
	FlushPendingMouse();
	if (recordFile)
		fflush(recordFile);
}

bool InputScript_Playing()
{
	return !events.empty();
}

void InputScript_CheckState(uint32_t frame, uint64_t hash)
{
	if (recordFile && frame % 30 == 0) {
		FlushPendingMouse();
		fprintf(recordFile, "C %u %016llx\n", frame, (unsigned long long)hash);
	}
	auto it = checksums.find(frame);
	if (it != checksums.end() && it->second != hash && !desyncReported) {
		desyncReported = true;
		fprintf(stderr, "DESYNC at frame %u: game state differs from the recording (expected %016llx, got %016llx)\n",
			frame, (unsigned long long)it->second, (unsigned long long)hash);
	}
}
