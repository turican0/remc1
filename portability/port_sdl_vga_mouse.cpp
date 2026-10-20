//#include "../engine/engine_support.h"
#include "port_sdl_vga_mouse.h"
#include "port_time.h"
#include "Settings.h"
#include "InputScript.h"
#include "port_timer.h"
#include "port_watchdog.h"
#include "Presenter.h"
#include <memory>
#include <vector>
#include <algorithm>

#include <cstdlib>

#include <cstdint>

#include "../engine/sub_main_mouse.h"
#include "../engine/CommandLineParser.h"



#ifdef USE_DOSBOX
extern DOS_Device* DOS_CON;
#endif //USE_DOSBOX

// The game draws into m_gamePalletisedSurface (8-bit indexed, current VGA mode size); the presenter
// (portability/Presenter.h) turns that plus the palette into pixels on screen.
static std::unique_ptr<IPresenter> g_presenter;
SDL_Surface* m_gamePalletisedSurface = NULL;
SDL_Color m_currentPalletColours[256];

uint8_t lastPressedKey_12EF70_12EF60; //3516e4
uint8_t pressedKeys_12EEF0_12EEE0[128]; // idb

uint16_t m_iOrigw = 640;
uint16_t m_iOrigh = 480;
int m_iColorsByte = 1;
int act_ColorsByte = 1;

bool m_bMaintainAspectRatio = true;

bool settingWindowGrabbed = true;
bool settingWASD = false;

const char* default_caption = "Magic Carpet 1 - Community Update";

bool inited = false;
Uint8 tempPalettebuffer[768];

int oldWidth;

bool subBlitLock = false;
static uint32_t presentedFrames = 0; // frames presented so far (input scripts and frame dumps are keyed on it)

// Initalize Color Masks.
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
Uint32 redMask = 0xff000000;
Uint32 greenMask = 0x00ff0000;
Uint32 blueMask = 0x0000ff00;
Uint32 alphaMask = 0x000000ff;
#else
Uint32 redMask = 0x000000ff;
Uint32 greenMask = 0x0000ff00;
Uint32 blueMask = 0x00ff0000;
Uint32 alphaMask = 0xff000000;
#endif

void VGA_Init(Uint32  /*flags*/, int width, int height, bool maintainAspectRatio)
{
	m_bMaintainAspectRatio = maintainAspectRatio;

	if (!inited)
	{
		// SDL video is always initialised (the event queue needs it; headless runs use the dummy driver).
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
			exit(1);
		}
		settingWindowGrabbed = g_settings.grabMouse;
		PresenterConfig pc;
		pc.windowWidth = g_settings.windowWidth;
		pc.windowHeight = g_settings.windowHeight;
		pc.fullscreen = g_settings.fullscreen;
		pc.grabMouse = g_settings.grabMouse;
		pc.vsync = g_settings.vsync;
		pc.logicalWidth = width;
		pc.logicalHeight = height;
		pc.title = default_caption;
		pc.dumpDir = g_settings.dumpFramesDir.c_str();
		pc.dumpEvery = g_settings.dumpEvery;
		g_presenter = g_settings.headless ? CreateNullPresenter() : CreateSdlPresenter();
		if (!g_presenter->Init(pc))
		{
			printf("Video initialisation failed\n");
			exit(1);
		}
		PortTimer_SetVsyncRefresh(g_presenter->VsyncRefreshHz());
		if (g_settings.frameStats)
			fprintf(stderr, "frame-stats: vsync refresh %d Hz\n", g_presenter->VsyncRefreshHz());

		m_gamePalletisedSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 8, SDL_PIXELFORMAT_INDEX8);
		if (!VGA_LoadFont())
		{
			printf("Failed to load font!\n");
			exit(-1);
		}

		SetBlackPalette();
		//Draw_debug_matrix1();
		Draw_black();
		Set_basic_Palette1();
		inited = true;
	}
}

Uint8* VGA_Get_Palette() {
	return tempPalettebuffer;
}

uint16_t lastResHeight=0;

void SetPalette(SDL_Color* colours) {
	memcpy(m_currentPalletColours, colours, sizeof(SDL_Color) * 256);
	if (m_gamePalletisedSurface)
	{
		SDL_SetPaletteColors(m_gamePalletisedSurface->format->palette, m_currentPalletColours, 0, 256);
	}
	if (g_presenter)
	{
		uint8_t rgb[768];
		for (int i = 0; i < 256; i++) {
			rgb[i * 3] = colours[i].r;
			rgb[i * 3 + 1] = colours[i].g;
			rgb[i * 3 + 2] = colours[i].b;
		}
		g_presenter->SetPalette(rgb);
	}
}

void Set_basic_Palette0() {
	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		tempPalettebuffer[i * 3] = i / 4;
		tempPalettebuffer[i * 3 + 1] = i / 4;
		tempPalettebuffer[i * 3 + 2] = i / 4;
		colors[i].r = tempPalettebuffer[i * 3];
		colors[i].g = tempPalettebuffer[i * 3 + 1];
		colors[i].b = tempPalettebuffer[i * 3 + 2];
	}
	SetPalette(colors);
}

void SetBlackPalette() {
	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		{
			colors[i].r = 0;
			colors[i].g = 0;
			colors[i].b = 0;
		}
	}
	SetPalette(colors);
}
void Set_basic_Palette1() {
	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		if (i != 0)
		{
			colors[i].r = 0;
			colors[i].g = 0;
			colors[i].b = 0;
		}
		else
		{
			colors[i].r = 255;
			colors[i].g = 255;
			colors[i].b = 255;
		}
	}
	SetPalette(colors);
}

void Set_basic_Palette3() {
	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		tempPalettebuffer[i * 3] = i;
		tempPalettebuffer[i * 3 + 1] = ((int)(i / 16)) * 16;
		tempPalettebuffer[i * 3 + 2] = (i % 16) * 16;
		colors[i].r = tempPalettebuffer[i * 3];
		colors[i].g = tempPalettebuffer[i * 3 + 1];
		colors[i].b = tempPalettebuffer[i * 3 + 2];
	}
	SetPalette(colors);
}

void putpixel(SDL_Surface* surface, int x, int y, Uint32 pixel)
{
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to set */
	Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;

	switch (bpp) {
	case 1:
		*p = pixel;
		break;

	case 2:
		*(Uint16*)p = pixel;
		break;

	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			p[0] = (pixel >> 16) & 0xff;
			p[1] = (pixel >> 8) & 0xff;
			p[2] = pixel & 0xff;
		}
		else {
			p[0] = pixel & 0xff;
			p[1] = (pixel >> 8) & 0xff;
			p[2] = (pixel >> 16) & 0xff;
		}
		break;

	case 4:
		*(Uint32*)p = pixel;
		break;
	}
}

