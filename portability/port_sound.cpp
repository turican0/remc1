#include "port_sound.h"
#include "port_timer.h"

#ifdef _MSC_VER
#include "SDL.h"
#include "SDL_mixer.h"
#else
#include "SDL2/SDL.h"
#include "SDL2/SDL_mixer.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <utility>

namespace {

constexpr int kChannels = 32;

bool available = false;
int deviceRate = 44100;
Uint16 deviceFormat = AUDIO_S16SYS;
int deviceChannels = 2;
int bankRate = 22050;
int channelVolume[kChannels];
// Game-visible channel state is driven by timer ticks, not by the audio device, so it is identical
// in live play, deterministic runs and replays (and with no audio device at all).
uint64_t channelEndTick[kChannels];

// converted samples, keyed by source pointer + length (bank memory stays loaded while in use)
std::map<std::pair<const uint8_t*, uint32_t>, Mix_Chunk*> cache;

Mix_Chunk* Convert(const uint8_t* data, uint32_t length)
{
	auto key = std::make_pair(data, length);
	auto it = cache.find(key);
	if (it != cache.end())
		return it->second;
	SDL_AudioCVT cvt;
	if (SDL_BuildAudioCVT(&cvt, AUDIO_U8, 1, bankRate, deviceFormat, deviceChannels, deviceRate) < 0)
		return nullptr;
	cvt.len = (int)length;
	cvt.buf = (Uint8*)SDL_malloc((size_t)length * cvt.len_mult);
	if (!cvt.buf)
		return nullptr;
	memcpy(cvt.buf, data, length);
	if (cvt.needed && SDL_ConvertAudio(&cvt) < 0) {
		SDL_free(cvt.buf);
		return nullptr;
	}
	Mix_Chunk* chunk = Mix_QuickLoad_RAW(cvt.buf, (Uint32)(cvt.needed ? cvt.len_cvt : cvt.len));
	if (!chunk) {
		SDL_free(cvt.buf);
		return nullptr;
	}
	chunk->allocated = 1; // Mix_FreeChunk frees buf
	cache[key] = chunk;
	return chunk;
}

int MixVolume(int volume0x7fff)
{
	if (volume0x7fff < 0) volume0x7fff = 0;
	if (volume0x7fff > 0x7FFF) volume0x7fff = 0x7FFF;
	return volume0x7fff * MIX_MAX_VOLUME / 0x7FFF;
}

} // namespace

bool PortSound_Init()
{
	if (available)
		return true;
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "sound: SDL audio init failed: %s\n", SDL_GetError());
		return false;
	}
	if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) < 0) {
		fprintf(stderr, "sound: Mix_OpenAudio failed: %s\n", Mix_GetError());
		return false;
	}
	Mix_QuerySpec(&deviceRate, &deviceFormat, &deviceChannels);
	Mix_AllocateChannels(kChannels);
	for (int i = 0; i < kChannels; i++)
		channelVolume[i] = 0x7FFF;
	available = true;
	return true;
}

void PortSound_Shutdown()
{
	if (!available)
		return;
	Mix_HaltChannel(-1);
	PortSound_ClearCache();
	Mix_CloseAudio();
	available = false;
}

bool PortSound_Available()
{
	return available;
}

void PortSound_SetSampleRate(int rate)
{
	if (rate > 0 && rate != bankRate) {
		bankRate = rate;
		PortSound_ClearCache();
	}
}

void PortSound_ClearCache()
{
	if (available)
		Mix_HaltChannel(-1);
	for (auto& kv : cache)
		Mix_FreeChunk(kv.second);
	cache.clear();
}

void PortSound_Start(int channel, const uint8_t* data, uint32_t length, int volume, int pan, int loops)
{
	if (channel < 0 || channel >= kChannels || !data || !length)
		return;
	if (loops < 0)
		channelEndTick[channel] = UINT64_MAX; // until stopped
	else
		channelEndTick[channel] = PortTimer_TickCount() + 1 + (uint64_t)((double)length * (loops + 1) * PortTimer_TickHz() / bankRate);
	static const bool debugSound = getenv("REMC1_DEBUG_SOUND") != nullptr;
	if (debugSound)
		fprintf(stderr, "sound: start ch=%d len=%u vol=%d pan=%d tick=%llu\n", channel, length, volume, pan,
			(unsigned long long)PortTimer_TickCount());
	if (!available)
		return;
	Mix_Chunk* chunk = Convert(data, length);
	if (!chunk)
		return;
	channelVolume[channel] = volume;
	Mix_HaltChannel(channel);
	Mix_Volume(channel, MixVolume(volume));
	// pan: 0 = left, 0x8000 = centre, 0xFFFF = right
	int p = pan & 0xFFFF;
	int right = p >= 0x8000 ? 255 : p * 255 / 0x8000;
	int left = p <= 0x8000 ? 255 : (0xFFFF - p) * 255 / 0x7FFF;
	Mix_SetPanning(channel, (Uint8)left, (Uint8)right);
	Mix_PlayChannel(channel, chunk, loops);
}

bool PortSound_IsDone(int channel)
{
	if (channel < 0 || channel >= kChannels)
		return true;
	return PortTimer_TickCount() >= channelEndTick[channel];
}

void PortSound_Stop(int channel)
{
	if (channel < 0 || channel >= kChannels)
		return;
	channelEndTick[channel] = 0;
	if (available)
		Mix_HaltChannel(channel);
}

void PortSound_SetVolume(int channel, int volume)
{
	if (!available || channel < 0 || channel >= kChannels)
		return;
	channelVolume[channel] = volume;
	Mix_Volume(channel, MixVolume(volume));
}

void PortSound_ScaleAllVolumes(int scale)
{
	if (!available)
		return;
	for (int i = 0; i < kChannels; i++) {
		channelVolume[i] = channelVolume[i] * scale / 0x7FFF;
		Mix_Volume(i, MixVolume(channelVolume[i]));
	}
}

int PortSound_ActiveCount()
{
	int n = 0;
	for (int i = 0; i < kChannels; i++)
		if (!PortSound_IsDone(i))
			n++;
	return n;
}
