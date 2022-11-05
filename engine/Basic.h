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

typedef struct {
	uint16_t x;
	uint16_t y;
	int16_t z;
}axis_3d;

typedef struct {
	uint16_t var_u16_29873_78;//78
	uint16_t var_u16_29875_80;//80 //speed
	uint16_t var_u16_29877_82;//82
	uint16_t var_u16_29879_84;//84
}axis_4d;

typedef struct {//size 8
	uint8 stub_a[4];
	int16 var_u16_4;
	uint16 var_u16_6;
}Type_456;

typedef struct {//size 48
	uint16 var_u16[24];//var_676
	//uint16 var_708[8];
}Type_48x;

typedef union {
	uint32 dword;
	uint16 word[2];
	uint8 byte[4];
}Type_dword_0x0_0;

typedef struct {//size 946
	Type_dword_0x0_0 dw_0;
	uint16 word_0x4_4;
	uint16 word_0x6_6;
	uint8 stub_a0[4];
	uint16 v_12;
	uint16 v_14;
	uint16 v_16;
	uint8 stub_a1[4];
	uint16 v_22;
	uint16 v_24;
	uint16 v_26;
	uint16 v_28;
	uint8 stub_a2[16];
	uint16 v_46;
	uint32 var_48;
	uint16 var_50;
	uint16 var_52[4];//fix size 4?
	uint8 stub_aa[246];
	uint32 u32_308;
	uint8 stub_b[2];
	uint16 u16_314;
	uint16 u16_316;
	uint32 u32_318;
	uint32 u32_322;
	uint8 u8_326;
	uint16 u16_327;
	uint16 u16_329;
	uint16 u16_331;
	uint8 u8_333[8];
	uint16 u16_341;
	uint8 stub_b3[8];
	uint32 u32_351;
	uint8 stub_c4[4];
	uint32 u32_359;
	uint8 stub_c[16];
	uint32 u32_379;
	uint32 u32_383;
	uint8 stub_c2[4];
	uint8 u8_391;
	uint8 u8_392;
	uint8 u8_393;
	uint8 stub_c2y[2];
	uint32 u32_396;
	uint32 u32_400;
	uint8 stub_c3[4];
	uint16 u16_408;
	uint16 u16_410;
	uint16 u16_412;
	uint8 stub_c4xx[1];
	uint8 var_415;
	uint8 stub_c4x[40];	
	Type_456 str_456[8];//fix size
	uint8 stub_cc1[2];
	uint16 u16_522;
	uint16 u16_524;
	uint16 u16_526;
	uint16 u16_528;
	uint8 stub_cc[2];
	int32 var_14958_1635_532[24];
	uint8 stub_ccxx[48];
	Type_48x var_676;
	uint8 stub_ccx[32];
	uint16 var_756;
	uint8 stub_c2b[14];
	int8 var_15198_1875_772[24];//fix size
	uint8 var_15222_1899_796[48];//fix size
	uint8 var_844[48];
	uint8 var_15318_1995_892[24];
	uint8 var_916[24];
	uint16 var_u16_2043_940;
	uint8 stub2[2];
	uint16 var_u16_2047_944;
	/*int32 var_532[8];//fix size
	uint8 stub_c2x[144];
	uint16 var_708;
	uint8 stub_c3b[46];
	uint16 var_756;
	uint8 stub_c2b[14];
	uint8 var_772[24];
	uint8 stub_c3bx[48];
	uint8 var_844[8];//fix size
	uint8 stub_d[64];//?
	uint8 var_916[24];
	uint16 var_940;//?
	uint8 stub_e[2];//?
	uint16 var_944;//?*/
}Type_160;

typedef struct {//14
	axis_3d axis;
	uint16 var_6;
	uint16 var_8;
	uint16 var_10;
	uint16 var_12;
} Type_13895;

typedef struct {//68
	char var_13351_28[64];//fix size
	uint16 var_u16_13415;//92
	uint16 var_u16_13417;//94
}
Type_str_28;

