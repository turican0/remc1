#pragma once

#include "engine/Basic.h"

#ifndef F_OK
#define F_OK 00
#endif /* F_OK */

void add_compare(uint32_t adress, bool debugafterload, bool compareGraphics = false, int stopstep = -1, bool skip = false, int exitindex = 1000000, int skip2 = 0);
uint32_t compare_with_sequence(const char* filename, const uint8_t* adress, uint32_t  /*adressdos*/, long count, long size1, uint32_t size2, uint8_t* origbyte, uint8_t* copybyte, long offset, bool regressions);
void allert_error();

void SaveCompare(char* name, int value, int len = -1, uint8* sequence = nullptr);
void CompareWith(char* name, int value, int len = -1, uint8* sequence = nullptr);

