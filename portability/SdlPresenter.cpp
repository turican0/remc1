#include "Presenter.h"

#ifdef _MSC_VER
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

class SdlPresenter : public IPresenter {
public:
	bool Init(const PresenterConfig& c) override
	{
		config_ = c;
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			fprintf(stderr, "SDL video init failed: %s\n", SDL_GetError());
			return false;
		}
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
		Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
		if (c.fullscreen)
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		window_ = SDL_CreateWindow(c.title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			c.windowWidth, c.windowHeight, flags);
		if (!window_) {
			fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
			return false;
		}
		Uint32 rflags = SDL_RENDERER_ACCELERATED | (c.vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
		renderer_ = SDL_CreateRenderer(window_, -1, rflags);
		if (!renderer_)
			renderer_ = SDL_CreateRenderer(window_, -1, 0); // software fallback (e.g. dummy driver)
		if (!renderer_) {
			fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
			return false;
		}
		SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
		// Letterbox to 4:3 and report mouse positions in logical (game) units.
		SDL_RenderSetLogicalSize(renderer_, c.logicalWidth, c.logicalHeight);
		SDL_ShowCursor(SDL_DISABLE);
		SetMouseGrab(c.grabMouse);
		SDL_RenderClear(renderer_);
		SDL_RenderPresent(renderer_);
		return true;
	}

	void Shutdown() override
	{
		if (texture_) SDL_DestroyTexture(texture_);
		if (renderer_) SDL_DestroyRenderer(renderer_);
		if (window_) SDL_DestroyWindow(window_);
		texture_ = nullptr;
		renderer_ = nullptr;
		window_ = nullptr;
	}

	void SetPalette(const uint8_t* rgb) override
	{
		for (int i = 0; i < 256; i++)
			palette_[i] = 0xFF000000u | (uint32_t(rgb[i * 3]) << 16) | (uint32_t(rgb[i * 3 + 1]) << 8) | rgb[i * 3 + 2];
	}

	void Present(const FrameView& f) override
	{
		if (!renderer_ || !f.pixels || f.width <= 0 || f.height <= 0)
			return;
		if (!texture_ || texW_ != f.width || texH_ != f.height) {
			if (texture_)
				SDL_DestroyTexture(texture_);
			texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, f.width, f.height);
			texW_ = f.width;
			texH_ = f.height;
		}
		void* dst;
		int dstPitch;
		if (SDL_LockTexture(texture_, nullptr, &dst, &dstPitch) != 0)
			return;
		for (int y = 0; y < f.height; y++) {
			uint32_t* out = (uint32_t*)((uint8_t*)dst + y * dstPitch);
			const uint8_t* in = f.pixels + y * f.pitch;
			if (f.bytesPerPixel == 1) {
				for (int x = 0; x < f.width; x++)
					out[x] = palette_[in[x]];
			}
			else { // RGB565
				const uint16_t* in16 = (const uint16_t*)in;
				for (int x = 0; x < f.width; x++) {
					uint16_t p = in16[x];
					uint32_t r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
					out[x] = 0xFF000000u | ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
				}
			}
		}
		SDL_UnlockTexture(texture_);
		SDL_RenderClear(renderer_);
		// every VGA mode filled a 4:3 CRT: stretch the frame over the whole logical area
		SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
		frames_++;
		if (config_.dumpDir && *config_.dumpDir && config_.dumpEvery > 0 && frames_ % config_.dumpEvery == 0)
			DumpRendered();
		SDL_RenderPresent(renderer_);
	}

	void DumpRendered()
	{
		int w, h;
		if (SDL_GetRendererOutputSize(renderer_, &w, &h) != 0)
			return;
		SDL_Surface* shot = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
		if (!shot)
			return;
		// read the whole output (incl. letterbox) - temporarily drop the logical size so the read rect is in pixels
		SDL_RenderSetLogicalSize(renderer_, 0, 0);
		if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, shot->pixels, shot->pitch) == 0) {
			char path[512];
			snprintf(path, sizeof(path), "%s/presented%06u.bmp", config_.dumpDir, frames_);
			SDL_SaveBMP(shot, path);
		}
		SDL_RenderSetLogicalSize(renderer_, config_.logicalWidth, config_.logicalHeight);
		SDL_FreeSurface(shot);
	}

	void ToggleFullscreen() override
	{
		bool fs = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
		SDL_SetWindowFullscreen(window_, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
	}

	void SetMouseGrab(bool grab) override
	{
		if (window_)
			SDL_SetWindowGrab(window_, grab ? SDL_TRUE : SDL_FALSE);
	}

	void LogicalToWindow(float lx, float ly, int& wx, int& wy) override
	{
		SDL_RenderLogicalToWindow(renderer_, lx, ly, &wx, &wy);
	}

	SDL_Window* Window() override { return window_; }
	int VsyncRefreshHz() override
	{
		SDL_RendererInfo info;
		if (!renderer_ || SDL_GetRendererInfo(renderer_, &info) != 0 || !(info.flags & SDL_RENDERER_PRESENTVSYNC))
			return 0;
		SDL_DisplayMode mode;
		if (SDL_GetWindowDisplayMode(window_, &mode) != 0 || mode.refresh_rate <= 0)
			return 0;
		return mode.refresh_rate;
	}

private:
	PresenterConfig config_;
	SDL_Window* window_ = nullptr;
	SDL_Renderer* renderer_ = nullptr;
	SDL_Texture* texture_ = nullptr;
	int texW_ = 0, texH_ = 0;
	unsigned frames_ = 0;
	uint32_t palette_[256] = {};
};

class NullPresenter : public IPresenter {
public:
	bool Init(const PresenterConfig&) override { return true; }
	void Shutdown() override {}
	void SetPalette(const uint8_t*) override {}
	void Present(const FrameView&) override {}
	void ToggleFullscreen() override {}
	void SetMouseGrab(bool) override {}
	void LogicalToWindow(float lx, float ly, int& wx, int& wy) override { wx = (int)lx; wy = (int)ly; }
	SDL_Window* Window() override { return nullptr; }
	int VsyncRefreshHz() override { return 0; }
};

} // namespace

std::unique_ptr<IPresenter> CreateSdlPresenter() { return std::make_unique<SdlPresenter>(); }
std::unique_ptr<IPresenter> CreateNullPresenter() { return std::make_unique<NullPresenter>(); }
