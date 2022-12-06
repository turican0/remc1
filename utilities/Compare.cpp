#include "Compare.h"

std::string regressionsDataPath = "c:/prenos/remc1-second/Debug/regressions/";

uint32 HashFromStr(char* name, int plus) {
	int index = 0;
	uint32 result = 0;
	while (name[index])
	{
		result += name[index];
		result <<= 3;
		index++;
	}
	result += plus;
	return result;
}

char buffer[512];
void SaveCompare(char* name, int value, int len, uint8* sequence)
{
	uint32 locIndex = getcompindex(HashFromStr(name, 0));

	if (len == -1)
	{
		len = 4;
		sequence = (uint8*)&value;
	}
	sprintf(buffer, "%s%s.dat", regressionsDataPath.c_str(), name);
	FILE* file;
	if (locIndex == 0)
		file = fopen(buffer, "wb");
	else
		file = fopen(buffer, "ab");
	fwrite(sequence, 1, len, file);
	fclose(file);
};
void CompareWith(char* name, int value, int len, uint8* sequence)
{
	uint32 locIndex = getcompindex(HashFromStr(name, 0));

	uint8 compBuffer[512];
	if (len == -1)
	{
		len = 4;
		sequence = (uint8*)&value;
	}
	sprintf(buffer, "%s%s.dat", regressionsDataPath.c_str(), name);
	FILE* file = fopen(buffer, "rb");
	fseek(file, len * locIndex, SEEK_SET);
	fread(compBuffer, 1, len, file);
	fclose(file);
	for (int i = 0; i < len; i++)
	{
		if (compBuffer[i] != sequence[i])
		{
			allert_error();
		}
	}
};

void allert_error() {
	int a = 10;
	int b = 0;
	int c = a / b;//this is generate error
}

//for debuging
int countcompindexes = 0;
typedef struct {
	int index;
	uint32_t adress;
} type_compstr;

type_compstr compstr[100];
int getcompindex(uint32_t adress) {
	bool finded = false;
	int findindex = 0;
	for (int i = 0; i < countcompindexes; i++)
	{
		if (compstr[i].adress == adress)
		{
			finded = true;
			findindex = i;
			break;
		}
	}
	if (finded)
	{
		compstr[findindex].index++;
		return compstr[findindex].index;
	}
	else
	{
		compstr[countcompindexes].adress = adress;
		countcompindexes++;
		return 0;
	}
};

uint32_t compare_with_sequence(const char* filename, const uint8_t* adress, uint32_t  /*adressdos*/, long count, long size1, uint32_t size2, uint8_t* origbyte, uint8_t* copybyte, long offset, bool regressions) {
	std::string finddir;
	uint8_t* buffer = (uint8_t*)malloc(size2);
	FILE* fptestepc;
	if (regressions)
		finddir = CommandLineParams.GetMemimagesPath() + std::string("regressions");
	else
		finddir = std::string("../../remc1/Debug/regressions");
	std::string finddir2 = GetSubDirectoryPath("", "");
	std::string findname = finddir2 + finddir + std::string("/sequence-") + filename + ".bin";
	fptestepc = fopen(findname.c_str(), "rb");
	if (fptestepc == NULL)
	{
		mydelay(100);
		fptestepc = fopen(findname.c_str(), "rb");
	}

#ifdef __linux__
	fseek(fptestepc, (long long)count * (long long)size1 + offset, SEEK_SET);
#else
	_fseeki64(fptestepc, (long long)count * (long long)size1 + offset, SEEK_SET);
#endif

	uint32_t i;
	/*for (i = 0; i < count; i++)
	{
		fread_s(buffer,size,1,size, fptestepc);
	}*/

	fread(buffer, size2, 1, fptestepc);
	if (size2 == 320 * 200)
	{
		VGA_Debug_Blit(320, 200, pdwScreenBuffer_12EFF4);
		/*for (int iii = 0; iii < 320 * 200; iii++)pdwScreenBuffer_351628[iii] = buffer[iii];*/
		VGA_Debug_Blit(320, 200, pdwScreenBuffer_12EFF4);
		VGA_Debug_Blit(320, 200, pdwScreenBuffer_12EFF4);
	}
	//for (i = size-1; i >0; i--)
	for (i = 0; i < size2; i++)
	{
		if (buffer[i] != adress[i])
		{
			*origbyte = buffer[i];
			*copybyte = adress[i];
			break;
		}
	}

	if (i < size2) {
		std::cout << "Regression compare sequence error @ function " << __FUNCTION__ << ", line " << __LINE__ << ": " << i << std::endl;
		allert_error();
	}
	free(buffer);
	fclose(fptestepc);
	return(i);
};