void Draw_debug_matrix0() {
	SDL_Rect dstrect;
	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		if (SDL_LockSurface(m_gamePalletisedSurface) < 0) {
			fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
			return;
		}
	}

	for (int i = 0; i < 16; i++)
		for (int j = 0; j < 16; j++)
		{
			dstrect.x = i * m_gamePalletisedSurface->w / 16;
			dstrect.y = j * m_gamePalletisedSurface->h / 16;
			dstrect.w = m_gamePalletisedSurface->w / 16;
			dstrect.h = m_gamePalletisedSurface->h / 16;
			SDL_FillRect(m_gamePalletisedSurface, &dstrect, i * 16 + j/*SDL_MapRGB(screen->format, i*16+j, 0, 0)*/);
		}

	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		SDL_UnlockSurface(m_gamePalletisedSurface);
	}
	SubBlit(m_iOrigw, m_iOrigh);
};

void Draw_black() {
	SDL_Rect dstrect;
	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		if (SDL_LockSurface(m_gamePalletisedSurface) < 0) {
			fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
			return;
		}
	}

	dstrect.x = 0;
	dstrect.y = 0;
	dstrect.w = m_gamePalletisedSurface->w;
	dstrect.h = m_gamePalletisedSurface->h;
	SDL_FillRect(m_gamePalletisedSurface, &dstrect, 1);

	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		SDL_UnlockSurface(m_gamePalletisedSurface);
	}
	SubBlit(m_iOrigw, m_iOrigh);
};

void Draw_debug_matrix1() {
	SDL_Rect dstrect;
	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		if (SDL_LockSurface(m_gamePalletisedSurface) < 0) {
			fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
			return;
		}
	}

	for (int i = 0; i < 16; i++)
		for (int j = 0; j < 16; j++)
		{
			dstrect.x = i * m_gamePalletisedSurface->w / 16;
			dstrect.y = j * m_gamePalletisedSurface->h / 16;
			dstrect.w = m_gamePalletisedSurface->w / 16;
			dstrect.h = m_gamePalletisedSurface->h / 16;
			SDL_FillRect(m_gamePalletisedSurface, &dstrect, 1);
		}

	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		SDL_UnlockSurface(m_gamePalletisedSurface);
	}
	SubBlit(m_iOrigw, m_iOrigh);
};

uint8_t fontbuffer[256 * 256];
SDL_Surface* surface_font = NULL;
bool VGA_LoadFont()
{
	//Loading success flag
	bool success = true;

	//Load splash image
	std::string fontPath = GetSubDirectoryPath("font/16x16-font.bmp");
	surface_font = SDL_LoadBMP(fontPath.c_str());
	if (surface_font == NULL)
	{
		printf("Unable to load image %s! SDL Error: %s\n", "16x16-font.bmp", SDL_GetError());
		success = false;
	}

	uint8_t* pixels = (uint8_t*)surface_font->pixels;
	for (int yy = 0; yy < 256; yy++)
		for (int xx = 0; xx < 256; xx++)
			fontbuffer[yy * 256 + xx] = pixels[(yy * 256 + xx) * 3];

	return success;
}

int lastpoz = 0;
int textwidth = 40;
int textheight = 30;

void Draw_letter(int letter_number, int pozx, int pozy) {
	SDL_Rect srcrect;
	SDL_Rect dstrect;
	srcrect.x = 16 * (letter_number % 16);
	srcrect.y = 16 * (int)(letter_number / 16);
	srcrect.w = 16;
	srcrect.h = 16;
	dstrect.x = 16 * pozx;
	dstrect.y = 16 * pozy;
	dstrect.w = 16;
	dstrect.h = 16;
	SDL_BlitSurface(surface_font, &srcrect, m_gamePalletisedSurface, &dstrect);
};

void Draw_letterToBuffer(int letter_number, int pozx, int pozy, uint8_t* buffer) {
	SDL_Rect srcrect;
	SDL_Rect dstrect;

	srcrect.x = 16 * (letter_number % 16);
	srcrect.y = 16 * (int)(letter_number / 16);
	srcrect.w = 16;
	srcrect.h = 16;
	dstrect.x = pozx;
	dstrect.y = pozy;
	dstrect.w = 16;
	dstrect.h = 16;

	for (int yy = 0; yy < dstrect.h; yy++)
		for (int xx = 0; xx < dstrect.w; xx++)
		{
			int srcx = xx + srcrect.x;
			int srcy = yy + srcrect.y;
			buffer[(dstrect.y + yy) * m_gamePalletisedSurface->w + (dstrect.x + xx)] = fontbuffer[srcx + srcy * 256];
		}
};

void VGA_GotoXY(int x, int y) {
	lastpoz = y * textwidth + x;
};
int VGA_WhereX() {
	return(lastpoz % textwidth);
};
int VGA_WhereY() {
	return((int)(lastpoz / textwidth));
};

POSITION VGA_WhereXY() {
	POSITION result;
	result.x = (lastpoz % textwidth);
	result.y = ((int)(lastpoz / textwidth));
	return result;
};

void VGA_Draw_string(char* wrstring) {
	if(!m_gamePalletisedSurface)return;
	//SDL_Rect srcrect = { 0,0,0,0 };
	//SDL_Rect dstrect = { 0,0,0,0 };
	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		if (SDL_LockSurface(m_gamePalletisedSurface) < 0) {
			fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
			return;
		}
	}

	for (uint32_t i = 0; i < strlen(wrstring); i++)
	{
		if (wrstring[i] == '\n')
		{
			lastpoz += textwidth - lastpoz % textwidth;
		}
		else
		{
			if (lastpoz <= textwidth * textheight)
				Draw_letter(wrstring[i], lastpoz % textwidth, (int)(lastpoz / textwidth));
			lastpoz++;
		}
	}

	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		SDL_UnlockSurface(m_gamePalletisedSurface);
	}
	SubBlit(m_iOrigw, m_iOrigh); // PORT: a fixed mydelay(10) followed here; pacing is PortTimer_LimitFrame/WaitRetrace
}

