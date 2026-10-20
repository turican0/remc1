#pragma once
// Digital sound effects backend (replaces the HMI SOS digital driver used by MC1).
//
// The game's Bullfrog sound layer (sub_48570 & co.) keeps 32 logical channels and talks to the
// driver through a handful of calls (start sample, is-done, stop, set volume). Those HMI entry
// points (sub_6535C, sub_651F8, sub_65965, sub_66118, ...) now forward here; samples are the
// 8-bit unsigned mono PCM from data/sndsN-V.dat (V=0: 11025 Hz, V=1: 22050 Hz).

#include <cstdint>

bool PortSound_Init();            // opens the audio device; false if unavailable (game runs silent)
void PortSound_Shutdown();
bool PortSound_Available();

// rate: sample rate of the loaded bank (11025/22050)
void PortSound_SetSampleRate(int rate);
// Forget converted samples (call when the bank memory is replaced).
void PortSound_ClearCache();

// channel: 0..31 (game channel); volume: 0..0x7FFF; pan: 0..0x7FFF..0xFFFF (left..centre..right)
// loops: 0 = play once, -1 = loop forever, n = play n+1 times
void PortSound_Start(int channel, const uint8_t* data, uint32_t length, int volume, int pan, int loops = 0);
bool PortSound_IsDone(int channel);
void PortSound_Stop(int channel);
void PortSound_SetVolume(int channel, int volume);
void PortSound_ScaleAllVolumes(int scale0x7fff);
int PortSound_ActiveCount();