typedef struct {//2049
	uint16 var_u16_13323;//0
	uint16 var_u16_13325;//2
	uint8 var_u8_13327;//4
	uint8 v_13328_5;//5
	uint8 var_u8_13329_6;//6
	uint16 var_u16_13330_7;//7
	uint8 var_u8_13332_9;//9
	uint16 var_u16_13333;//10
	uint8 stub0b[2];
	uint16 var_u16_13337_14;//14
	uint16 var_u16_13339_16;//16	
	uint32 var_u32_13341_18;//24
	uint8 stub1[2];
	uint32 var_u32_13347_24;//24
	Type_str_28 str_13351_28[8];//28

	Type_13895 str_13895_572[33];
	//Type_13895 str_13909_586[32];//fix size
	char str_14357[12];
	char str_14369_1046[48];//fix size
	uint8 stub2bx[4];
	uint8 var_14421_1098;
	uint8 var_14422_1099;
	uint8 stub2bxx[3];
	Type_160 str_1103;//fix it
	/*uint8 stub2b[528];
	int32 var_14958_1635[60];//fix size
	uint8 var_15198_1875[24];//fix size
	uint8 var_15222_1899[96];//fix size
	uint8 var_15318_1995[24];
	uint8 stub3[24];
	uint16 var_u16_2043;
	uint8 stub2[2];
	uint16 var_u16_2047;*/
	//2049 * str_AE400_AE3F0->var_u16_8 + dword_AE400_AE3F0() + 13347
}
TypeStrAE400_13323;

//13269), 0x18u

typedef union {
	uint16 a16[2];
	uint32 a32;
} un16_32;

typedef struct {// len 20
	uint8_t stub[20];
}
Type_AE400_20;
typedef struct {// len 48
	axis_3d v_9177;
	uint16 v_9181;
}
Type_AE400_9177;

typedef union {
	uint8_t byte[4];
	uint32_t dword;
} Type_29811;


typedef struct {// len 32
	uint16 v_0;//?
	uint16 v_2;//?
	uint16 v_4;//?
	uint16 v_6;//?
	uint16 v_8;//?
	uint16 v_10;
	uint16 v_12;
	uint16 v_14;
	uint16 v_16;
	uint16 v_18;
	uint32 v_20;
	uint16 v_26;
	uint16 v_28;
	uint16 v_30;
}
Type_156;

typedef struct {// len 36
uint32_t var_u16_29885_90;//90
uint16_t var_u16_29889_94;//94

uint32_t var_u16_29891_96;//96
uint16_t var_u16_29895_100;//100
uint8_t stub3axx[6];
uint16_t var_u16_29903_108;//108
uint8_t stub3axx1[2];
uint16_t var_u16_29907_112;//112
uint8_t stub3axx2[4];
uint16_t var_u16_29913_118;//118
uint8_t stub3axx3[6];
}
Type_29885_90;