int drawCounter = 0;
void VGA_Draw_stringXYtoBuffer(char* wrstring, int x, int y, uint8_t* buffer) {
	//fix it !!! if (unitTests)return;

	if (oldWidth != m_gamePalletisedSurface->w)
		drawCounter = 0;
	if (drawCounter<20)
	{
		drawCounter++;
		return;
	}
	int loclastpoz = 0;
	for (uint32_t i = 0; i < strlen(wrstring); i++)
	{
		if (wrstring[i] == '\n')
		{
			loclastpoz += textwidth - loclastpoz % textwidth;
		}
		else
		{
			Draw_letterToBuffer(wrstring[i], x + (loclastpoz % textwidth) * 16, y + ((int)(loclastpoz / textwidth)) * 16, buffer);
			loclastpoz++;
		}
	}
}

void VGA_Init(int width, int height, bool maintainAspectRatio) {
	//fix it !!! if (unitTests)return;
	m_bMaintainAspectRatio = maintainAspectRatio;

#define SDL_HWPALETTE 0
	VGA_Init(SDL_HWPALETTE | SDL_INIT_AUDIO, width, height, maintainAspectRatio);
}

SDL_Rect dst;

void VGA_Resize(int width, int height) {
	m_iOrigw = width;
	m_iOrigh = height;
}

void VGA_SetColors(int colors) {
	m_iColorsByte = colors/8;
};

FILE* fptpal;
void SavePal(Uint8* Palettebuffer, char* filename)
{
	fptpal = fopen(filename, "wb");
	fwrite(Palettebuffer, 768, 1, fptpal);
	fclose(fptpal);
}

void VGA_Set_file_Palette(char* filename) {
	uint8_t Palettebuffer[768];
	fptpal = fopen(filename, "rb");
	fread(Palettebuffer, 768, 1, fptpal);
	fclose(fptpal);

	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		colors[i].r = 4 * Palettebuffer[i * 3];
		colors[i].g = 4 * Palettebuffer[i * 3 + 1];
		colors[i].b = 4 * Palettebuffer[i * 3 + 2];
	}
	SetPalette(colors);
}

void VGA_Set_Palette_oneColor(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
	// PORT: was pushing 255 uninitialised SDL_Colors along with the one changed entry.
	tempPalettebuffer[index * 3] = red;
	tempPalettebuffer[index * 3 + 1] = green;
	tempPalettebuffer[index * 3 + 2] = blue;
	uint8_t pal[768];
	memcpy(pal, tempPalettebuffer, sizeof(pal));
	VGA_Set_Palette(pal);
}

void VGA_Set_Palette(Uint8* Palettebuffer) {
	memcpy(tempPalettebuffer, Palettebuffer, 768);
	SDL_Color colors[256];
	/* Fill colors with color information */
	for (int i = 0; i < 256; i++) {
		colors[i].r = 4 * Palettebuffer[i * 3];
		colors[i].g = 4 * Palettebuffer[i * 3 + 1];
		colors[i].b = 4 * Palettebuffer[i * 3 + 2];
	}
	SetPalette(colors);
}

void VGA_Set_Palette2(Uint8* Palettebuffer) {
	memcpy(tempPalettebuffer, Palettebuffer, 768);
	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		colors[i].r = Palettebuffer[i * 3];
		colors[i].g = Palettebuffer[i * 3 + 1];
		colors[i].b = Palettebuffer[i * 3 + 2];
	}
	SetPalette(colors);
}

void VGA_Write_basic_Palette(Uint8* Palettebuffer) {
	memcpy(tempPalettebuffer, Palettebuffer, 768);
}

void VGA_test() {
	//int x = m_gamePalletisedSurface->w / 2;
	//int y = m_gamePalletisedSurface->h / 2;

	/* Lock the screen for direct access to the pixels */
	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		if (SDL_LockSurface(m_gamePalletisedSurface) < 0) {
			fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
			return;
		}
	}

	for (int i = 0; i < 600; i++)
		for (int j = 0; j < 400; j++)
			putpixel(m_gamePalletisedSurface, i, j, 127);

	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		SDL_UnlockSurface(m_gamePalletisedSurface);
	}
	/* Update just the part of the display that we've changed */

	SubBlit(m_iOrigw, m_iOrigh);
}