int test_D41A0_id_pointer(uint32_t adress) {

	if ((adress >= 593) && (adress < 593 + 1000 * 4))return 2;//event
	if ((adress >= 0x74FF) && (adress < 0x75a3 * 1000 + 8))return 2;//event

	if ((adress >= 0x39d5) && (adress < 0x39d5+4))return 1; //timer
	/*
	if ((adress >= 0x314d) && (adress < 0x3151))return 2;//clock - 4 bytes
	if ((adress >= 0x3999) && (adress < 0x399d))return 2;//clock2 - 4 bytes
	if ((adress >= 0x41e5) && (adress < 0x41e9))return 2;//clock3 - 4 bytes
	if ((adress >= 0x4a31) && (adress < 0x4a35))return 2;//clock4 - 4 bytes
	if ((adress >= 0x527d) && (adress < 0x5281))return 2;//clock5 - 4 bytes
	if ((adress >= 0x5ac9) && (adress < 0x5acd))return 2;//clock6 - 4 bytes
	if ((adress >= 0x6315) && (adress < 0x6319))return 2;//clock7 - 4 bytes
	if ((adress >= 0x6b61) && (adress < 0x6b65))return 2;//clock8 - 4 bytes

	if ((adress >= 0x235) && (adress < 0x236))return 2;//music

	if ((adress >= 0x246) && (adress < 0x2186))return 1;

	if ((adress >= 0x36552) && (adress < 0x36553))return 1;
	if ((adress >= 0x3655c) && (adress < 0x3655d))return 1;
	if ((adress >= 0x3655f) && (adress < 0x36560))return 1;
	if ((adress >= 0x36566) && (adress < 0x36567))return 1;
	if ((adress >= 0x36570) && (adress < 0x36571))return 1;
	if ((adress >= 0x36608) && (adress < 0x36609))return 1;
	if ((adress >= 0x36628) && (adress < 0x36630))return 1;

	for (int j = 1; j < 50; j++)
	{
		if ((adress >= 0x3664C + 0xa + 39 * j) && (adress < 0x3664C + 0xa + 1 + 39 * j))return 1;
	}
	if ((adress >= 0x36620) && (adress < 0x36621))return 1;
	for (uint32_t i = 0; i < 32; i++) {
		if ((adress >= 0x36656 + i * 39) && (adress < 0x36657 + i * 39))return 1;
	}
	for (uint32_t i = 0; i < 0x3e8; i++) {
		if ((adress >= 0x6f2e + i * 168) && (adress < 0x6f37 + i * 168))return 1;
	}
	if ((adress >= 0x36df6) && (adress < 0x36df7))return 1;*/
	return 0;
}

uint32_t compare_with_sequence_D41A0(const char* filename, uint8_t* adress, uint32_t count, uint32_t size, uint8_t* origbyte, uint8_t* copybyte, long offset, bool regressions) {
	std::string finddir;
	uint8_t* buffer = (uint8_t*)malloc(size);
	FILE* fptestepc;
	if (regressions)
		finddir = CommandLineParams.GetMemimagesPath() + std::string("regressions");
	else
		finddir = std::string("../../remc1/Debug/regressions");
	std::string finddir2 = GetSubDirectoryPath("", "");
	std::string findname = finddir2 + finddir + std::string("/sequence-") + filename + ".bin";
	fptestepc = fopen(findname.c_str(), "rb");
	if (fptestepc == NULL)
	{
		mydelay(100);
		fptestepc = fopen(findname.c_str(), "rb");
	}
	fseek(fptestepc, count * size + offset, SEEK_SET);

	fread(buffer, size, 1, fptestepc);
	uint32_t i;
	bool testa, testb;
	for (i = 0; i < size; i++)
	{
		int testx = test_D41A0_id_pointer(i);
		if (testx == 1)
		{
			if (*(uint32_t*)&buffer[i])testa = true;
			else testa = false;
			if (*(uint32_t*)&adress[i])testb = true;
			else testb = false;
			if (testa != testb)
			{
				*origbyte = buffer[i];
				*copybyte = adress[i];
				break;
			}
			i += 3;
		}
		else if (testx == 0) {
			if (buffer[i] != adress[i])
			{
				*origbyte = buffer[i];
				*copybyte = adress[i];
				break;
			}
		}
	}

	if (i < size) {
		std::cout << "Regression compare sequence error @ function " << __FUNCTION__ << ", line " << __LINE__ << ": " << i << std::endl;
		allert_error();
	}
	free(buffer);
	fclose(fptestepc);
	return(i);
};

