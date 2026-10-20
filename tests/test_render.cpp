// Tier 1: the software rasteriser (DrawTriangle_729A3_72EB3): vertex-order independence, top-edge
// clipping, shading/texture interpolation. Note: it draws clockwise triangles only (back faces are
// culled - also in the original), and the first row of a top-clipped triangle is not drawn.
// The terrain renderer splits each map cell along one of two diagonals (by cell parity) and passes
// the same corners in different orders; any order-dependent bias shows up as a checkerboard.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "sub_main.h"
#include "engine/Basic.h"
#include "support/test_support.h"

using remc1test::GameBytes;

// Rasteriser state, defined in sub_main.cpp.
extern uint8* beginPrevLineFrame_93AC8;
extern uint8* actTexture_93AD0;
extern int pitchViewPort_93AD4;
extern uint16 widthViewPort_93AD8;
extern uint16 heightViewPort_93ADC;
extern uint8 byte_967E1;
typedef struct {
	char fog_B7934_B7924[0x4000];
	char byte_BB934_BB924[0x10000];
	char byte_CB934_CB924[0x300];
	uint8 byte_CBC34_CBC24[0x300];
} TypeStrPal; // identical to the definition in sub_main.cpp
extern TypeStrPal strPal;
void DrawTriangle_729A3_72EB3(Type_RenderPoint* pnt1, Type_RenderPoint* pnt2, Type_RenderPoint* pnt3);

namespace {

constexpr int W = 320, H = 200;

// Draws the triangle with the given vertex order; returns the frame. With a constant texture and
// shade table mapping (shade << 8 | texel) -> shade, every pixel holds the interpolated shade.
GameBytes Draw(uint8_t mode, const Type_RenderPoint (&tri)[3], const int (&order)[3])
{
	static GameBytes texture(0x20000, 0x20); // texel >= 12: mode 26 does not blend with the screen
	GameBytes screen(W * H, 0xFF);
	for (int i = 0; i < 0x10000; i++)
		strPal.byte_BB934_BB924[i] = (char)(i >> 8);
	for (int i = 0; i < 0x4000; i++)
		strPal.fog_B7934_B7924[i] = (char)(i >> 8); // mode 26 shades through the fog table
	beginPrevLineFrame_93AC8 = screen.data();
	pitchViewPort_93AD4 = W;
	widthViewPort_93AD8 = W;
	heightViewPort_93ADC = H;
	actTexture_93AD0 = texture.data();
	byte_967E1 = mode;
	Type_RenderPoint p[3] = { tri[order[0]], tri[order[1]], tri[order[2]] };
	DrawTriangle_729A3_72EB3(&p[0], &p[1], &p[2]);
	return screen;
}

Type_RenderPoint Vertex(int x, int y)
{
	Type_RenderPoint v{};
	v.x_0 = x;
	v.y_1 = y;
	v.z_4 = (20 << 16) + x * 2048 + y * 4096; // affine shading: 20 + x/32 + y/16
	return v;
}

void ExpectOrderIndependent(uint8_t mode, const Type_RenderPoint (&tri)[3])
{
	// Clockwise orders only: the rasteriser culls by winding, like the terrain renderer expects.
	const int orders[3][3] = { { 0, 1, 2 }, { 1, 2, 0 }, { 2, 0, 1 } };
	const auto ref = Draw(mode, tri, orders[0]);
	int drawn = 0;
	for (uint8_t px : ref)
		drawn += px != 0xFF;
	ASSERT_GT(drawn, 100) << "mode " << (int)mode << ": nothing drawn - wrong winding or clipping setup?";
	for (const auto& o : orders) {
		const auto img = Draw(mode, tri, o);
		int worst = 0, where = -1;
		for (int i = 0; i < W * H; i++) {
			if ((ref[i] == 0xFF) != (img[i] == 0xFF))
				continue; // edge pixels may differ by one
			const int d = std::abs((int)ref[i] - (int)img[i]);
			if (d > worst) {
				worst = d;
				where = i;
			}
		}
		EXPECT_LE(worst, 1) << "mode " << (int)mode << " order " << o[0] << o[1] << o[2] << ": shade differs by "
			<< worst << " at (" << where % W << "," << where / W << ")";
	}
}

} // namespace