/*

Keyboard scan code/character code combinations returned in AH/AL by
INT 16h functions 00h/01h (standard) and functions 01h/11h (extended)

Key                        Normal           Shift           Ctrl             Alt
						 Std     Ext     Std     Ext     Std     Ext     Std     Ext
Esc                     01/1B   01/1B   01/1B   01/1B   01/1B   01/1B     -     01/00
1 !                     02/31   02/31   02/21   02/21     -       -     78/00   78/00
2 @                     03/32   03/32   03/40   03/40   03/00   03/00   79/00   79/00
3 #                     04/33   04/33   04/23   04/23     -       -     7A/00   7A/00
4 $                     05/34   05/34   05/24   05/24     -       -     7B/00   7B/00
5 %                     06/35   06/35   06/25   06/25     -       -     7C/00   7C/00
6 ^                     07/36   07/36   07/5E   07/5E   07/1E   07/1E   7D/00   7D/00
7 &                     08/37   08/37   08/26   08/26     -       -     7E/00   7E/00
8 *                     09/38   09/38   09/2A   09/2A     -       -     7F/00   7F/00
9 (                     0A/39   0A/39   0A/28   0A/28     -       -     80/00   80/00
0 )                     0B/30   0B/30   0B/29   0B/29     -       -     81/00   81/00
- _                     0C/2D   0C/2D   0C/5F   0C/5F   0C/1F   0C/1F   82/00   82/00
= +                     0D/3D   0D/3D   0D/2B   0D/2B     -       -     83/00   83/00
Backspace               0E/08   0E/08   0E/08   0E/08   0E/7F   0E/7F     -     0E/00
Tab                     0F/09   0F/09   0F/00   0F/00     -     94/00     -     A5/00
q Q                     10/71   10/71   10/51   10/51   10/11   10/11   10/00   10/00
w W                     11/77   11/77   11/57   11/57   11/17   11/17   11/00   11/00
e E                     12/65   12/65   12/45   12/45   12/05   12/05   12/00   12/00
r R                     13/72   13/72   13/52   13/52   13/12   13/12   13/00   13/00
t T                     14/74   14/74   14/54   14/54   14/14   14/14   14/00   14/00
y Y                     15/79   15/79   15/59   15/59   15/19   15/19   15/00   15/00
u U                     16/75   16/75   16/55   16/55   16/15   16/15   16/00   16/00
i I                     17/69   17/69   17/49   17/49   17/09   17/09   17/00   17/00
o O                     18/6F   18/6F   18/4F   18/4F   18/0F   18/0F   18/00   18/00
p P                     19/70   19/70   19/50   19/50   19/10   19/10   19/00   19/00
[ {                     1A/5B   1A/5B   1A/7B   1A/7B   1A/1B   1A/1B     -     1A/00
] }                     1B/5D   1B/5D   1B/7D   1B/7D   1B/1D   1B/1D     -     1B/00
Enter                   lC/0D   1C/0D   1C/0D   1C/0D   1C/0A   1C/0A     -     1C/00
Ctrl                      *       *       *       *       *       *       *       *
a A                     1E/61   1E/61   1E/41   1E/41   1E/01   1E/01   1E/00   1E/00
s S                     1F/73   1F/73   1F/53   1F/53   1F/13   1F/13   1F/00   1F/00
d D                     20/64   20/64   20/44   20/44   20/04   20/04   20/00   20/00
f F                     21/66   21/66   21/46   21/46   21/06   21/06   21/00   21/00
g G                     22/67   22/67   22/47   22/47   22/07   22/07   22/00   22/00
h H                     23/68   23/68   23/48   23/48   23/08   23/08   23/00   23/00
j J                     24/6A   24/6A   24/4A   24/4A   24/0A   24/0A   24/00   24/00
k K                     25/6B   25/6B   25/4B   25/4B   25/0B   25/0B   25/00   25/00
l L                     26/6C   26/6C   26/4C   26/4C   26/0C   26/0C   26/00   26/00
; :                     27/3B   27/3B   27/3A   27/3A     -       -       -     27/00
' "                     28/27   28/27   28/22   28/22     -       -       -     28/00
` ~                     29/60   29/60   29/7E   29/7E     -       -       -     29/00
Left Shift                *       *       *       *       *       *       *       *
\ |                     2B/5C   2B/5C   2B/7C   2B/7C   2B/1C   2B/1C     -     2B/00
z Z                     2C/7A   2C/7A   2C/5A   2C/5A   2C/1A   2C/1A   2C/00   2C/00
x X                     2D/78   2D/78   2D/58   2D/58   2D/18   2D/18   2D/00   2D/00
c C                     2E/63   2E/63   2E/43   2E/43   2E/03   2E/03   2E/00   2E/00
v V                     2F/76   2F/76   2F/56   2F/56   2F/16   2F/16   2F/00   2F/00
b B                     30/62   30/62   30/42   30/42   30/02   30/02   30/00   30/00
n N                     31/6E   31/6E   31/4E   31/4E   31/0E   31/0E   31/00   31/00
m M                     32/6D   32/6D   32/4D   32/4D   32/0D   32/0D   32/00   32/00
, <                     33/2C   33/2C   33/3C   33/3C     -       -       -     33/00
. >                     34/2E   34/2E   34/3E   34/3E     -       -       -     34/00
/ ?                     35/2F   35/2F   35/3F   35/3F     -       -       -     35/00
Right Shift               *       *       *       *       *       *       *       *
*                       37/2A   37/2A   37/2A   37/2A     -     96/00     -     37/00
Alt                       *       *       *       *       *       *       *       *
Space                   39/20   39/20   39/20   39/20   39/20   39/20   39/20   39/20
Caps Lock                 *       *       *       *       *       *       *       *
F1                      3B/00   3B/00   54/00   54/00   5E/00   5E/00   68/00   68/00
F2                      3C/00   3C/00   55/00   55/00   5F/00   5F/00   69/00   69/00
F3                      3D/00   3D/00   56/00   56/00   60/00   60/00   6A/00   6A/00
F4                      3E/00   3E/00   57/00   57/00   61/00   61/00   6B/00   6B/00
F5                      3F/00   3F/00   58/00   58/00   62/00   62/00   6C/00   6C/00
F6                      40/00   40/00   59/00   59/00   63/00   63/00   6D/00   6D/00
F7                      41/00   41/00   5A/00   5A/00   64/00   64/00   6E/00   6E/00
F8                      42/00   42/00   5B/00   5B/00   65/00   65/00   6F/00   6F/00
F9                      43/00   43/00   5C/00   5C/00   66/00   66/00   70/00   70/00
F10                     44/00   44/00   5D/00   5D/00   67/00   67/00   71/00   71/00
Num Lock                  *       *       *       *       *       *       *       *
Scroll Lock               *       *       *       *       *       *       *       *
Home 7                  47/00   47/00   47/37   47/37   77/00   77/00   00/#    00/#
Up Arrow 8              48/00   48/00   48/38   48/38     -     8D/00   00/#    00/#
PgUp 9                  49/00   49/00   49/39   49/39   84/00   84/00   00/#    00/#
-                       4A/2D   4A/2D   4A/2D   4A/2D     -     8E/00     -     4A/00
Left Arrow 4            4B/00   4B/00   4B/34   4B/34   73/00   73/00   00/#    00/#
Center Key 5              -     4C/00   4C/35   4C/35     -     8F/00   00/#    00/#
Right Arrow 6           4D/00   4D/00   4D/36   4D/36   74/00   74/00   00/#    00/#
+                       4E/2B   4E/2B   4E/2B   4E/2B     -     90/00     -     4E/00
End 1                   4F/00   4F/00   4F/31   4F/31   75/00   75/00   00/#    00/#
Down Arrow 2            50/00   50/00   50/32   50/32     -     91/00   00/#    00/#
PgDn 3                  51/00   51/00   51/33   51/33   76/00   76/00   00/#    00/#
Ins 0                   52/00   52/00   52/30   52/30     -     92/00   00/#    00/#
Del .                   53/00   53/00   53/2E   53/2E     -     93/00     -       -
SysReq (84-key only)      *       *       *       *       *       *       *       *
PrtSc/SysReq (101-key)    *       *       *       *     72/00   72/00     *       *
Pause/Break (101-key)     *       *       *       *     00/00   00/00     *       *
F11 (101-key)             -     85/00     -     87/00     -     89/00     -     8B/00
F12 (101-key)             -     86/00     -     88/00     -     8A/00     -     8C/00
Home (101-key)          47/00   47/E0   47/00   47/E0   77/00   77/E0     -     97/00
Up Arrow (101-key)      48/00   48/E0   48/00   48/E0     -     8D/E0     -     98/00
Page Up (101-key)       49/00   49/E0   49/00   49/E0   84/00   84/E0     -     99/00
Left Arrow (101-key)    4B/00   4B/E0   4B/00   4B/E0   73/00   73/E0     -     9B/00
Right Arrow (101-key)   4D/00   4D/E0   4D/00   4D/E0   74/00   74/E0     -     9D/00
End (101-key)           4F/00   4F/E0   4F/00   4F/E0   75/00   75/E0     -     9F/00
Down Arrow (101-key)    50/00   50/E0   50/00   50/E0     -     91/E0     -     A0/00
Page Down (101-key)     51/00   51/E0   51/00   51/E0   76/00   76/E0     -     A1/00
Insert (101-key)        52/00   52/E0   52/00   52/E0     -     92/E0     -     A2/00
Delete (101-key)        53/00   53/E0   53/00   53/E0     -     93/E0     -     A3/00
/ (101-key)             35/2F   E0/2F   35/2F   E0/2F     -     95/00     -     A4/00
Enter (101-key)         1C/0D   E0/0D   1C/0D   E0/0D   1C/0A   E0/0A     -     A6/00
Right Alt (101-key)       *       *       *       *       *       *       *       *
Right Ctrl (101-key)      *       *       *       *       *       *       *       *
\ | (102-key)           56/5C   56/5C   56/7C   56/7C     -       -       -       -

- Keystroke ignored
* Processed by the BIOS
# Alt-keypad number returns the specified character code and scan code 0
*/

