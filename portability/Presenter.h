#pragma once
// Presentation backend: takes the game's finished frame (8-bit indexed, or RGB565 in the rare 16-bit
// modes) plus the current VGA palette and shows it. The game never talks to SDL video directly.
//
// Implementations:
//   SdlPresenter  - SDL2 renderer + streaming texture (default)
//   NullPresenter - no output (headless runs, tests)
// An OpenGL presenter (palette lookup in a shader, CRT effects, ...) only has to implement this
// interface.

#include <cstdint>
#include <memory>

struct SDL_Window;

struct PresenterConfig {
	int windowWidth = 1280;
	int windowHeight = 960;
	bool fullscreen = false;
	bool grabMouse = true;
	bool vsync = true;
	int logicalWidth = 640;  // game mouse/VGA space; every mode is shown stretched to this 4:3 area
	int logicalHeight = 480;
	const char* title = "Magic Carpet";
	// Debug: every dumpEvery-th presented frame, save what was actually rendered (after palette
	// conversion and scaling) as <dumpDir>/presented######.bmp. Empty = off.
	const char* dumpDir = "";
	int dumpEvery = 30;
};

struct FrameView {
	const uint8_t* pixels = nullptr;
	int width = 0;
	int height = 0;
	int pitch = 0;          // bytes per row
	int bytesPerPixel = 1;  // 1 = palette indices, 2 = RGB565
};

class IPresenter {
public:
	virtual ~IPresenter() = default;
	virtual bool Init(const PresenterConfig& config) = 0;
	virtual void Shutdown() = 0;
	// palette: 256 entries of 8-bit R,G,B
	virtual void SetPalette(const uint8_t* rgb768) = 0;
	virtual void Present(const FrameView& frame) = 0;
	virtual void ToggleFullscreen() = 0;
	virtual void SetMouseGrab(bool grab) = 0;
	// Logical (640x480) <-> window pixel coordinates, for mouse warping.
	virtual void LogicalToWindow(float lx, float ly, int& wx, int& wy) = 0;
	virtual SDL_Window* Window() = 0;
	// Display refresh rate when Present() blocks on vertical sync, else 0 (frame pacing uses it).
	virtual int VsyncRefreshHz() = 0;
};

std::unique_ptr<IPresenter> CreateSdlPresenter();
std::unique_ptr<IPresenter> CreateNullPresenter();
