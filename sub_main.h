#pragma once

#include "portability/port_filesystem.h"
#include "utilities/DataFileIO.h"

#include <stdio.h>
#include <defs.h>


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

extern uint8_t* begSearch_AE3F4_26C3F4;
extern uint8_t* begBuild00Tab_AE440_26C440;
extern uint8_t* begFont0Tab_AE3B8_26C3B8;
extern uint8_t* begPalData_AE424_26C424;
extern uint8_t* begPalMem_AE418_26C418;
extern uint8_t* begPointersTab_AE450_26C450;
extern TColor* begPal00Dat_AE428_AE418_26C428;
extern uint8_t* begTextDat_AE41C_AE40C_26C41C;
extern uint8_t* begBlkDat_AE3F0_26C3F0;
extern uint8_t* begSky_AE3D8_26C3D8;
extern uint8_t* begBscreen_AE3FC_26C3FC;
extern uint8_t* begSpr00Tab_AE42C_26C42C;

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

typedef struct {
	uint8 var_u8_1;
	uint16 var_u16_8;
	uint16 var_u16_10;

	char textBuffer_117[32];

	un16_32 var_u32_576;

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

	TypeStrAE400_13347 str_13347[80];//fix

} Type_str_AE400_AE3F0;

extern Type_str_AE400_AE3F0* str_AE400_AE3F0;

typedef struct {
	uint8 var_u8_0;
	uint8 var_u8_1;
	uint8 var_u8_8;
	uint16 var_u16_13;
	uint16 var_u16_15;
	uint16 var_u16_17;
	uint8 var_u8_23;
	uint32 var_u32_25;
	uint8 var_u8_29;
	char var_u8_117[32];//fix size
	uint8 var_u8_151;
	uint8_t* var_u32_168;
	uint32 var_u32_172;
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

int sub_main(int argc, const char** argv, const char** envp);