void ToggleFullscreen() {
	if (g_presenter)
		g_presenter->ToggleFullscreen();
}

bool handleSpecialKeys(const SDL_Event &event) {
	bool specialKey = false;
	if ((event.key.keysym.sym == SDLK_RETURN) && (event.key.keysym.mod & KMOD_ALT)) {
		ToggleFullscreen();
		specialKey = true;
	}
	else if ((event.key.keysym.sym == SDLK_F10) && (event.key.keysym.mod & KMOD_CTRL)) {
		settingWindowGrabbed = !settingWindowGrabbed;
		if (g_presenter)
			g_presenter->SetMouseGrab(settingWindowGrabbed);
		std::cout << "Set window grab to " << (settingWindowGrabbed ? "true" : "false") << std::endl;
		specialKey = true;
	}
	else if ((event.key.keysym.sym == SDLK_F11) && (event.key.keysym.mod & KMOD_CTRL)) {
		settingWASD = !settingWASD;
		std::cout << "Set WASD to " << (settingWindowGrabbed ? "true" : "false") << std::endl;
		specialKey = true;
	}

	return specialKey;
}

int mousex, mousey;

// SDL reports mouse positions in renderer logical units (640x480, see VGA_Init). The game's mouse
// space is 640 wide and 2x the line count in 200/400-line modes (DOS INT 33h in mode 13h), i.e.
// 640x400 for 320x200 and 640x400 modes, 640x480 for 640x480.
static void LogicalToGameMouse(int lx, int ly, int& gx, int& gy) {
	int gameMouseH = (m_iOrigw <= 320) ? m_iOrigh * 2 : m_iOrigh;
	gx = lx < 0 ? 0 : (lx > 639 ? 639 : lx);
	gy = ly * gameMouseH / 480;
	if (gy < 0) gy = 0;
	if (gy > gameMouseH - 1) gy = gameMouseH - 1;
}

bool pressed = false;
uint16_t lastchar = 0;
int events()
{
	SDL_Event event;
	Uint8 buttonindex;
	Uint8 buttonstate;
	uint32_t buttonresult;
	while (SDL_PollEvent(&event))
	{
		if ((g_settings.headless || InputScript_Playing()) && event.type != SDL_QUIT)
			continue; // replays and headless runs are driven by the input script only (live mouse/keys would desync them)
		switch (event.type)
		{
		case SDL_KEYDOWN:
			if (event.key.repeat)
				break;
			event.key.keysym.scancode = Port_RemapKey(event.key.keysym.scancode);
			if (!handleSpecialKeys(event)) {
				InputScript_RecordKey(presentedFrames, true, event.key.keysym.scancode);
				VGA_InjectKey(true, event.key.keysym.scancode);
			}
			break;

		case SDL_KEYUP:
			event.key.keysym.scancode = Port_RemapKey(event.key.keysym.scancode);
			InputScript_RecordKey(presentedFrames, false, event.key.keysym.scancode);
			VGA_InjectKey(false, event.key.keysym.scancode);
			break;

		case SDL_MOUSEMOTION:
			LogicalToGameMouse(event.motion.x, event.motion.y, mousex, mousey);
			InputScript_RecordMouseMove(presentedFrames, mousex, mousey);
			MouseEvents(1, mousex, mousey);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:

			buttonresult = 0;

			buttonindex = event.button.button;
			buttonstate = event.button.state;
			switch (buttonindex) {
			case SDL_BUTTON_LEFT:
			{
				switch (buttonstate) {
				case SDL_PRESSED:
				{
					buttonresult |= 2;
					break;
				}
				case SDL_RELEASED:
				{
					buttonresult |= 4;
					break; }
				}
				break;
			}
			case SDL_BUTTON_MIDDLE:
			{
				switch (buttonstate) {
				case SDL_PRESSED:
				{
					buttonresult |= 0x20;
					break;
				}
				case SDL_RELEASED:
				{
					buttonresult |= 0x40;
					break; }
				}
				break;
			}
			case SDL_BUTTON_RIGHT:
			{
				switch (buttonstate) {
				case SDL_PRESSED:
				{
					buttonresult |= 0x8;
					break;
				}
				case SDL_RELEASED:
				{
					buttonresult |= 0x10;
					break; }
				}
				break;
			}
			}

			LogicalToGameMouse(event.button.x, event.button.y, mousex, mousey);
			InputScript_RecordMouseButtons(presentedFrames, buttonresult, mousex, mousey);
			MouseEvents(buttonresult, mousex, mousey);
			break;

		case SDL_QUIT: return 0;
		}
	}
	return 1;
}

// x/y are in game mouse space (see LogicalToGameMouse)
void VGA_Set_mouse(int16_t x, int16_t y) {
	if (!g_presenter || !g_presenter->Window())
		return;
	int gameMouseH = (m_iOrigw <= 320) ? m_iOrigh * 2 : m_iOrigh;
	int wx, wy;
	g_presenter->LogicalToWindow((float)x, (float)y * 480.0f / gameMouseH, wx, wy);
	SDL_WarpMouseInWindow(g_presenter->Window(), wx, wy);
};

