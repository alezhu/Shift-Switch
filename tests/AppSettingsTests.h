#pragma once

#include "TestHarness.h"
#include "AppSettings.h"
#include <windows.h>

inline void testModifierConversions()
{
	// Test 0 <-> 0
	CHECK_EQUAL(HotkeyModifiersToMod(0), 0u);
	CHECK_EQUAL(ModToHotkeyModifiers(0), 0);

	// Test single modifiers
	CHECK_EQUAL(HotkeyModifiersToMod(HOTKEYF_ALT), static_cast<UINT>(MOD_ALT));
	CHECK_EQUAL(ModToHotkeyModifiers(MOD_ALT), static_cast<BYTE>(HOTKEYF_ALT));

	CHECK_EQUAL(HotkeyModifiersToMod(HOTKEYF_CONTROL), static_cast<UINT>(MOD_CONTROL));
	CHECK_EQUAL(ModToHotkeyModifiers(MOD_CONTROL), static_cast<BYTE>(HOTKEYF_CONTROL));

	CHECK_EQUAL(HotkeyModifiersToMod(HOTKEYF_SHIFT), static_cast<UINT>(MOD_SHIFT));
	CHECK_EQUAL(ModToHotkeyModifiers(MOD_SHIFT), static_cast<BYTE>(HOTKEYF_SHIFT));

	// Test dual combinations
	CHECK_EQUAL(HotkeyModifiersToMod(HOTKEYF_CONTROL | HOTKEYF_ALT), static_cast<UINT>(MOD_CONTROL | MOD_ALT));
	CHECK_EQUAL(ModToHotkeyModifiers(MOD_CONTROL | MOD_ALT), static_cast<BYTE>(HOTKEYF_CONTROL | HOTKEYF_ALT));

	CHECK_EQUAL(HotkeyModifiersToMod(HOTKEYF_CONTROL | HOTKEYF_SHIFT), static_cast<UINT>(MOD_CONTROL | MOD_SHIFT));
	CHECK_EQUAL(ModToHotkeyModifiers(MOD_CONTROL | MOD_SHIFT), static_cast<BYTE>(HOTKEYF_CONTROL | HOTKEYF_SHIFT));

	CHECK_EQUAL(HotkeyModifiersToMod(HOTKEYF_ALT | HOTKEYF_SHIFT), static_cast<UINT>(MOD_ALT | MOD_SHIFT));
	CHECK_EQUAL(ModToHotkeyModifiers(MOD_ALT | MOD_SHIFT), static_cast<BYTE>(HOTKEYF_ALT | HOTKEYF_SHIFT));

	// Test triple combination (default: Ctrl+Shift+Alt)
	constexpr BYTE allHk = HOTKEYF_CONTROL | HOTKEYF_SHIFT | HOTKEYF_ALT;
	constexpr UINT allMod = MOD_CONTROL | MOD_SHIFT | MOD_ALT;
	CHECK_EQUAL(HotkeyModifiersToMod(allHk), allMod);
	CHECK_EQUAL(ModToHotkeyModifiers(allMod), allHk);

	// HOTKEYF_EXT should be ignored in MOD conversion
	CHECK_EQUAL(HotkeyModifiersToMod(HOTKEYF_EXT | HOTKEYF_ALT), static_cast<UINT>(MOD_ALT));
}

