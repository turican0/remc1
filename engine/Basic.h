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

//#include <stdio.h>
//#include <defs.h>
#include <string>
#include "../engine/CommandLineParser.h"
#include "../portability/port_filesystem.h"
#include "../portability/port_time.h"

#pragma pack (1)

typedef struct {//2049
	uint32 var_u32_13323;
	uint16 var_u16_13325;
	uint8 var_u8_13327;
	uint8 stub0[5];
	uint16 var_u16_13333;
	uint8 stub1[12];
	uint32 var_u32_13347;
	//2049 * str_AE400_AE3F0->var_u16_8 + dword_AE400_AE3F0() + 13347

}
TypeStrAE400_13323;

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
typedef struct {// len 164
	uint8_t stub1[16];
	uint32 var_u32_29811;
	uint8_t stub2[44];
	uint8_t var_u8_29859;
	uint8_t stub3[68];
	uint32 var_u32_29931;//136
	uint8_t stub4[20];
	uint32 var_u32_29955;//160
}
Type_AE400_29795;
typedef struct {// len 106
	uint32 var_u32_232607;
	uint8_t var_u8_232611[96];
	uint8_t stubb[6];
}
Type_AE400_232607;

typedef struct {// len 18
	uint16_t data_0;
	uint16_t data_2;
	uint16_t data_4;
	uint16_t data_6;
	uint16_t data_8;
	uint16_t data_10;
	uint16_t data_12;
	uint16_t data_14;
	uint16_t data_16;
}
Type_1090;

typedef struct {//len 38812
	uint8 stub_0[1072];
	Type_1090 str_1072[1];	
	Type_1090 var_u16_1090[1999];//size 9 count 1000
	//37072 end
	uint8 stub_37072[1730];
	//uint8 stub_193795[37710];
	uint16 var_u16_232597;
	uint8 stub15[8];
}
Type_AE400_193795;

typedef struct {//size 232713
	uint8 stub0[1];
	uint8 var_u8_1;
	uint8 stub0b[2];
	uint32 var_u32_4;
	uint16 var_u16_8;
	uint16 var_u16_10;
	Type_AE400_20 str_12;// len 20
	uint32 var_u32_32;
	uint16 var_u16_36;
	uint16 var_u16_38;
	uint32 var_u32_40;
	uint8 stub4[532];
	un16_32 var_u32_576;
	uint8 var_u8_580;
	uint8 stub4b[12];
	Type_AE400_29795* var_u32_593[1000];
	uint32 var_u32_4593;
	Type_AE400_29795* var_u32_4597[1000];
	uint8 var_u8_8597;
	uint8 var_u8_8598;
	uint8 var_u8_8599;
	uint8 var_u8_8600;
	uint8 var_u8_8601;
	uint8 var_u8_8602;
	uint8 stub8[3];
	uint8 var_u8_8606;
	uint8 stub9[1];
	uint8 var_u8_8608;
	uint8 stub10[12];
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
	uint8 stub11[545];
	Type_AE400_9177 str_9177;// len 48
	uint8 stub12[4100];
	TypeStrAE400_13323 str_13323[8];//fix
	uint8 stub13[16262];
	Type_AE400_29795 str_29795[1000];// len 164000	
	Type_AE400_193795 str_193795;
	Type_AE400_232607 str_232607;// len 106
} Type_str_AE400_AE3F0;

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


typedef struct {//size 36478
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
	char textBuffer_117[32];//fix size
	uint8 var_u8_150;
	uint8 var_u8_151;
	uint8 var_u8_152;
	uint8_t* var_u32_168;
	uint32 var_u32_172;
	Type_AE408_184 str_184;// len 14
	uint32 var_u32_188;
	uint32 var_u32_192;
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

typedef struct {//44 lenght
	char path[28];
	uint8_t** colorPalette_var28;//1C // Palette
	uint8_t** var32_end_buffer;//20 // maybe buffer
	int32_t var36_size_buffer;//24 //maybe file size
	uint32_t var40_alloc_type;//28
} Pathstruct;

typedef union { uint8_t bytes[4]; int32 size; }
Type_fileSize;
#pragma pack (16)

extern uint8_t* readBuffer_12EFF4;

extern __int16 mapEntityIndex_10C1E0[0x10000];
extern char mapTerrainType_CC1E0[0x10000];
extern char mapHeightmap_DC1E0[0x10000];
extern char mapShading_EC1E0[0x10000];
extern char mapAngle_FC1E0[0x10000];

extern Type_str_AE400_AE3F0* str_AE400_AE3F0;
extern Type_str_AE408_AE3F8* str_AE408_AE3F8;

int dword_AE400_AE3F0();

//void dword_AE400_AE3F0(int input);

int dword_AE408_AE3F8();

void dword_AE408_AE3F8(int input);
