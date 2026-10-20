// Diagnostic (not run by default): checks offsets implied by field *names* only.
// Run: remc1-tests --gtest_also_run_disabled_tests --gtest_filter=*LayoutNames*
// A failure means either a misaligned struct (real bug: named and raw-offset accesses disagree)
// or a misleading name. Fix the struct or the name, then move the fact into test_layout.cpp.
#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "sub_main.h"
#include "engine/Basic.h"

TEST(DISABLED_LayoutNames, AllNameImpliedOffsets)
{
	int bad = 0;
#define LAYOUT_NAME_OFFSET(T, F, N)                                                                  \
	if (offsetof(T, F) != (size_t)(N)) {                                                             \
		bad++;                                                                                       \
		ADD_FAILURE() << #T "::" #F " is at " << offsetof(T, F) << ", name says " << (N);           \
	}
#include "generated/layout_name_cases.inc"
#undef LAYOUT_NAME_OFFSET
	EXPECT_EQ(bad, 0);
}