type_compstr lastcompstr;
void add_compare(uint32_t adress, bool debugafterload, bool compareGraphics, int stopstep, bool skip, int exitindex, int skip2) {
	uint8_t origbyte20 = 0;
	uint8_t remakebyte20 = 0;
	int comp20;

	char buffer1[500];
	sprintf(buffer1, "%08X-0028A1E0", adress);
	char buffer2[500];
	sprintf(buffer2, "%08X-0001F690", adress);
	char buffer3[500];
	sprintf(buffer3, "%08X-002B3A74", adress);
	char buffer4[500];
	sprintf(buffer4, "%08X-002ECFF4", adress);

	if (debugafterload)
	{
		int index = getcompindex(adress);
		if (index >= skip2)
		{
			if (index >= stopstep)
			{
				if (index >= exitindex)
					exit(exitindex);
				if (!skip)
				{
					comp20 = compare_with_sequence(buffer1, (uint8_t*)mapTerrainType_CC1E0, 0x28A1E0, index - skip2, 0x70000, 0x10000, &origbyte20, &remakebyte20, 0, (exitindex != 1000000));
					comp20 = compare_with_sequence(buffer1, (uint8_t*)mapHeightmap_DC1E0, 0x28A1E0, index - skip2, 0x70000, 0x10000, &origbyte20, &remakebyte20, 0x10000, (exitindex != 1000000));
					comp20 = compare_with_sequence(buffer1, (uint8_t*)mapShading_EC1E0, 0x28A1E0, index - skip2, 0x70000, 0x10000, &origbyte20, &remakebyte20, 0x20000, (exitindex != 1000000));
					comp20 = compare_with_sequence(buffer1, (uint8_t*)mapAngle_FC1E0, 0x28A1E0, index - skip2, 0x70000, 0x10000, &origbyte20, &remakebyte20, 0x30000, (exitindex != 1000000));
					//comp20 = compare_with_sequence(buffer1, (uint8_t*)mapEntityIndex_10C1E0, 0x28A1E0, index - skip2, 0x70000, 0x20000, &origbyte20, &remakebyte20, 0x40000, (exitindex != 1000000));

#ifdef TEST_x64
					type_shadow_D41A0_BYTESTR_0 shadow_D41A0_BYTESTR_0;
					Convert_to_shadow_D41A0_BYTESTR_0(&D41A0_0, &shadow_D41A0_BYTESTR_0);
					comp20 = compare_with_sequence_D41A0(buffer2, (uint8_t*)&shadow_D41A0_BYTESTR_0, 0x356038, index - skip2, 224790, &origbyte20, &remakebyte20, 0, (exitindex != 1000000));
#else
					//26c400 1f690
					comp20 = compare_with_sequence_D41A0(buffer2, (uint8_t*)str_AE400_AE3F0, index, 232713, &origbyte20, &remakebyte20, 0, (exitindex != 1000000));
#endif

					// FIXME: skip the test of str_E2A74 for the moment as there are differences with the memimage that need to be clarified
					//type_shadow_str_E2A74 shadow_str_E2A74[0x69];
					//Convert_to_shadow_str_E2A74(str_E2A74, shadow_str_E2A74);
					//comp20 = compare_with_sequence_array_E2A74(buffer3, (uint8_t*)&shadow_str_E2A74, 0x2b3a74, index - skip2, 0xc4e, 0xc4e, &origbyte20, &remakebyte20, 0, (exitindex != 1000000));

					//screen
					//comp20 = compare_with_sequence(buffer4, pdwScreenBuffer_351628, 0x3aa0a4, index, 320 * 200, 320 * 200, &origbyte20, &remakebyte20);
				}
				//if(debugcounter_271478>5)
				if(compareGraphics)
					comp20 = compare_with_sequence(buffer4, pdwScreenBuffer_12EFF4, 0x002ECFF4, index, 320 * 200, 320 * 200, &origbyte20, &remakebyte20, 0, (exitindex != 1000000));
				if (stopstep > -1)
				{
					comp20 = index;
				}

				lastcompstr.index = index;
				lastcompstr.adress = adress;
			}
		}
	}
};
//for debuging