/*
void VGA_Blit_old(Uint8* srcBuffer) {
	//fix it !!!! if (unitTests)return;

	oldWidth = m_gamePalletisedSurface->w;
	if (CommandLineParams.DoHideGraphics()) return;
	events();

	if (m_iOrigh != m_gamePalletisedSurface->h)
	{
		SDL_FreeSurface(m_gamePalletisedSurface);

		m_gamePalletisedSurface =
			SDL_CreateRGBSurface(
				SDL_SWSURFACE, m_iOrigw, m_iOrigh, 24,
				redMask, greenMask, blueMask, alphaMask);

		m_gamePalletisedSurface =
			SDL_ConvertSurfaceFormat(
				m_gamePalletisedSurface, SDL_PIXELFORMAT_INDEX8, 0);

		SDL_SetPaletteColors(m_gamePalletisedSurface->format->palette, m_currentPalletColours, 0, 256);

		lastResHeight = m_iOrigh;
	}

	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		if (SDL_LockSurface(m_gamePalletisedSurface) < 0) {
			fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
			return;
		}
	}

	if(srcBuffer)
		memcpy(m_gamePalletisedSurface->pixels, srcBuffer, m_gamePalletisedSurface->h * m_gamePalletisedSurface->w);
	
	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		SDL_UnlockSurface(m_gamePalletisedSurface);
	}
	SubBlit(m_iOrigw, m_iOrigh);
	//fix it !!!! SOUND_UPDATE();
}
*/

void VGA_Blit(Uint8* srcBuffer) {
	//fix it !!!! if (unitTests)return;

	oldWidth = m_gamePalletisedSurface->w;
	if (CommandLineParams.DoHideGraphics()) return;
	InputScript_OnFrame(presentedFrames);
	events();

	if ((m_iOrigh != m_gamePalletisedSurface->h)||(m_iColorsByte != act_ColorsByte))
	{
		act_ColorsByte = m_iColorsByte;
		SDL_FreeSurface(m_gamePalletisedSurface);

		m_gamePalletisedSurface =
			SDL_CreateRGBSurface(
				SDL_SWSURFACE, m_iOrigw, m_iOrigh, 24,
				redMask, greenMask, blueMask, alphaMask);

		if (m_iColorsByte == 1)
			m_gamePalletisedSurface =
			SDL_ConvertSurfaceFormat(
				m_gamePalletisedSurface, SDL_PIXELFORMAT_INDEX8, 0);
		else
		m_gamePalletisedSurface =
			SDL_ConvertSurfaceFormat(
				m_gamePalletisedSurface, SDL_PIXELFORMAT_RGB565, 0);

		SDL_SetPaletteColors(m_gamePalletisedSurface->format->palette, m_currentPalletColours, 0, 256);

		lastResHeight = m_iOrigh;
	}

	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		if (SDL_LockSurface(m_gamePalletisedSurface) < 0) {
			fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
			return;
		}
	}

	if (srcBuffer)
		memcpy(m_gamePalletisedSurface->pixels, srcBuffer, m_gamePalletisedSurface->h * m_gamePalletisedSurface->w * act_ColorsByte);

	if (SDL_MUSTLOCK(m_gamePalletisedSurface)) {
		SDL_UnlockSurface(m_gamePalletisedSurface);
	}
	SubBlit(m_iOrigw, m_iOrigh);
	//fix it !!!! SOUND_UPDATE();
}

// Automation/diagnostics: --dump-frames / --exit-after-frames (see Settings.h)
// --frame-stats: frame-interval percentiles every 5 s, to measure smoothness (pacing changes).
static void FrameStats()
{
	static std::vector<double> intervals;
	static uint64_t last = 0, windowStart = 0;
	const uint64_t now = SDL_GetPerformanceCounter();
	const double freq = (double)SDL_GetPerformanceFrequency();
	if (last)
		intervals.push_back((now - last) * 1000.0 / freq);
	else
		windowStart = now;
	last = now;
	if ((now - windowStart) / freq < 5.0 || intervals.empty())
		return;
	std::vector<double> v = intervals;
	std::sort(v.begin(), v.end());
	auto pct = [&](double p) { return v[(size_t)(p * (v.size() - 1))]; };
	fprintf(stderr, "frame-stats: %.1f fps  ms p50 %.1f p90 %.1f p99 %.1f max %.1f  (game ticks %.1f/s)\n",
		v.size() / ((now - windowStart) / freq), pct(0.5), pct(0.9), pct(0.99), v.back(),
		PortTimer_TickCount() / ((double)SDL_GetTicks64() / 1000.0));
	intervals.clear();
	windowStart = now;
}

// Optional key remapping applied before recording/injection (recordings store the result).
SDL_Scancode Port_RemapKey(SDL_Scancode sc)
{
	if (g_settings.wasdAsArrows) {
		switch (sc) {
		case SDL_SCANCODE_W: return SDL_SCANCODE_UP;
		case SDL_SCANCODE_S: return SDL_SCANCODE_DOWN;
		case SDL_SCANCODE_A: return SDL_SCANCODE_LEFT;
		case SDL_SCANCODE_D: return SDL_SCANCODE_RIGHT;
		default: break;
		}
	}
	if (g_settings.tabAsEnter && sc == SDL_SCANCODE_TAB)
		return SDL_SCANCODE_RETURN;
	return sc;
}

static void DebugFrameHooks()
{
	presentedFrames++;
	Watchdog_Kick();
	{
		extern uint64_t Port_GameStateHash();
		InputScript_CheckState(presentedFrames, Port_GameStateHash());
	}
	if (g_settings.frameStats)
		FrameStats();
	InputScript_FlushRecording();
	PortTimer_OnFrame();
	if (!g_settings.frameHashFile.empty() || !g_settings.stateTraceFile.empty() || g_settings.stateDumpFrame)
	{
		// FNV-1a 64 over the visible indexed image and the palette: identical across platforms
		// and pointer sizes, so replay goldens from the 32-bit build verify the 64-bit one.
		uint64_t h = 0xcbf29ce484222325ull;
		auto mix = [&h](const uint8_t* p, size_t n) {
			for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ull; }
		};
		const uint8_t* px = (const uint8_t*)m_gamePalletisedSurface->pixels;
		for (int y = 0; y < m_gamePalletisedSurface->h; y++)
			mix(px + y * m_gamePalletisedSurface->pitch, (size_t)m_gamePalletisedSurface->w * act_ColorsByte);
		for (int i = 0; i < 256; i++) {
			uint8_t rgb[3] = { m_currentPalletColours[i].r, m_currentPalletColours[i].g, m_currentPalletColours[i].b };
			mix(rgb, 3);
		}
		static FILE* hashFile = g_settings.frameHashFile.empty() ? nullptr : fopen(g_settings.frameHashFile.c_str(), "w");
		if (hashFile)
		{
			fprintf(hashFile, "%u %dx%d %016llx\n", presentedFrames, m_gamePalletisedSurface->w,
				m_gamePalletisedSurface->h, (unsigned long long)h);
			fflush(hashFile);
		}
		// state trace (tools/regress.py): whole game state (pointer slots normalised) + this frame
		static FILE* traceFile = g_settings.stateTraceFile.empty() ? nullptr : fopen(g_settings.stateTraceFile.c_str(), "w");
		extern uint64_t Port_FullStateHash(FILE* dump);
		if (traceFile)
		{
			fprintf(traceFile, "%u %016llx %016llx\n", presentedFrames, (unsigned long long)Port_FullStateHash(nullptr),
				(unsigned long long)h);
			if (presentedFrames % 256 == 0)
				fflush(traceFile);
		}
		// --state-dump <frame>:<file>: the normalised game state of one frame (tools/diff_state.py)
		if (g_settings.stateDumpFrame == presentedFrames && !g_settings.stateDumpFile.empty())
		{
			if (FILE* f = fopen(g_settings.stateDumpFile.c_str(), "wb"))
			{
				Port_FullStateHash(f);
				fclose(f);
			}
		}
	}
	if (!g_settings.dumpFramesDir.empty() && (presentedFrames % g_settings.dumpEvery) == 0)
	{
		char name[64];
		snprintf(name, sizeof(name), "/frame%06u.bmp", presentedFrames);
		std::string path = g_settings.dumpFramesDir + name;
		if (SDL_SaveBMP(m_gamePalletisedSurface, path.c_str()) != 0)
			printf("SDL_SaveBMP(%s) failed: %s\n", path.c_str(), SDL_GetError());
	}
	if (g_settings.exitAfterFrames > 0 && presentedFrames >= (uint32_t)g_settings.exitAfterFrames)
	{
		printf("exit after %u frames\n", presentedFrames);
		fflush(stdout);
		std::_Exit(0);
	}
	// Headless replays end 10 s (game time) after the script's last event instead of idling forever.
	if (g_settings.headless && g_settings.exitAfterFrames <= 0 && InputScript_Exhausted(presentedFrames, 300))
	{
		printf("input script exhausted, exit at frame %u\n", presentedFrames);
		fflush(stdout);
		std::_Exit(0);
	}
}

