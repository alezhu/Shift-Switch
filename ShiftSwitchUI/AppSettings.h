#pragma once
#include <windows.h>
#include <commctrl.h>
#include "ScopedResource.h"

constexpr int TOGGLE_HOTKEY_ID = 1;
constexpr const WCHAR* SHIFTSWITCH_REG_KEY = L"Software\\ShiftSwitch";

enum class TrayDblClickAction : DWORD
{
	ToggleActive = 0,
	Settings = 1,
	Exclusions = 2,
	About = 3
};

struct HotkeyConfig
{
	UINT vk = 'S';
	UINT modifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;
};

// Convert between msctls_hotkey32 (HOTKEYF_*) and RegisterHotKey (MOD_*)
inline UINT HotkeyModifiersToMod(BYTE hotkeyMods)
{
	UINT mod = 0;
	if (hotkeyMods & HOTKEYF_ALT)     mod |= MOD_ALT;
	if (hotkeyMods & HOTKEYF_CONTROL) mod |= MOD_CONTROL;
	if (hotkeyMods & HOTKEYF_SHIFT)   mod |= MOD_SHIFT;
	return mod;
}

inline BYTE ModToHotkeyModifiers(UINT mod)
{
	BYTE hotkeyMods = 0;
	if (mod & MOD_ALT)     hotkeyMods |= HOTKEYF_ALT;
	if (mod & MOD_CONTROL) hotkeyMods |= HOTKEYF_CONTROL;
	if (mod & MOD_SHIFT)   hotkeyMods |= HOTKEYF_SHIFT;
	return hotkeyMods;
}

inline bool LoadHotkeyConfig(HKEY hRootKey, const WCHAR* subKey, HotkeyConfig& config)
{
	// Default values
	config.vk = 'S';
	config.modifiers = MOD_CONTROL | MOD_SHIFT | MOD_ALT;

	ScopedHKey hKey;
	if (RegOpenKeyExW(hRootKey, subKey, 0, KEY_READ, hKey.addressof()) != ERROR_SUCCESS)
	{
		return false;
	}

	DWORD dwVK = 0;
	DWORD size = sizeof(dwVK);
	DWORD type = 0;
	if (RegQueryValueExW(hKey, L"HotkeyVK", nullptr, &type, reinterpret_cast<LPBYTE>(&dwVK), &size) == ERROR_SUCCESS && type == REG_DWORD)
	{
		config.vk = dwVK;
	}

	DWORD dwMod = 0;
	size = sizeof(dwMod);
	if (RegQueryValueExW(hKey, L"HotkeyModifiers", nullptr, &type, reinterpret_cast<LPBYTE>(&dwMod), &size) == ERROR_SUCCESS && type == REG_DWORD)
	{
		config.modifiers = dwMod;
	}

	return true;
}

inline bool SaveHotkeyConfig(HKEY hRootKey, const WCHAR* subKey, const HotkeyConfig& config)
{
	ScopedHKey hKey;
	if (RegCreateKeyExW(hRootKey, subKey, 0, nullptr, 0, KEY_WRITE, nullptr, hKey.addressof(), nullptr) != ERROR_SUCCESS)
	{
		return false;
	}

	DWORD dwVK = config.vk;
	DWORD dwMod = config.modifiers;
	RegSetValueExW(hKey, L"HotkeyVK", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVK), sizeof(dwVK));
	RegSetValueExW(hKey, L"HotkeyModifiers", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwMod), sizeof(dwMod));
	return true;
}

inline bool LoadTrayDblClickAction(HKEY hRootKey, const WCHAR* subKey, TrayDblClickAction& action)
{
	action = TrayDblClickAction::ToggleActive;

	ScopedHKey hKey;
	if (RegOpenKeyExW(hRootKey, subKey, 0, KEY_READ, hKey.addressof()) != ERROR_SUCCESS)
	{
		return false;
	}

	DWORD dwAction = 0;
	DWORD size = sizeof(dwAction);
	DWORD type = 0;
	if (RegQueryValueExW(hKey, L"TrayDblClickAction", nullptr, &type, reinterpret_cast<LPBYTE>(&dwAction), &size) == ERROR_SUCCESS && type == REG_DWORD)
	{
		if (dwAction <= static_cast<DWORD>(TrayDblClickAction::About))
		{
			action = static_cast<TrayDblClickAction>(dwAction);
		}
	}

	return true;
}

inline bool SaveTrayDblClickAction(HKEY hRootKey, const WCHAR* subKey, TrayDblClickAction action)
{
	ScopedHKey hKey;
	if (RegCreateKeyExW(hRootKey, subKey, 0, nullptr, 0, KEY_WRITE, nullptr, hKey.addressof(), nullptr) != ERROR_SUCCESS)
	{
		return false;
	}

	DWORD dwAction = static_cast<DWORD>(action);
	RegSetValueExW(hKey, L"TrayDblClickAction", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwAction), sizeof(dwAction));
	return true;
}