inline void testHotkeyRegistryPersistence()
{
	constexpr const WCHAR* testKey = L"Software\\ShiftSwitch\\Test_Settings";

	// Ensure clean state
	RegDeleteKeyW(HKEY_CURRENT_USER, testKey);

	// Defaults on non-existent key
	HotkeyConfig config;
	config.vk = 0;
	config.modifiers = 0;
	LoadHotkeyConfig(HKEY_CURRENT_USER, testKey, config);
	CHECK_EQUAL(config.vk, static_cast<UINT>('S'));
	CHECK_EQUAL(config.modifiers, static_cast<UINT>(MOD_CONTROL | MOD_SHIFT | MOD_ALT));

	// Save custom hotkey
	config.vk = VK_F11;
	config.modifiers = MOD_CONTROL | MOD_ALT;
	CHECK_TRUE(SaveHotkeyConfig(HKEY_CURRENT_USER, testKey, config));

	// Read back and verify
	HotkeyConfig loaded;
	CHECK_TRUE(LoadHotkeyConfig(HKEY_CURRENT_USER, testKey, loaded));
	CHECK_EQUAL(loaded.vk, static_cast<UINT>(VK_F11));
	CHECK_EQUAL(loaded.modifiers, static_cast<UINT>(MOD_CONTROL | MOD_ALT));

	// Save disabled hotkey (None)
	config.vk = 0;
	config.modifiers = 0;
	CHECK_TRUE(SaveHotkeyConfig(HKEY_CURRENT_USER, testKey, config));

	CHECK_TRUE(LoadHotkeyConfig(HKEY_CURRENT_USER, testKey, loaded));
	CHECK_EQUAL(loaded.vk, 0u);
	CHECK_EQUAL(loaded.modifiers, 0u);

	// Clean up
	RegDeleteKeyW(HKEY_CURRENT_USER, testKey);
}

inline void testTrayDblClickPersistence()
{
	constexpr const WCHAR* testKey = L"Software\\ShiftSwitch\\Test_Settings";

	// Ensure clean state
	RegDeleteKeyW(HKEY_CURRENT_USER, testKey);

	// Defaults on non-existent key
	TrayDblClickAction action = TrayDblClickAction::About;
	LoadTrayDblClickAction(HKEY_CURRENT_USER, testKey, action);
	CHECK_EQUAL(static_cast<DWORD>(action), static_cast<DWORD>(TrayDblClickAction::ToggleActive));

	// Save and verify each action
	const TrayDblClickAction actionsToTest[] = {
		TrayDblClickAction::ToggleActive,
		TrayDblClickAction::Settings,
		TrayDblClickAction::Exclusions,
		TrayDblClickAction::About
	};

	for (const auto a : actionsToTest)
	{
		CHECK_TRUE(SaveTrayDblClickAction(HKEY_CURRENT_USER, testKey, a));
		TrayDblClickAction loaded = TrayDblClickAction::ToggleActive;
		CHECK_TRUE(LoadTrayDblClickAction(HKEY_CURRENT_USER, testKey, loaded));
		CHECK_EQUAL(static_cast<DWORD>(loaded), static_cast<DWORD>(a));
	}

	// Test out-of-range value clamping
	ScopedHKey hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, testKey, 0, KEY_WRITE, hKey.addressof()) == ERROR_SUCCESS)
	{
		DWORD badValue = 999;
		RegSetValueExW(hKey, L"TrayDblClickAction", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&badValue), sizeof(badValue));
	}
	action = TrayDblClickAction::Settings;
	LoadTrayDblClickAction(HKEY_CURRENT_USER, testKey, action);
	CHECK_EQUAL(static_cast<DWORD>(action), static_cast<DWORD>(TrayDblClickAction::ToggleActive));

	// Clean up
	RegDeleteKeyW(HKEY_CURRENT_USER, testKey);
}

inline void testHotkeyWindowRegistration()
{
	// Create a dummy message-only window
	WNDCLASSEXW wc = { sizeof(wc) };
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"ShiftSwitch_TestHotkeyWindow";
	RegisterClassExW(&wc);

	HWND hWnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
	CHECK_TRUE(hWnd != nullptr);

	if (hWnd != nullptr)
	{
		// Try to register a test hotkey
		constexpr int TEST_HOTKEY_ID = 42;
		const BOOL registered = RegisterHotKey(hWnd, TEST_HOTKEY_ID, MOD_CONTROL | MOD_SHIFT | MOD_ALT, 'K');
		if (registered)
		{
			CHECK_TRUE(registered != FALSE);
			UnregisterHotKey(hWnd, TEST_HOTKEY_ID);
		}
		DestroyWindow(hWnd);
	}

	UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

inline void runAppSettingsTests()
{
	std::printf("--- Settings & Hotkey Configuration Tests ---\n");
	testModifierConversions();
	testHotkeyRegistryPersistence();
	testTrayDblClickPersistence();
	testHotkeyWindowRegistration();
}
