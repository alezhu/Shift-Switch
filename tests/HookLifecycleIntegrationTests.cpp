#include "TestHarness.h"
#include "HookLifecycleIntegrationTests.h"
#include "HookController.h"
#include "CHandle.h"

#include <windows.h>
#include <shlwapi.h>
#include <vector>
#include <string>

std::wstring FindHookDll()
{
#ifdef _WIN64
	const wchar_t* dllName = L"ShiftSwitch64.dll";
#else
	const wchar_t* dllName = L"ShiftSwitch.dll";
#endif

	wchar_t exePath[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	PathRemoveFileSpecW(exePath);

	wchar_t currentDir[MAX_PATH] = L"";
	wcscpy_s(currentDir, exePath);

	for (int level = 0; level < 6; ++level)
	{
		const std::vector<std::wstring> checks = {
			std::wstring(currentDir) + L"\\" + dllName,
			std::wstring(currentDir) + L"\\bin\\Release\\" + dllName,
			std::wstring(currentDir) + L"\\bin\\Debug\\" + dllName,
			std::wstring(currentDir) + L"\\Release\\" + dllName,
			std::wstring(currentDir) + L"\\Debug\\" + dllName
		};

		for (const auto& path : checks)
		{
			const DWORD attr = GetFileAttributesW(path.c_str());
			if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
			{
				wchar_t fullPath[MAX_PATH] = L"";
				GetFullPathNameW(path.c_str(), MAX_PATH, fullPath, nullptr);
				return std::wstring(fullPath);
			}
		}

		if (!PathRemoveFileSpecW(currentDir))
			break;
	}
	return L"";
}

struct HookControllerTestAccessor
{
	static SETHOOK& GetSetHookFn(HookController& controller) { return controller.m_pfnSETHOOK; }
	static HMODULE GetLibHandle(const HookController& controller) { return controller.m_hLib; }
};

namespace
{
	static BOOL APIENTRY MockFailingSetHook(BOOL bSet)
	{
		if (!bSet) return FALSE;
		return TRUE;
	}

	// Test 1: Real hook DLL loading, SetHook(TRUE), SetHook(FALSE), FreeLibrary lifecycle
	void testHookEnableDisableLifecycle(const std::wstring& dllPath)
	{
		HookController controller(dllPath);
		CHECK_FALSE(controller.IsActive());

		// 1.1: Enable
		const bool enabled = controller.Enable();
		CHECK_TRUE(enabled);
		CHECK_TRUE(controller.IsActive());

		// 1.2: Enable idempotence (redundant enable is a no-op)
		const bool reenabled = controller.Enable();
		CHECK_TRUE(reenabled);
		CHECK_TRUE(controller.IsActive());

		// 1.3: Disable
		const bool disabled = controller.Disable();
		CHECK_TRUE(disabled);
		CHECK_FALSE(controller.IsActive());

		// 1.4: Disable idempotence (redundant disable is a no-op)
		const bool redisabled = controller.Disable();
		CHECK_TRUE(redisabled);
		CHECK_FALSE(controller.IsActive());
	}

	// Test 2: Rapid Enable/Disable multi-cycle stress test
	void testHookMultiCycleStress(const std::wstring& dllPath)
	{
		HookController controller(dllPath);
		constexpr int CYCLES = 5;
		for (int i = 0; i < CYCLES; ++i)
		{
			CHECK_TRUE(controller.Enable());
			CHECK_TRUE(controller.IsActive());
			CHECK_TRUE(controller.Disable());
			CHECK_FALSE(controller.IsActive());
		}
	}

	// Test 3: Shutdown clean teardown
	void testHookShutdown(const std::wstring& dllPath)
	{
		HookController controller(dllPath);
		CHECK_TRUE(controller.Enable());
		CHECK_TRUE(controller.IsActive());

		controller.Shutdown();
		CHECK_FALSE(controller.IsActive());

		// Multiple shutdowns should be harmless
		controller.Shutdown();
		CHECK_FALSE(controller.IsActive());
	}

	// Test 4: Low-level hook mutex coexistence and handling
	void testLowLevelHookMutexBehavior(const std::wstring& dllPath)
	{
		// Even if another ShiftSwitch instance is already running and owns
		// Local\ShiftSwitch_LowLevelHook, Enable() must handle ERROR_ALREADY_EXISTS
		// gracefully without crashing or hanging, and Disable() must cleanly exit.
		HookController controller(dllPath);
		CHECK_TRUE(controller.Enable());
		CHECK_TRUE(controller.IsActive());

		// Verify mutex exists while active
		CHandle mutex(OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\ShiftSwitch_LowLevelHook"));
		CHECK_TRUE(mutex.isValid());

		CHECK_TRUE(controller.Disable());
		CHECK_FALSE(controller.IsActive());
	}

	// Test 5: Error handling with invalid / nonexistent DLL path
	void testHookInvalidDllPath()
	{
		HookController controller(L"C:\\invalid_path\\non_existent_shiftswitch.dll");
		CHECK_FALSE(controller.Enable());
		CHECK_FALSE(controller.IsActive());
		CHECK_TRUE(controller.Disable());
	}

	// Test 6: Disable failure retains DLL and active state so disable can be retried
	void testHookDisableFailureRetainsState(const std::wstring& dllPath)
	{
		HookController controller(dllPath);
		CHECK_TRUE(controller.Enable());
		CHECK_TRUE(controller.IsActive());
		CHECK_TRUE(HookControllerTestAccessor::GetLibHandle(controller) != nullptr);

		const auto realSetHook = HookControllerTestAccessor::GetSetHookFn(controller);
		CHECK_TRUE(realSetHook != nullptr);

		// Temporarily mock SetHook to simulate SetHook(FALSE) failure
		HookControllerTestAccessor::GetSetHookFn(controller) = &MockFailingSetHook;

		// Disable() must return false, must NOT unload the DLL, and must remain active
		CHECK_FALSE(controller.Disable());
		CHECK_TRUE(controller.IsActive());
		CHECK_TRUE(HookControllerTestAccessor::GetLibHandle(controller) != nullptr);

		// Restore real SetHook: subsequent Disable() must succeed and cleanly unload
		HookControllerTestAccessor::GetSetHookFn(controller) = realSetHook;
		CHECK_TRUE(controller.Disable());
		CHECK_FALSE(controller.IsActive());
		CHECK_TRUE(HookControllerTestAccessor::GetLibHandle(controller) == nullptr);

		// Redundant disable is idempotent
		CHECK_TRUE(controller.Disable());
	}
}

void runHookLifecycleIntegrationTests()
{
	std::printf("--- Hook Lifecycle Integration Tests ---\n");

	const std::wstring dllPath = FindHookDll();
	if (dllPath.empty())
	{
		std::printf("  WARNING: ShiftSwitch hook DLL not found. Skipping DLL tests.\n");
		CHECK(false); // Signal failure so missing build output is caught
		return;
	}

	std::printf("  Testing with DLL: %ls\n", dllPath.c_str());
	testHookEnableDisableLifecycle(dllPath);
	testHookMultiCycleStress(dllPath);
	testHookShutdown(dllPath);
	testLowLevelHookMutexBehavior(dllPath);
	testHookInvalidDllPath();
	testHookDisableFailureRetainsState(dllPath);
}