namespace {

// Draws `tri` into a viewport of height `h` starting `rowOffset` rows into a W x 400 buffer.
GameBytes DrawAt(uint8_t mode, Type_RenderPoint (&tri)[3], int rowOffset, int h, int order)
{
	static GameBytes texture(0x20000, 0x20);
	GameBytes screen(W * 400, 0xFF);
	for (int i = 0; i < 0x10000; i++)
		strPal.byte_BB934_BB924[i] = (char)(i >> 8);
	for (int i = 0; i < 0x4000; i++)
		strPal.fog_B7934_B7924[i] = (char)(i >> 8);
	beginPrevLineFrame_93AC8 = screen.data() + rowOffset * W;
	pitchViewPort_93AD4 = W;
	widthViewPort_93AD8 = W;
	heightViewPort_93ADC = (uint16)h;
	actTexture_93AD0 = texture.data();
	byte_967E1 = mode;
	Type_RenderPoint p[3] = { tri[order], tri[(order + 1) % 3], tri[(order + 2) % 3] };
	DrawTriangle_729A3_72EB3(&p[0], &p[1], &p[2]);
	return screen;
}

} // namespace

TEST(Rasteriser, TopClippingMatchesUnclipped)
{
	// Triangle partly above the top edge (negative y). Reference: the same triangle 100 rows lower
	// in a 400-row viewport, i.e. never clipped. Rows 100..299 of the reference must equal rows
	// 0..199 of the clipped drawing - shade and coverage.
	const int shapes[2][6] = { { 40, -60, 220, 30, 90, 150 }, { 160, -90, 260, 120, 20, 60 } };
	for (uint8_t mode : { (uint8_t)5, (uint8_t)26 }) {
		for (const auto& s : shapes) {
			for (int order = 0; order < 3; order++) {
				Type_RenderPoint clipped[3] = { Vertex(s[0], s[1]), Vertex(s[2], s[3]), Vertex(s[4], s[5]) };
				Type_RenderPoint shifted[3] = { Vertex(s[0], s[1] + 100), Vertex(s[2], s[3] + 100), Vertex(s[4], s[5] + 100) };
				// same shading as the clipped triangle (Vertex() derives shade from y)
				for (int i = 0; i < 3; i++)
					shifted[i].z_4 = clipped[i].z_4;
				const auto img = DrawAt(mode, clipped, 0, H, order);
				const auto ref = DrawAt(mode, shifted, 0, 400, order);
				int bad = 0, firstX = -1, firstY = -1, worst = 0;
				for (int y = 1; y < H; y++) // row 0 is skipped when a triangle is clipped at the top
					for (int x = 0; x < W; x++) {
						const int a = img[y * W + x], b = ref[(y + 100) * W + x];
						const bool edge = (a == 0xFF) != (b == 0xFF);
						const int d = (a == 0xFF || b == 0xFF) ? (edge ? 99 : 0) : std::abs(a - b);
						if (d > 1) {
							if (!bad++) { firstX = x; firstY = y; }
							worst = (std::max)(worst, d);
						}
					}
				// one-pixel edge differences are rounding; count only real disagreements
				EXPECT_LT(bad, 40) << "mode " << (int)mode << " shape " << (&s - shapes) << " order " << order
					<< ": " << bad << " pixels differ (worst " << worst << ", first at " << firstX << "," << firstY << ")";
			}
		}
	}
}

