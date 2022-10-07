#pragma once

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

typedef struct {//2049
	uint16 var_u16_13325;
	uint8 var_u8_13327;
	uint32 var_u32_13347;
	//2049 * str_AE400_AE3F0->var_u16_8 + dword_AE400_AE3F0() + 13347

}
TypeStrAE400_13347;

typedef union {
	uint16 a16[2];
	uint32 a32;
} un16_32;

typedef struct {// len 20
	uint8_t stub[20];
}
Type_AE400_20;
typedef struct {// len 48
	uint8_t stub[48];
}
Type_AE400_9177;
typedef struct {// len 164000
	uint8_t stub[164000];
}
Type_AE400_29795;
typedef struct {// len 106
	uint8_t stub[106];
}
Type_AE400_232607;

typedef struct {
	uint8 var_u8_1;
	uint16 var_u16_8;
	uint16 var_u16_10;
	Type_AE400_20 str_20;// len 20
	uint32 var_u32_32;
	uint16 var_u16_36;
	uint16 var_u16_38;
	uint32 var_u32_40;
	char textBuffer_117[32];
	un16_32 var_u32_576;
	uint8 var_u8_580;
	uint8 var_u8_8597;
	uint8 var_u8_8598;
	uint8 var_u8_8599;

	uint8 var_u8_8600;
	uint8 var_u8_8601;
	uint8 var_u8_8602;

	uint8 var_u8_8606;
	uint8 var_u8_8608;

	uint8 var_u8_8621;
	uint8 var_u8_8622;
	uint8 var_u8_8623;
	uint8 var_u8_8624;
	uint8 var_u8_8625;
	uint8 var_u8_8626;
	uint8 var_u8_8627;
	uint8 var_u8_8628;
	uint8 var_u8_8629;
	uint8 var_u8_8630;
	uint8 var_u8_8631;
	Type_AE400_9177 str_9177;// len 48
	TypeStrAE400_13347 str_13347[80];//fix
	Type_AE400_29795 str_29795;// len 164000
	Type_AE400_232607 str_232607;// len 106
} Type_str_AE400_AE3F0;

extern Type_str_AE400_AE3F0* str_AE400_AE3F0;

typedef struct {// len 16
	uint8_t stub[16];
}
Type_AE408_93;
typedef struct {// len 14
	uint8_t stub[14];
}
Type_AE408_184;
typedef struct {// len 80
	uint8_t stub[80];
}
Type_AE408_36382;


typedef struct {
	uint8 var_u8_0;
	uint8 var_u8_1;
	uint8 var_u8_2;
	uint8 var_u8_3;

	uint8 var_u8_8;
	uint16 var_u16_13;
	uint16 var_u16_15;
	uint16 var_u16_17;
	uint8 var_u8_23;
	uint32 var_u32_25;
	char var_u8_29[32];
	char var_u8_61[32];
	Type_AE408_93 str_93;// len 16
	char var_u8_117[32];//fix size
	uint8 var_u8_150;
	uint8 var_u8_151;
	uint8 var_u8_152;
	uint8_t* var_u32_168;
	uint32 var_u32_172;
	Type_AE408_184 str_184;// len 14
	uint32 var_u32_206;
	uint32 var_u32_210;
	uint32 var_u32_36378;
	Type_AE408_36382 str_36382;// len 80
	uint32 var_u32_36462;
	uint32 var_u32_36466;
	uint32 var_u32_36470;
	uint32 var_u32_36474;
} Type_str_AE408_AE3F8;
/*
typedef struct {//66
	int8_t byte_0;
	int8_t byte_1;
	int16_t word_6;
	int8_t byte_49;
}
Type_str_9398C;
*/

typedef union {
	uint8_t* p;
	uint32_t align;
} p64align;

typedef struct {//lenght 66(changed to 70)
	uint8_t ncb_command_0;
	uint8_t ncb_retcode_1;
	uint8_t ncb_lsn_2;
	uint8_t ncb_num_3;
	p64align ncb_buffer_4;
	uint16_t ncb_bufferLength_8;
	char ncb_callName_10[16];
	char ncb_name_26[16];
	uint8_t ncb_rto_42;
	uint8_t ncb_sto_43;
	uint8_t ncb_cmd_cplt_49;//lock for end command
}
myNCB;

#pragma pack (1)
typedef struct {//44 lenght
	char path[28];
	uint8_t** colorPalette_var28;//1C // Palette
	uint8_t** var32_end_buffer;//20 // maybe buffer
	int32_t var36_size_buffer;//24 //maybe file size
	uint32_t var40_alloc_type;//28
} Pathstruct;
#pragma pack (16)



typedef union { uint8_t bytes[4]; int32 size; }
Type_fileSize;

extern Type_str_AE408_AE3F8* str_AE408_AE3F8;

int dword_AE400_AE3F0();

void dword_AE400_AE3F0(int input);

int dword_AE408_AE3F8();

void dword_AE408_AE3F8(int input);

int sub_main(int argc, char* argv[]);

void sub_4FF30();
void sub_50030();
void sub_4FEB0();
void sub_4FD20();