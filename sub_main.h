#pragma once
#include "utilities/Compare.h"
#include "portability/port_filesystem.h"
#include "portability/port_sdl_vga_mouse.h"
#include "utilities/DataFileIO.h"

#include <stdio.h>
#include <defs.h>

#include <chrono>
#include <thread>

#include <thread>
#include <chrono>
#include <functional>
#include <cstdio>
#include <atomic>


#define _BYTE  uint8
#define _WORD  uint16
#define _DWORD uint32

#pragma pack (1)
typedef struct {//lenght 3
	uint8_t red;
	uint8_t green;
	uint8_t blue;
}
TColor;
#pragma pack (16)

extern int dword_A99A0_A99B8;

extern uint8_t* begSearch_AE3F4_26C3F4_26C3E4;
extern uint8_t* begBuildTab_AE440_AE430_26C440_26C430;
extern uint8_t* begFont0Tab_AE3B8_AE3A8_26C3B8_26C3A8;
extern uint8_t* begPalData_AE424_AE414_26C424_26C414;
extern TColor* begPalMem_AE418_AE408_26C418_26C408;
extern uint8_t* begPointersTab_AE450_AE440_26C450_26C440;
extern TColor* begPalDat_AE428_AE418_26C428_26C418;
extern uint8_t* begTextDat_AE41C_AE40C_26C41C_26C40C;
extern uint8_t* begBlkDat_AE3F0_26C3F0_26C3E0;
extern uint8_t* begSky_AE3D8_AE3C8_26C3D8_26C3C8;
extern uint8_t* begBscreen_AE3FC_AE3EC_26C3FC_26C3EC;
extern uint8_t* begSprTab_AE42C_AE41C_26C42C_26C41C;

int sub_main(int argc, char* argv[]);

void sub_4FF30();
void sub_50030();
void sub_4FEB0();
void sub_4FD20();