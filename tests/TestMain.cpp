#include "TestHarness.h"
#include "ShiftTapTests.h"
#include "ExclusionsIntegrationTests.h"
#include "HookLifecycleIntegrationTests.h"
#include "IpcIntegrationTests.h"
#include "IconHelperTests.h"
#include "AppSettingsTests.h"
#include "TsfSyncTests.h"

#include <windows.h>
#include <cstdio>
#include <cwchar>

int wmain(int argc, wchar_t* argv[])
{
	// Child process mode for cross-process integration tests
	if (argc > 1 && wcscmp(argv[1], L"--test-child-worker") == 0)
	{
		return runChildWorker(argc, argv);
	}

	std::printf("========================================\n");
	std::printf("   ShiftSwitch Integration Test Suite   \n");
	std::printf("========================================\n\n");

	runShiftTapTests();
	std::printf("\n");

	runExclusionsIntegrationTests();
	std::printf("\n");

	runHookLifecycleIntegrationTests();
	std::printf("\n");

	runIpcIntegrationTests();
	std::printf("\n");

	runIconHelperTests();
	std::printf("\n");

	runAppSettingsTests();
	std::printf("\n");

	runTsfSyncTests();
	std::printf("\n");

	std::printf("========================================\n");
	std::printf("Total: %d checks, %d failures\n", ::TestHarness::g_checks, ::TestHarness::g_failures);
	std::printf("========================================\n");

	return ::TestHarness::g_failures == 0 ? 0 : 1;
}
