// Tier 2: tests that need the extracted game data (REMC1_DATA_DIR). Skipped otherwise.
#include <gtest/gtest.h>

#include "support/test_support.h"
#include "sub_main.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
using namespace remc1test;

// RncUnpack_62B60_63070_orig: the faithful decompiled in-place decoder. RncUnpack_62B60_63070: what
// the game now calls (swars decoder on a padded copy). UnpackM1: swars decoder used by the old port.
int RncUnpack_62B60_63070(uint8_t* input, uint8_t* output);
int RncUnpack_62B60_63070_orig(uint8_t* input, uint8_t* output);
long UnpackM1(unsigned char* buffer, unsigned long bufsize);

namespace {

uint32_t BigEndian32(const uint8_t* p)
{
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

bool IsRnc(const std::vector<uint8_t>& d)
{
	return d.size() >= 18 && memcmp(d.data(), "RNC\x01", 4) == 0;
}

std::vector<fs::path> DataFiles(const std::string& dataDir)
{
	std::vector<fs::path> files;
	for (auto& e : fs::recursive_directory_iterator(fs::path(dataDir) / "CD"))
		if (e.is_regular_file()) {
			auto ext = e.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
			if ((ext == ".DAT" || ext == ".TAB" || ext == ".PAL") && e.file_size() < 4 * 1024 * 1024)
				files.push_back(e.path());
		}
	std::sort(files.begin(), files.end());
	return files;
}

} // namespace

// The decompiled in-place RNC decoder must agree with the independent swars implementation on
// every packed file shipped with the game, and the output must match the committed hashes.
TEST(GameData, RncDecodersAgreeAndMatchGoldens)
{
	std::string data = DataDir();
	if (data.empty())
		GTEST_SKIP() << "set REMC1_DATA_DIR to the extracted game data";

	Goldens goldens("data_hashes.txt");
	int packed = 0;
	for (auto& file : DataFiles(data)) {
		std::vector<uint8_t> raw = ReadFile(file);
		std::string key = fs::relative(file, data).generic_string();
		std::transform(key.begin(), key.end(), key.begin(), ::toupper);
		std::vector<uint8_t> out;
		if (IsRnc(raw)) {
			packed++;
			uint32_t unpackedLen = BigEndian32(&raw[4]);
			// the game decodes in place and needs head room past the unpacked size
			GameBytes a(std::max<size_t>(unpackedLen, raw.size()) + 4096);
			memcpy(a.data(), raw.data(), raw.size());
			int lenA = RncUnpack_62B60_63070_orig(a.data(), a.data());
			ASSERT_EQ((uint32_t)lenA, unpackedLen) << key;

			// what the game calls: decode into an exactly-sized buffer (no head room needed any more)
			GameBytes c(unpackedLen);
			GameBytes in(raw.begin(), raw.end());
			int lenC = RncUnpack_62B60_63070(in.data(), c.data());
			ASSERT_EQ((uint32_t)lenC, unpackedLen) << key << " (port decoder)";
			ASSERT_EQ(0, memcmp(a.data(), c.data(), unpackedLen)) << key << ": port decoder differs from original";

			std::vector<uint8_t> b(a.size());
			memcpy(b.data(), raw.data(), raw.size());
			long lenB = UnpackM1(b.data(), (unsigned long)b.size());
			ASSERT_EQ((uint32_t)lenB, unpackedLen) << key << " (swars decoder)";
			ASSERT_EQ(0, memcmp(a.data(), b.data(), unpackedLen)) << key << ": decoders disagree";
			out.assign(a.begin(), a.begin() + unpackedLen);
		}
		else {
			out = raw;
		}
		std::string expected;
		std::string got = HashHex(Fnv1a64(out.data(), out.size())) + " " + std::to_string(out.size());
		EXPECT_TRUE(goldens.Check(key, got, &expected)) << key << ": got " << got << " expected " << expected;
	}
	EXPECT_GT(packed, 20) << "expected many RNC-packed files";
}
