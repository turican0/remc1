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

typedef struct {
	int var0;
} Type_str_AE400_AE3F0;

extern Type_str_AE400_AE3F0 str_AE400_AE3F0;

int dword_AE400_AE3F0();

int sub_main(int argc, const char** argv, const char** envp);