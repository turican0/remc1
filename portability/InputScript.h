#pragma once
// Scripted / recorded input, keyed by presented-frame number.
//
// Script format (one event per line, '#' comments):
//   <frame> key <SDL scancode name>        press, auto-release 3 frames later   e.g. "120 key Return"
//   <frame> keydown <name> / keyup <name>
//   <frame> mouse <x> <y>                   move, in game mouse space (0..639, 0..479)
//   <frame> lclick [x y] / rclick [x y]     press (optionally moving first), auto-release 3 frames later
//   <frame> ldown / lup / rdown / rup
//   <frame> quit                            exit the process
// Scancode names are SDL_GetScancodeFromName() names ("Return", "Escape", "Space", "Up", "A", "F1", ...).
//
// Recording (--record-input <file>) writes the same format from live SDL input, so a play session
// can be replayed with --input-script.

#include <string>
#include <cstdint>

bool InputScript_Load(const std::string& path);
bool InputScript_StartRecording(const std::string& path);
// Called once per presented frame, before SDL events are pumped.
void InputScript_OnFrame(uint32_t frame);
// true once a loaded script has no events left and graceFrames have passed since its last one
bool InputScript_Exhausted(uint32_t frame, uint32_t graceFrames);
// true while an input script is loaded: live input is then ignored (it would desynchronise the replay)
bool InputScript_Playing();
// Called once per presented frame with a hash of the game state: recordings store it every 30 frames
// ("C" lines); replays compare and report the first frame that differs ("DESYNC at frame N").
void InputScript_CheckState(uint32_t frame, uint64_t hash);

// Recording hooks (called from the SDL event pump)
void InputScript_RecordKey(uint32_t frame, bool down, int sdlScancode);
void InputScript_RecordMouseMove(uint32_t frame, int x, int y);
void InputScript_RecordMouseButtons(uint32_t frame, uint32_t buttonBits, int x, int y);
// Writes any buffered mouse position (called once per presented frame).
void InputScript_FlushRecording();