void SubBlit(uint16_t originalResWidth, uint16_t originalResHeight) {
	DebugFrameHooks();
	if (!g_presenter)
		return;
	FrameView f;
	f.pixels = (const uint8_t*)m_gamePalletisedSurface->pixels;
	f.width = originalResWidth;
	f.height = originalResHeight;
	f.pitch = m_gamePalletisedSurface->pitch;
	f.bytesPerPixel = act_ColorsByte;
	g_presenter->Present(f);
}

void VGA_Init_test() {}

void VGA_Debug_Blit(int width, int height, Uint8* buffer) {
	VGA_Blit(buffer);
}

void VGA_close()
{
	SDL_FreeSurface(surface_font);
	surface_font = NULL;
	SDL_FreeSurface(m_gamePalletisedSurface);
	m_gamePalletisedSurface = NULL;
	if (g_presenter)
		g_presenter->Shutdown();
	g_presenter.reset();
	SDL_Quit();
}

int16_t VGA_get_shift_status() {
	return 0;
}
bool VGA_check_standart_input_status() {
	bool locpressed = pressed;
	//uint16_t loclastchar = lastchar;
	pressed = false;
	return locpressed;
}

uint16_t fixchar(uint16_t loclastchar) {
	auto sdl_char = (loclastchar & 0xff00) >> 8;
	if (settingWASD) {
		switch (sdl_char) {
        case SDL_SCANCODE_W:
			sdl_char = SDL_SCANCODE_UP;
			break;
        case SDL_SCANCODE_A:
			sdl_char = SDL_SCANCODE_LEFT;
			break;
        case SDL_SCANCODE_S:
			sdl_char = SDL_SCANCODE_DOWN;
			break;
        case SDL_SCANCODE_D:
			sdl_char = SDL_SCANCODE_RIGHT;
			break;
        case SDL_SCANCODE_LSHIFT:
			sdl_char = SDL_SCANCODE_RCTRL;
			break;
		}
	}

	switch (sdl_char)
	{
	case SDL_SCANCODE_ESCAPE://esc
		loclastchar = 0x011b;
		break;

	case SDL_SCANCODE_1://1
		loclastchar = 0x0231;
		break;
	case SDL_SCANCODE_2://2
		loclastchar = 0x0332;
		break;
	case SDL_SCANCODE_3://3
		loclastchar = 0x0433;
		break;
	case SDL_SCANCODE_4://4
		loclastchar = 0x0534;
		break;
	case SDL_SCANCODE_5://5
		loclastchar = 0x0635;
		break;
	case SDL_SCANCODE_6://6
		loclastchar = 0x0736;
		break;
	case SDL_SCANCODE_7://7
		loclastchar = 0x0837;
		break;
	case SDL_SCANCODE_8://8
		loclastchar = 0x0938;
		break;
	case SDL_SCANCODE_9://9
		loclastchar = 0x0a39;
		break;
	case SDL_SCANCODE_0://0
		loclastchar = 0x0b30;
		break;

	case SDL_SCANCODE_MINUS://-
	case SDL_SCANCODE_KP_MINUS:
		loclastchar = 0x0c2d;
		break;
	case SDL_SCANCODE_EQUALS://=
	case SDL_SCANCODE_KP_PLUS:
		loclastchar = 0x0d3d;
		break;
	case SDL_SCANCODE_BACKSPACE://backspace
		loclastchar = 0x0E08;
		break;
	case SDL_SCANCODE_TAB://tab
	case SDL_SCANCODE_KP_TAB:
		loclastchar = 0x0F09;
		break;
	case SDL_SCANCODE_Q://q
		loclastchar = 0x1071;
		break;
	case SDL_SCANCODE_W://w
		loclastchar = 0x1177;
		break;
	case SDL_SCANCODE_E://e
		loclastchar = 0x1265;
		break;
	case SDL_SCANCODE_R://r
		loclastchar = 0x1372;
		break;
	case SDL_SCANCODE_T://t
		loclastchar = 0x1474;
		break;
	case SDL_SCANCODE_Y://y
		loclastchar = 0x1579;
		break;
	case SDL_SCANCODE_U://u
		loclastchar = 0x1675;
		break;
	case SDL_SCANCODE_I://i
		loclastchar = 0x1769;
		break;
	case SDL_SCANCODE_O://o
		loclastchar = 0x186f;
		break;
	case SDL_SCANCODE_P://p
		loclastchar = 0x1970;
		break;

	case SDL_SCANCODE_LEFTBRACKET://[
		loclastchar = 0x1a5b;
		break;
	case SDL_SCANCODE_RIGHTBRACKET://]
		loclastchar = 0x1b5d;
		break;

	case SDL_SCANCODE_RETURN://enter
	case SDL_SCANCODE_RETURN2://enter
		loclastchar = 0x1c0d;
		break;
	case SDL_SCANCODE_LCTRL://ctrl
	case SDL_SCANCODE_RCTRL:
		loclastchar = 0x1d00;
		break;

	case SDL_SCANCODE_A://a
		loclastchar = 0x1e61;
		break;
	case SDL_SCANCODE_S://s
		loclastchar = 0x1f73;
		break;
	case SDL_SCANCODE_D://d
		loclastchar = 0x2064;
		break;
	case SDL_SCANCODE_F://f
		loclastchar = 0x2166;
		break;
	case SDL_SCANCODE_G://g
		loclastchar = 0x2267;
		break;
	case SDL_SCANCODE_H://h
		loclastchar = 0x2368;
		break;
	case SDL_SCANCODE_J://j
		loclastchar = 0x246a;
		break;
	case SDL_SCANCODE_K://k
		loclastchar = 0x256b;
		break;
	case SDL_SCANCODE_L://l
		loclastchar = 0x266c;
		break;

	case SDL_SCANCODE_SEMICOLON://;
		loclastchar = 0x273b;
		break;
	case SDL_SCANCODE_APOSTROPHE://'
		loclastchar = 0x2827;
		break;
	case SDL_SCANCODE_GRAVE://`
		loclastchar = 0x2960;
		break;

	case SDL_SCANCODE_LSHIFT://left shift
		loclastchar = 0x2a00;
		break;
	case SDL_SCANCODE_BACKSLASH:// "\"
		loclastchar = 0x2b5c;
		break;

	case SDL_SCANCODE_Z://z
		loclastchar = 0x2c7a;
		break;
	case SDL_SCANCODE_X://x
		loclastchar = 0x2d78;
		break;
	case SDL_SCANCODE_C://c
		loclastchar = 0x2e63;
		break;
	case SDL_SCANCODE_V://v
		loclastchar = 0x2f76;
		break;
	case SDL_SCANCODE_B://b
		loclastchar = 0x3062;
		break;
	case SDL_SCANCODE_N://n
		loclastchar = 0x316e;
		break;
	case SDL_SCANCODE_M://m
		loclastchar = 0x326d;
		break;
	case SDL_SCANCODE_COMMA://,
		loclastchar = 0x332c;
		break;
	case SDL_SCANCODE_PERIOD://.
		loclastchar = 0x342e;
		break;
	case SDL_SCANCODE_SLASH:// "/"
		loclastchar = 0x352f;
		break;

	case SDL_SCANCODE_RSHIFT://right shift
		loclastchar = 0x3600;
		break;

	case SDL_SCANCODE_LALT://alt
	case SDL_SCANCODE_RALT:
		loclastchar = 0x3800;
		break;

	case SDL_SCANCODE_SPACE://space
		loclastchar = 0x3920;
		break;

	case SDL_SCANCODE_F1://f1
		loclastchar = 0x3b00;
		break;
	case SDL_SCANCODE_F2://f2
		loclastchar = 0x3c00;
		break;
	case SDL_SCANCODE_F3://f3
		loclastchar = 0x3d00;
		break;
	case SDL_SCANCODE_F4://f4
		loclastchar = 0x3e00;
		break;
	case SDL_SCANCODE_F5://f5
		loclastchar = 0x3f00;
		break;
	case SDL_SCANCODE_F6://f6
		loclastchar = 0x4000;
		break;
	case SDL_SCANCODE_F7://f7
		loclastchar = 0x4100;
		break;
	case SDL_SCANCODE_F8://f8
		loclastchar = 0x4200;
		break;
	case SDL_SCANCODE_F9://f9
		loclastchar = 0x4300;
		break;
	case SDL_SCANCODE_F10://f10
		loclastchar = 0x4400;
		break;

	case SDL_SCANCODE_HOME://home
		loclastchar = 0x4700;
		break;

	case SDL_SCANCODE_UP://up
		loclastchar = 0x4800;
		break;
	case SDL_SCANCODE_PAGEUP://pageup
		loclastchar = 0x4900;
		break;
	case SDL_SCANCODE_RIGHT://right
		loclastchar = 0x4d00;
		break;
	case SDL_SCANCODE_DOWN://down
		loclastchar = 0x5000;
		break;
	case SDL_SCANCODE_LEFT://left
		loclastchar = 0x4b00;
		break;
	case SDL_SCANCODE_END://end
		loclastchar = 0x4f00;
		break;
	case SDL_SCANCODE_PAGEDOWN://pagedown
		loclastchar = 0x5000;
		break;

	case SDL_SCANCODE_INSERT://ins
		loclastchar = 0x5200;
		break;
	case SDL_SCANCODE_DELETE://del
		loclastchar = 0x5300;
		break;
	}

	return loclastchar;
}