TEST(Rasteriser, TopClippingKeepsTextureCoordinates)
{
	// Texel value encodes u (texel = 0x40 + (u & 0x3F)); the shade tables pass the texel through, so
	// every pixel shows which u the rasteriser sampled. A triangle crossing the top edge must sample
	// the same texels as the unclipped reference.
	static GameBytes texture(0x20000);
	for (size_t i = 0; i < texture.size(); i++)
		texture[i] = (uint8_t)(0x40 + (i & 0x3F));
	const int s[6] = { 40, -60, 220, 30, 90, 150 };
	for (uint8_t mode : { (uint8_t)5, (uint8_t)26 }) {
		GameBytes out[2];
		for (int pass = 0; pass < 2; pass++) {
			const int dy = pass ? 100 : 0;
			GameBytes screen(W * 400, 0xFF);
			for (int i = 0; i < 0x10000; i++)
				strPal.byte_BB934_BB924[i] = (char)(i & 0xFF);
			for (int i = 0; i < 0x4000; i++)
				strPal.fog_B7934_B7924[i] = (char)(i & 0xFF);
			beginPrevLineFrame_93AC8 = screen.data();
			pitchViewPort_93AD4 = W;
			widthViewPort_93AD8 = W;
			heightViewPort_93ADC = (uint16)(pass ? 400 : H);
			actTexture_93AD0 = texture.data();
			byte_967E1 = mode;
			Type_RenderPoint p[3];
			for (int i = 0; i < 3; i++) {
				p[i] = {};
				p[i].x_0 = s[2 * i];
				p[i].y_1 = s[2 * i + 1] + dy;
				p[i].u_2 = s[2 * i] << 14;              // u = x / 4
				p[i].v_3 = (s[2 * i + 1] + 60) << 13;   // v = (y + 60) / 8, same for both passes
				p[i].z_4 = 30 << 16;
			}
			DrawTriangle_729A3_72EB3(&p[0], &p[1], &p[2]);
			out[pass] = screen;
		}
		int bad = 0, fx = -1, fy = -1;
		for (int y = 1; y < H; y++) // row 0: see TopClippingMatchesUnclipped
			for (int x = 0; x < W; x++) {
				const int a = out[0][y * W + x], b = out[1][(y + 100) * W + x];
				if (a != 0xFF && b != 0xFF && std::abs(a - b) > 1) {
					if (!bad++) { fx = x; fy = y; }
				}
			}
		EXPECT_EQ(bad, 0) << "mode " << (int)mode << ": " << bad << " pixels sample different texels after top clipping, first at "
			<< fx << "," << fy << ": clipped " << (int)out[0][fy * W + fx] << " vs " << (int)out[1][(fy + 100) * W + fx];
	}
}

TEST(Rasteriser, ShadeStaysBetweenVertexShades)
{
	// Small triangles of every shape class (flat top, flat bottom, general with the middle vertex
	// left or right) with in-game shade magnitudes (33..38 << 16): every pixel's shade must stay
	// within the vertex shades.
	struct Tri { const char* name; int p[6]; int z[3]; };
	const Tri tris[] = {
		{ "flat top", { 67, 18, 74, 18, 45, 34 }, { 36, 33, 35 } },
		{ "flat bottom", { 60, 10, 80, 40, 40, 40 }, { 33, 38, 35 } },
		{ "general mid-right", { 45, 10, 90, 30, 50, 60 }, { 36, 33, 35 } },
		{ "two above top, wide", { 20, 40, 200, -30, 260, -10 }, { 35, 34, 33 } },
		{ "two above top, lowest second (label124)", { 181, 40, 173, -5, 221, -12 }, { 28, 33, 30 } },
	};
	static GameBytes texture(0x20000, 0x20);
	for (const Tri& t : tris) {
		for (int order = 0; order < 3; order++) {
			GameBytes screen(W * H, 0xFF);
			for (int i = 0; i < 0x10000; i++)
				strPal.byte_BB934_BB924[i] = (char)(i >> 8);
			for (int i = 0; i < 0x4000; i++)
				strPal.fog_B7934_B7924[i] = (char)(i >> 8);
			beginPrevLineFrame_93AC8 = screen.data();
			pitchViewPort_93AD4 = W;
			widthViewPort_93AD8 = W;
			heightViewPort_93ADC = H;
			actTexture_93AD0 = texture.data();
			byte_967E1 = 5;
			Type_RenderPoint p[3];
			for (int i = 0; i < 3; i++) {
				const int k = (i + order) % 3;
				p[i] = {};
				p[i].x_0 = t.p[2 * k];
				p[i].y_1 = t.p[2 * k + 1];
				p[i].z_4 = (t.z[k] << 16) + 0x8000;
			}
			DrawTriangle_729A3_72EB3(&p[0], &p[1], &p[2]);
			const int lo = (std::min)({ t.z[0], t.z[1], t.z[2] }), hi = (std::max)({ t.z[0], t.z[1], t.z[2] });
			int drawn = 0, bad = 0, badVal = 0;
			for (uint8_t px : screen) {
				if (px == 0xFF)
					continue;
				drawn++;
				if (px < lo - 1 || px > hi + 1) {
					if (!bad++)
						badVal = px;
				}
			}
			EXPECT_GT(drawn, 0) << t.name << " order " << order << ": nothing drawn";
			EXPECT_EQ(bad, 0) << t.name << " order " << order << ": " << bad << "/" << drawn << " pixels shaded "
				<< badVal << ", vertex shades " << lo << ".." << hi;
		}
	}
}

