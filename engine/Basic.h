#pragma once
#include "ptr32.h" // ptr32<T>: 4-byte pointer slots in the packed DOS-layout structs below

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
	int32 a;
	int32 b;
	int32 c;
} Type_32bitAxis;

typedef struct {//size 44
	char text[14];
	uint16 x;//14
	uint16 y;//16
	uint8 stub[238];
} vesa_str;

typedef struct {
	uint16_t x;
	uint16_t y;
	int16_t z;
}axis_3d;

typedef struct {
	int16_t x;
	int16_t y;
	int16_t z;
}axis_3du;

/// Entity collision box (entity +78): centre is (x, y, z + zOffset), half-extents radiusX/Y/Z.
/// Two boxes collide when every axis distance is below the sum of the radii (sub_118C0).
typedef struct {
	int16_t zOffset_29873_78;//78  PORT: signed (castles use -8192); was uint16_t, so nothing ever hit a castle
	uint16_t radiusX_29875_80;//80
	uint16_t radiusY_29877_82;//82
	uint16_t radiusZ_29879_84;//84
}axis_4d;

typedef struct {//size 10
	uint32 var_0;
	uint32 var_4;
	uint16 var_8;
} Tmaps_str;

typedef struct {//size 34
	int16 var_0;
	int16 var_2;
	uint16 i16_4[15];
} Type_989C6;

typedef struct {//size 14
	uint16 var_0;
	uint16 var_2;
	uint16 var_4;
	uint16 var_6;
	uint16 var_8;
	int8 var_10;
	int8 var_11;
	int8 var_12;
	int8 var_13;
} Type_99BA0;

typedef struct {//size ?
	uint16 var_0;
	uint16 xx;
	uint16 yy;
	uint8 datax[];
}
Type_sub168_2_2;

typedef struct {//size 14
	ptr32<Type_sub168_2_2> var_u32_0;
	uint32 var_u32_4;
	uint16 var_u16_8[3];
}
Type_sub168_2;

typedef struct {//size 4
	ptr32<Type_sub168_2> var_u32_0;
}
Type_sub168_3;

typedef struct {//size 26
	uint32 var_0;
	int32 var_4x;
	ptr32<Type_sub168_2> var_8;
	ptr32<Type_sub168_3> var_12;
	ptr32<uint8> var_16;
	uint16 var_20;
	uint16 var_22;
	uint16 var_24;
}
Type_sub168;

typedef struct {
	Type_sub168 sub_0;
	Type_sub168_2 sub_26[529];
	Type_sub168_3 sub_7432[529];
	uint8 last[529];
	uint8 stub[];
}
Type_168;

typedef struct {//size 28
	uint32 var_0;
	ptr32<Type_sub168_2> var_4; //size 6?
	uint32 var_8;
	uint16 var_12;
	uint16 var_14;
	uint16 var_16;
	uint16 var_18;
	uint16 var_20;
	uint16 var_22;
	uint16 var_24;
	uint16 var_26;
}Type_sub_AE3E0;

typedef struct {//size 6
	uint16_t var_0;//count
	ptr32<Type_sub_AE3E0> var_2;
}Type_AE3E0;


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
	uint32 dw;
	ptr32<uint8> p;
}Type_IntPointer;

typedef union {
	uint32 dword;
	uint16 word[2];
	uint8 byte[4];
}Type_dword_0x0_0;