void VGA_cleanKeyBuffer() {
	uint16_t loclastchar = lastchar;
	lastchar = 0;
	loclastchar = fixchar(loclastchar);
	while (loclastchar != 0)
	{
		loclastchar = lastchar;
		lastchar = 0;
		loclastchar = fixchar(loclastchar);
	}
}

uint16_t VGA_read_char_from_buffer() {
	//bool locpressed = pressed;
	uint16_t loclastchar = lastchar;
	lastchar = 0;
	loclastchar = fixchar(loclastchar);
	return loclastchar;
}

void setPress(bool locpressed, uint16_t loclastchar) {
	loclastchar = fixchar(loclastchar);

	if (locpressed)
	{
		lastPressedKey_12EF70_12EF60 = (loclastchar & 0xff00) >> 8;// VGA_read_char_from_buffer();
		pressedKeys_12EEF0_12EEE0[lastPressedKey_12EF70_12EF60 & 0x7F] = lastPressedKey_12EF70_12EF60;
	}
	else
	{
		pressedKeys_12EEF0_12EEE0[((loclastchar & 0xff00) >> 8) & 0x7F] = 0;
	}
}

// Single entry point for key input (live SDL events and input scripts).
// Feeds both the INT 16h-style "last char" buffer and the INT 9-style pressed-key table.
void VGA_InjectKey(bool down, int sdlScancode) {
	uint16_t code = (uint16_t)(((sdlScancode & 0xff) << 8) | (SDL_GetKeyFromScancode((SDL_Scancode)sdlScancode) & 0xff));
	if (down) {
		pressed = true;
		lastchar = code;
	}
	setPress(down, code);
}

void VGA_mouse_clear_keys() {
	for (int i = 0; i < 128; i++)
		pressedKeys_12EEF0_12EEE0[i] = 0;
}
uint32_t VGA_PresentedFrames()
{
	return presentedFrames;
}
