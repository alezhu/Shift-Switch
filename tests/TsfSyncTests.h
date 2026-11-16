#pragma once

#include "TestHarness.h"
#include "TsfSync.h"
#include <windows.h>

inline void testTsfSyncNull()
{
	CHECK_FALSE(TsfSync::SyncTsfProfile(nullptr));
}

inline void testTsfSyncValid()
{
	const HKL hklEn = LoadKeyboardLayoutW(L"00000409", 0);
	if (hklEn != nullptr)
	{
		CHECK_TRUE(TsfSync::SyncTsfProfile(hklEn));
	}
	const HKL hklRu = LoadKeyboardLayoutW(L"00000419", 0);
	if (hklRu != nullptr)
	{
		CHECK_TRUE(TsfSync::SyncTsfProfile(hklRu));
	}
}

inline void runTsfSyncTests()
{
	testTsfSyncNull();
	testTsfSyncValid();
}