typedef struct {//size 946
	Type_dword_0x0_0 dw_0;//setting retybe to dword 6 (move)
	int16 word_0x4_4;// roll rate (signed)
	int16 word_0x6_6;// pitch rate (signed)
	uint8 stub_a0[4];
	int16 v_12;//speed Y
	int16 v_14;
	int16 v_16;//speed X
	uint8 stub_a1[4];
	uint16 v_22;
	uint16 v_24;
	uint16 v_26;
	uint16 v_28;
	uint8 stub_a2[16];
	uint16 v_46;
	int16 var_48;// PORT: was uint32, which shifted var_50/var_52 by 2 bytes; code reads *(int16*)(+48) and *(uint16*)(+50)
	uint16 var_50;
	uint16 var_52[4];//fix size 4?
	uint8 stub_aa[248];
	uint32 u32_308;
	uint8 stub_b[2];
	uint16 u16_314;
	uint16 u16_316;
	uint32 u32_318;
	uint32 u32_322;
	uint8 u8_326;
	int16 u16_327;// roll (signed) PORT: was uint16
	int16 u16_329;// pitch (signed) PORT: was uint16 - negative pitch (looking up) made 2*input-pitch run away and the camera spin
	uint16 u16_331;
	uint8 u8_333[8];
	uint16 u16_341;
	uint8 stub_b3[8];
	int32 u32_351;
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
	uint16 u16_408;//x speed
	uint16 u16_410;//y speed
	uint16 u16_412;//z speed
	uint8 stub_c4xx[1];
	uint8 var_415;
	uint8 stub_c4x[40];	
	Type_456 str_456[8];//fix size
	uint8 stub_cc1[2];
	uint16 u16_522;
	uint16 u16_524;
	uint16 u16_526;
	int16 u16_528;
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
	uint16 yaw_6;
	int16 pitch_8;// PORT: was uint16; DrawWorld takes it as a signed int (negative = looking up)
	uint16 roll_10;// unsigned on purpose: the renderer takes roll as a 0..65535 angle (quadrant = roll>>8&7); pitch_8 must be signed
	uint16 fov_12;
} Type_13895;