TEST(Rasteriser, CapturedTopEdgeCells)
{
	// Terrain cells covering the top of the screen in top_render_glitch.txt frame 510 (loop 2 of
	// DrawSkyTerrainParticles): A=v213 B=v208 C=v203 D=v198 as {x, y, u, v, z}. Shades are 28..34.
	struct Cell { int parity; Type_RenderPoint a, b, c, d; };
	const Cell cells[] = {
		{ 0, { 105, 44, 0, 0, 1933312 }, { 142, 5, 2097151, 0, 2260992 }, { 173, -5, 2097151, 2097151, 2195456 }, { 133, 43, 0, 2097151, 1867776 } },
		{ 1, { 133, 43, 0, 2097151, 1867776 }, { 173, -5, 2097151, 2097151, 2195456 }, { 221, -12, 2097151, 0, 1998848 }, { 181, 40, 0, 0, 1867776 } },
	};
	static GameBytes texture(0x20000, 0x20);
	int idx = 0;
	for (const Cell& c : cells) {
		GameBytes screen(W * H, 0xFF);
		for (int i = 0; i < 0x10000; i++)
			strPal.byte_BB934_BB924[i] = (char)(i >> 8);
		for (int i = 0; i < 0x4000; i++)
			strPal.fog_B7934_B7924[i] = (char)(i >> 8);
		beginPrevLineFrame_93AC8 = screen.data();
		pitchViewPort_93AD4 = W;
		widthViewPort_93AD8 = W;
		heightViewPort_93ADC = H;
		actTexture_93AD0 = texture.data();
		byte_967E1 = 5;
		Type_RenderPoint a = c.a, b = c.b, cc = c.c, d = c.d;
		// loop 2 triangle orders (DrawSkyTerrainParticles near pass)
		if (c.parity) {
			DrawTriangle_729A3_72EB3(&a, &b, &d);
			DrawTriangle_729A3_72EB3(&d, &b, &cc);
		} else {
			DrawTriangle_729A3_72EB3(&a, &b, &cc);
			DrawTriangle_729A3_72EB3(&a, &cc, &d);
		}
		int drawn = 0, bad = 0, badVal = 0, bx = 0, by = 0;
		for (int i = 0; i < W * H; i++) {
			if (screen[i] == 0xFF)
				continue;
			drawn++;
			if (screen[i] < 27 || screen[i] > 35) {
				if (!bad++) { badVal = screen[i]; bx = i % W; by = i / W; }
			}
		}
		EXPECT_GT(drawn, 500) << "cell " << idx;
		EXPECT_EQ(bad, 0) << "cell " << idx << ": " << bad << " of " << drawn << " pixels shaded out of range, first " << badVal << " at " << bx << "," << by;
		idx++;
	}
}

TEST(Rasteriser, ShadingIndependentOfVertexOrder)
{
	const Type_RenderPoint a[3] = { Vertex(40, 20), Vertex(200, 60), Vertex(90, 170) };   // middle vertex right
	const Type_RenderPoint b[3] = { Vertex(150, 20), Vertex(220, 170), Vertex(30, 110) }; // middle vertex left
	for (uint8_t mode : { (uint8_t)5, (uint8_t)26 }) {
		ExpectOrderIndependent(mode, a);
		ExpectOrderIndependent(mode, b);
	}
}
