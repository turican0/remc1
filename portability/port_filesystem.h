#pragma once
#ifndef PORT_FILESYSTEM_H
#define PORT_FILESYSTEM_H

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>
#include <string>
#include "mctypes.h"
#include "fcaseopen.h"

#ifdef _MSC_VER
	#include <windows.h>
	#include <direct.h>
	#include <io.h>
	#include "dirent-x.h"
#else
	#include <dirent.h>
	#include <unistd.h>
	#include <limits.h>

	#define MAX_PATH PATH_MAX
	#define _chdir chdir
#endif

//#include <stdlib.h>
//#include <string.h>
//#include <ctype.h>

#define DEBUG_MKDIR
#define DEBUG_START
#define DEBUG_FILEOPS
#define DEBUG_LOADSAVE

//#define DEBUG_PRINT_DEBUG_TO_SCREEN

extern char gameFolder[512];
extern char cdFolder[512];
extern char bigGraphicsFolder[512];


bool file_exists(const char * filename);
FILE* mycreate(const char* path, uint32_t flags);
int32_t myaccess(const char* path, uint32_t flags);
int32_t mymkdir(const char* path);

FILE* myopen(const char* path, int pmode, uint32_t flags);
int myclose(FILE* descriptor);
int32_t mylseek(FILE* filedesc, x_DWORD position, char type);
int32_t myfseek(FILE* filedesc, x_DWORD position, char type);

void AdvReadfile(const char* path, uint8_t* buffer);
void ReadGraphicsfile(const char* path, uint8_t* buffer,long size=-1);
bool ExistGraphicsfile(const char* path);

long myftell(FILE* decriptor);

int DirExists(const char* path);

FILE* myopent(char* path, char* type);

typedef struct {
	int number;
	char dir[256][512];
} dirsstruct;

dirsstruct getListDir(char* dirname);
//dirsstruct getListDirFix(char* dirname);
void FixDir(char* outdirname, char* indirname);

int dos_getdrive(int* a);

std::string get_exe_path();

uint64_t dos_getdiskfree(uint8_t a, int16_t a2);

#ifdef _MSC_VER
#endif

void debug_printf(const char* format, ...);

std::string GetSubDirectoryPath(const char* subDirectory);

std::string GetSubDirectoryPath(const char* gamepath, const char* subDirectory);

std::string GetSubDirectoryFilePath(const char* subDirectory, const char* fileName);

std::string GetSubDirectoryFile(const char* gamepath, const char* subDirectory, const char* fileName);

std::string GetSaveGameFile(const char* gamepath, int16_t index);

int GetDirectory(char* directory, const char* filePath);

#endif //PORT_FILESYSTEM