typedef struct {//68
	char messageText_13351_28[64];//fix size
	uint16 periods_13415;//92
	uint16 drawType_13417;//94
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
	uint16 playIndex_13333;//10
	uint8 stub0b[2];
	uint16 var_u16_13337_14;//14
	uint16 var_u16_13339_16;//16	
	uint32 var_u32_13341_18;//24 //diameter
	uint8 stub1[2];
	uint32 var_u32_13347_24;//24
	Type_str_28 messages_13351_28[8];//28

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
typedef struct {// 8 bytes; str_9177[8] spans 64 (offset 9177..9241)
	axis_3d v_9177;
	uint16 v_9181;
}
Type_AE400_9177;

typedef union {
	uint8_t byte[4];
	uint32_t dword;
} Type_29811;


typedef struct {// len 32 - creature movement profile (creatureTypes_98F38[])
	uint16 v_0;//?
	uint16 v_2;//?
	uint16 v_4;//?
	uint16 v_6;//?
	uint16 v_8;//?
	int16 maxHeight_10; // max height above ground (KeepHeightAboveGround_42000_42340)
	int16 minHeight_12; // min height above ground
	int16 vertSpeed_14; // vertical speed towards the allowed band (negative = sinks)
	uint16 maxSlope_16; // steepest terrain slope it can move onto (MoveCreature_196E0)
	uint16 v_18;
	uint32 v_20;
	uint16 v_22;
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
	ptr32<_Type_AE400_29795> next;
	uint32 rand_29799_4;//4
	uint32 maxLife_29803_8;//8
	int32 actLife_29807_12;//12 //act live?
	Type_29811 var_29811_16;//16
	//uint8 var_u8_29812;//17
	uint16 var_u16_29815_20;//20
	uint16 var_u16_29817_22;//22
	uint16 id_29819_24;//24
	int16 var_u16_29821_26;//26 z plus
	uint16 var_u16_29823_28;//28
	uint16 var_u16_29825_30;//30
	uint16 var_u16_29827_32;//32
	uint16 var_u16_29829_34;//34
	uint8_t stub2a[2];
	uint16 var_u16_29833_38;//38
	uint16 var_u16_29835_40;//40
	uint16 var_u16_29837_42;//42
	uint16 var_u16_29839_44;//44
	int16 var_u16_29841_46;//46
	uint16 var_48;//29843
	uint16 var_50;//50
	uint16 var_u16_29847_52;//52
	uint16 var_u16_29849_54;//54
	uint8_t stub2eb[2];//56
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

	axis_4d collisionBox_29873_78;

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

	int16 actSpeed_29921_126;//126

	int16 actSpeed_29923_128;//128 // pre actspeed
	uint16 acceleration_29925_130;//130 //acceleration
	// PORT: these three are signed 32-bit in the original (signed compares/idiv); +132 was a uint16 (+ stub)
	// and +136/+140 uint32: negative mana changes (spell costs) were added as ~65000 and a mana value
	// below 0 wrapped and was clamped to the capacity - mana never ran out (tools/diff_tick.py).
	int32 manaDelta_29927_132;//132 per-tick mana change: regeneration, or minus a spell's cost
	int32 manaMax_29931_136;//136 mana capacity
	int32 mana_29935_140;//140 current mana
	uint16 var_u16_29939_144;//144
	uint16 var_u16_29941_146;//146 //index entity
	uint8_t stub4[2];
	axis_3du var_u32_150;
	//uint16 var_u16_154;
	ptr32<Type_156> var_u32_29951_156;//156
	ptr32<Type_160> var_u32_29955_160;//160
}
Type_AE400_29795;
typedef struct {// len 106
	uint32 var_u32_232607;
	uint8_t var_u8_232611[96];
	uint16_t var_u16_232707;
	uint32_t var_u32_232709;
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

typedef struct {
	uint8 stub0[1];
	uint8 var_u8_1;
	uint8 var_u8_2;
	uint8 stub0b[1];
}
Type_str_AE400_AE3F0_0;

typedef struct {
	uint8 reflections_8597;//0 is on
	uint8 shadowsOn_8598;
	uint8 skyOn_8599;
	uint8 var_u8_8600;

	uint8 mapOn_8601;
	uint8 iconsOn_8602;
	uint8 mod3D_8603;
	uint8 blur_8604;

	uint8 softenOn_8605;
	uint8 var_u8_8606;
	uint8 stub9[1];
	uint8 var_u8_8608;

	uint8 stub10[12];

	uint8 var_u8_8621;
	uint8 var_u8_8622;
	uint8 var_u8_8623;//sky is on
	uint8 var_u8_8624;

	uint8 var_u8_8625;
	uint8 var_u8_8626;
	uint8 var_u8_8627;
	uint8 var_u8_8628;

	uint8 var_u8_8629;
	uint8 var_u8_8630;
	uint8 var_u8_8631;
	uint8 var_u8_8632;
}
Type_settings;

typedef struct {//size 232713
	Type_str_AE400_AE3F0_0 var_0;
	uint32 rand_4;
	uint16 var_u16_8;
	uint16 var_u16_10;
	Type_AE400_20 str_12;// len 20
	uint32 var_u32_32;
	uint16 var_u16_36;
	uint16 var_u16_38;
	int32 var_u32_40;
	uint8 var_u8_44[532];
	un16_32 var_u32_576;
	uint8 var_u8_580;
	uint32 var_u32_581;
	uint8 stub4b[8];
	ptr32<Type_AE400_29795> var_u32_593[1000];
	int32 var_u32_4593;
	ptr32<Type_AE400_29795> var_u32_4597[1000];

	Type_settings set;

	uint8 stub11[544];
	Type_AE400_9177 str_9177[8];// len 48
	uint8 stub12[2033];
	TypeStrAE400_13323 str_11274;
	TypeStrAE400_13323 str_13323[8];//fix
	char var_29715[8][10];// 5 - arrows
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
/*
typedef struct {// len 80
	uint8_t stub[80];
}
Type_AE408_36382;*/

typedef struct {//size 36478
	uint8 var_u8_0;
	int8 var_u8_1;
	uint8 var_u8_2;
	uint8 var_u8_3;
	Type_29811 var_u32_4;
	uint8 var_u8_8;
	extptr32<FILE> var_u32_9;
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
	uint8 var_u8_149;
	uint8 gameSpeed_150;
	uint8 var_u8_151;
	uint8 var_u8_152;
	uint32 var_u32_153;
	uint32 var_u32_157;
	uint8 var_u8_161;
	int32 var_u32_162;
	uint16 var_u16_166;
	ptr32<Type_168> var_u32_168;
	uint32 var_u32_172;
	uint8 stubd[8];
	Type_AE408_184 str_184;// len 14
	uint8 stube[8];
	uint32 var_u32_206;
	uint32 var_u32_210;
	uint8 stubf[36164];
	uint32 var_u32_36378;
	ptr32<Type_AE400_29795> str_36382x[20];// len 80  creature (class 5) lists, one per model, rebuilt each tick by sub_41780_41AC0
	ptr32<Type_AE400_29795> var_u32_36462[4];// 0 wizards/castles (class 3), 1 effects 10/39-40, 2 effects 10/45, 3 missiles (class 9). DOS: struct ends here (size 36478)
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
	ptr32<uint8_t> p;
	uint32_t align;
} p64align;

typedef struct {// NetBIOS NCB, port-specific layout (DOS original was 66 bytes)
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
	uint8_t** colorPalette_var28;//1C // Palette (port-side table, accessed by name: real pointers)
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

typedef struct {
	int8_t x;
	int8_t y;
} bsaxis_2d;

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
	ptr32<uint8_t> Tab_0;//0
	TypeTabDimm dim;//4	
} TypeTab;

typedef struct {
	TypeTab* bTab; // PORT: real pointers - loader targets in the path tables (Pathstruct), accessed by name
	TypeTab* eTab;
	uint8_t* Dat;
} TypeStrFont;

typedef struct _Type_dword_96884 {
	//Type_sub_96884* var32_0;
	ptr32<TypeTab> next;
	uint8_t var8_4;
	uint8_t var8_5;
	uint8_t var8_6;
	uint8_t var8_7;
} Type_dword_96884;

typedef struct {//size 44
	int32 x_0;
	int32 alt_4;
	int32 alt2_8;
	int32 y_12;
	int32 pnt1_16;
	int32 pnt2_20;
	int32 pnt3_24;
	int32 pnt4_28;
	int32 pnt5_32;
	int16 haveBillboard_36;
	uint16 triangleFeatures_38;
	int8 var_40;
	uint8 textIndex_41;
	int8 textUV_42;
	bool textAtyp_43;
} Type_BegBscreen;

typedef struct {//size 4*5
	int32 x_0;
	int32 y_1;
	int32 u_2;
	int32 v_3;
	int32 z_4;
} Type_RenderPoint;

typedef struct {//size 4*5
	un16_32 x_0;
	un16_32 y_1;
	un16_32 u_2;
	un16_32 v_3;
	un16_32 z_4;
} Type_RenderPoint2;

typedef struct {//size 6
	ptr32<uint8> pointer_0;
	uint16 pos_4;
}
Type_AD008_ACFF8;

typedef struct {//size 6*4 - ring iterator (BeginRingIter_11410 / NextRingOffset_114B0)
	int32 firstRing_0;
	int32 lastRing_4;
	int32 ring_8; // current ring, -1 = iterator free
	int32 indexInRing_12;
	ptr32<Type_AD008_ACFF8> ringDesc_16; // current ring: offsets + count
	ptr32<uint8> offsets_20; // next (dx, dy) signed byte pair, 4 bytes per entry
}
Type_AC5F0;

#pragma pack (16)

extern uint8_t* begWscreen_2ECFF4_2ECFE4;

extern int16_t mapEntityIndex_10C1E0_10C1D0[0x10000];
extern uint8_t mapTerrainType_CC1E0_CC1D0[0x10000];
extern uint8_t mapHeightmap_DC1E0_DC1D0[0x10000];
extern uint8_t mapShading_EC1E0_EC1D0[0x10000];
extern uint8_t mapAngle_FC1E0_FC1D0[0x10000];

extern Type_str_AE400_AE3F0* str_AE400_AE3F0;
extern Type_str_AE408_AE3F8* str_AE408_AE3F8;

int dword_AE400_AE3F0();

//void dword_AE400_AE3F0(int input);

int dword_AE408_AE3F8();

void dword_AE408_AE3F8(int input);

int my_sign32(int32_t var);