typedef struct _Type_AE400_29795 {// len 164
	_Type_AE400_29795* next;
	uint32 rand_29799_4;//4
	uint32 maxLife_29803_8;//8
	int32 var_u32_29807_12;//12 //act live?
	Type_29811 var_29811_16;//16
	//uint8 var_u8_29812;//17
	uint16 var_u16_29815_20;//20
	uint16 var_u16_29817_22;//22
	uint16 id_29819_24;//24
	uint16 var_u16_29821_26;//26
	uint16 var_u16_29823_28;//28
	uint16 var_u16_29825_30;//30
	uint16 var_u16_29827_32;//32
	uint8_t stub2a[4];
	uint16 var_u16_29833_38;//38
	uint16 var_u16_29835_40;//40
	uint16 var_u16_29837_42;//42
	uint16 var_u16_29839_44;//44
	uint16 var_u16_29841_46;//46
	uint16 var_48;//29843
	uint16 var_50;
	uint8_t stub2e[6];
	int8_t var_u8_29853_58;//58
	uint8_t stub2ax[2];
	uint8_t var_u8_29856_61;//61
	uint8_t var_u8_29857_62;//62
	uint8_t var_u8_29858_63;//63
	uint8_t var_u8_29859_64;//64
	uint8_t var_u8_29860_65;//65
	int8_t sClass_29861_66;//66
	int8_t sModel_29862_67;//67
	int8_t var_u8_29863_68;//68
	uint8_t stub2b[1];
	uint8_t var_u8_29865_70;//70
	uint8_t var_u8_29866_71;//71
	axis_3d var_u32_29867_72;//72
	//uint16_t var_u16_29871_76;//76

	axis_4d var_u16_29873_78;

	uint16_t var_u16_29881_86;//86
	uint8_t var_u8_29883_88;//88
	uint8_t var_u8_29884_89;//89

	Type_29885_90 str_29885_90;//90
	//struct
	/*uint32_t var_u16_29885_90;//90
	uint16_t var_u16_29889_94;//94

	uint32_t var_u16_29891_96;//96
	uint16_t var_u16_29895_100;//100
	uint8_t stub3axx[24];*/
	//end of struct

	uint16 actSpeed_29921_126;//126

	uint16 actSpeed_29923_128;//128
	uint8_t stub3ay[2];
	uint16 var_u16_29927_132;//132
	uint8_t stub3[2];
	uint32 var_u32_29931;//136
	uint32 var_u32_29935_140;//140
	uint16 var_u16_29939_144;//144
	uint16 var_u16_29941_146;//146
	uint8_t stub4[2];
	axis_3d var_u32_150;
	//uint16 var_u16_154;
	Type_156* var_u32_29951_156;//156
	Type_160* var_u32_29955_160;//160
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

typedef struct {// len 216
	uint8 stub[4];
	uint16 var_230871;
	uint8 stub2[2];
	uint16 var_230875;
	uint8 stub3[2];
	uint16 var_230879;
	uint8 stub4[2];
	
	uint8 var_230883[100];
	uint8 var_230983[100];
}
Type_37072;

typedef struct {//len 38812
	uint8 stub_0[1072];
	Type_1090 str_1072[2000];	
	//Type_1090 var_u16_1090[1999];//size 9 count 1000
	//37072 end
	//uint8 stub_37072[12];//230867
	Type_37072 str_230867_37072[8];//fix size
	//uint8 stub_193795[37710];
	uint16 var_u16_232595_38800;
	uint16 var_u16_232597_38802;
	uint8 stub_z[8];
}
Type_AE400_193795;

typedef struct {//size 232713
	uint8 stub0[1];
	uint8 var_u8_1;
	uint8 var_u8_2;
	uint8 stub0b[1];
	uint32 rand_4;
	uint16 var_u16_8;
	uint16 var_u16_10;
	Type_AE400_20 str_12;// len 20
	uint32 var_u32_32;
	uint16 var_u16_36;
	uint16 var_u16_38;
	int32 var_u32_40;
	uint8 stub4[532];
	un16_32 var_u32_576;
	uint8 var_u8_580;
	uint8 stub4b[12];
	Type_AE400_29795* var_u32_593[1000];
	int32 var_u32_4593;
	Type_AE400_29795* var_u32_4597[1000];
	uint8 var_u8_8597;
	uint8 var_u8_8598;
	uint8 var_u8_8599;
	int8 var_u8_8600;
	uint8 var_u8_8601;
	uint8 var_u8_8602;
	uint8 var_u8_8603;
	uint8 stub8[2];
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
	Type_AE400_9177 str_9177[8];// len 48
	uint8 stub12[2033];
	TypeStrAE400_13323 str_11274;
	TypeStrAE400_13323 str_13323[8];//fix
	char var_29715[8][10];
	Type_AE400_29795 str_29795[1000];// len 164000	
	Type_AE400_193795 str_193795;
	Type_AE400_232607 str_232607;// len 106
} Type_str_AE400_AE3F0;

typedef struct {// len 16
	uint8_t var_u8[16];
}
Type_AE408_93;
typedef struct {// len 14 //184
	uint8_t stuba[4];
	uint32 var_u32_188;
	uint32 var_u32_192;
	uint8_t stubb[2];
}
Type_AE408_184;
typedef struct {// len 80
	uint8_t stub[80];
}
Type_AE408_36382;

typedef struct {//size 14
	uint16 var_u16_0[2];
	uint32 var_u32_4;
	uint16 var_u16_8[3];
}
Type_sub168_2;

typedef struct {//size 4
	uint32 var_u16_0;
}
Type_sub168_3;

typedef struct {
	uint32 var_0;
	uint32 var_4;
	Type_sub168_2* var_8;
	Type_sub168_2* var_12;
	Type_sub168_3* var_16;
	uint16 var_20;
	uint16 var_22;
	uint16 var_24;
}
Type_sub168;

typedef struct {
	Type_sub168 sub_0;
	Type_sub168_2 sub_26[529];
	Type_sub168_3 sub_7432[529];
}
Type_168;

typedef struct {//size 36478
	uint8 var_u8_0;
	uint8 var_u8_1;
	uint8 var_u8_2;
	uint8 var_u8_3;
	Type_29811 var_u32_4;
	uint8 var_u8_8;
	FILE* var_u32_9;
	uint16 var_u16_13;
	uint16 var_u16_15;
	uint16 var_u16_17;
	uint8 stuba[3];
	int8 var_u8_22;
	uint8 var_u8_23;
	uint8 var_u8_24;
	uint32 var_u32_25;
	char var_u8_29[32];
	char var_u8_61[32];
	Type_AE408_93 str_93;// len 16
	uint8 stubb[8];
	char textBuffer_117[32];//fix size
	uint8 stubc[1];
	uint8 var_u8_150;
	uint8 var_u8_151;
	uint8 var_u8_152;
	uint32 var_u32_153;
	uint32 var_u32_157;
	uint8 var_u8_161;
	uint32 var_u32_162;
	uint16 var_u16_166;
	Type_168* var_u32_168;
	uint32 var_u32_172;
	uint8 stubd[8];
	Type_AE408_184 str_184;// len 14
	uint8 stube[8];
	uint32 var_u32_206;
	uint32 var_u32_210;
	uint8 stubf[36164];
	uint32 var_u32_36378;
	Type_AE408_36382 str_36382;// len 80
	Type_AE400_29795* var_u32_36462[4];
	//uint32 var_u32_36466;
	//uint32 var_u32_36470;
	//uint32 var_u32_36474;
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

typedef struct {
	uint8_t x;
	uint8_t y;
} baxis_2d;

typedef union {
	baxis_2d _axis_2d;
	uint16_t word;
}
uaxis_2d;

typedef struct {
	uint8_t x;//4
	uint8_t y;//5
} TypeTabDimm;

typedef struct {
	uint8_t* Tab_0;//0
	TypeTabDimm dim;//4	
} TypeTab;

typedef struct {
	TypeTab* bTab;
	TypeTab* eTab;
	uint8_t* Dat;
} TypeStrFont;

typedef struct _Type_dword_96884 {
	//Type_sub_96884* var32_0;
	TypeTab* next;
	uint8_t var8_4;
	uint8_t var8_5;
	uint8_t var8_6;
	uint8_t var8_7;
} Type_dword_96884;
#pragma pack (16)

extern uint8_t* pdwScreenBuffer_12EFF4;

extern int16_t mapEntityIndex_10C1E0[0x10000];
extern uint8_t mapTerrainType_CC1E0[0x10000];
extern uint8_t mapHeightmap_DC1E0[0x10000];
extern uint8_t mapShading_EC1E0[0x10000];
extern uint8_t mapAngle_FC1E0[0x10000];

extern Type_str_AE400_AE3F0* str_AE400_AE3F0;
extern Type_str_AE408_AE3F8* str_AE408_AE3F8;

int dword_AE400_AE3F0();

//void dword_AE400_AE3F0(int input);

int dword_AE408_AE3F8();

void dword_AE408_AE3F8(int input);

int my_sign32(int32_t var);
