#pragma once

#include "portability/port_filesystem.h"

#include <stdio.h>
#include <defs.h>


#define _BYTE  uint8
#define _WORD  uint16
#define _DWORD uint32

extern __int16 word_A9990;
extern int dword_A99A0;
extern int dword_AE450;

typedef struct {//2049
	uint32 var_u32_0;
	//2049 * str_AE400_AE3F0->var_u16_8 + dword_AE400_AE3F0() + 13347

}
TypeStrAE400_13347;

typedef struct {
	uint8 var_u8_1;
	uint16 var_u16_8;
	uint16 var_u16_10;

	char textBuffer_117[32];

	uint8 var_u8_8597;
	uint8 var_u8_8598;
	uint8 var_u8_8599;

	uint8 var_u8_8600;
	uint8 var_u8_8601;
	uint8 var_u8_8602;

	uint8 var_u8_8606;

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
	uint8 var_u8_8;
	uint16 var_u16_13;
	uint16 var_u16_15;
	uint16 var_u16_17;
	uint32 var_u32_25;
	uint8 var_u8_29;
} Type_str_AE408_AE3F8;

extern Type_str_AE408_AE3F8* str_AE408_AE3F8;

int dword_AE400_AE3F0();

void dword_AE400_AE3F0(int input);

int dword_AE408_AE3F8();

void dword_AE408_AE3F8(int input);

int sub_main(int argc, const char** argv, const char** envp);