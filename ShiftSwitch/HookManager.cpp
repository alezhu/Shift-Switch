#include "pch.h"
#include "HookManager.h"
#include "AsyncLogger.h"
#include "ProcessExclusionCache.h"
#include "LayoutSwitcher.h"
#include "KeyProcessor.h"
#include "TsfSync.h"
#include "ProcessUtils.h"
#include "IpcMessages.h"
#include "ScopedResource.h"
#include "ScopeExit.h"
#include "CHandle.h"
#include <sstream>

HMODULE g_hModule = nullptr;
HKL hklEnglish = nullptr;
HKL hklRussian = nullptr;
DWORD dwCurrentProcess = 0;
DWORD dwHookProcess = 0;
ScopedHook hook_WH_KEYBOARD;
ScopedHook hook_WH_GETMESSAGE;
ScopedHook hook_WH_KEYBOARD_LL;
CHandle hLowLevelHookMutex;
UINT WMAPP_SWITCH_LAYOUT = 0;
UINT WMAPP_SYNC_PROFILE = 0;
std::atomic<bool> bSkipProcess{false};

static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_perProcessInitialized{false};
static ShiftTap::State g_hookState;
static ShiftTap::State g_lowLevelState;

void EnsurePerProcessInit()
{
	if (g_perProcessInitialized.exchange(true))
		return;

	WMAPP_SWITCH_LAYOUT = RegisterWindowMessageW(MSG_SWITCH_LAYOUT_NAME);
	WMAPP_SYNC_PROFILE = RegisterWindowMessageW(MSG_SYNC_PROFILE_NAME);

	const auto hUser32 = GetModuleHandleW(L"User32");
	if (hUser32 != nullptr)
	{
		typedef BOOL(WINAPI* FN_ChangeWindowMessageFilter)(UINT, DWORD);
		const auto lpfn_ChangeWindowMessageFilter = reinterpret_cast<FN_ChangeWindowMessageFilter>(
			GetProcAddress(hUser32, "ChangeWindowMessageFilter"));
		if (lpfn_ChangeWindowMessageFilter != nullptr)
		{
			lpfn_ChangeWindowMessageFilter(WMAPP_SWITCH_LAYOUT, MSGFLT_ADD);
			lpfn_ChangeWindowMessageFilter(WMAPP_SYNC_PROFILE, MSGFLT_ADD);
		}
	}

	dwCurrentProcess = GetCurrentProcessId();

	wchar_t dllPath[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, dllPath, MAX_PATH);
	LogInit(dllPath);
}

bool InitializeOnce()
{
	if (g_initialized.exchange(true))
		return true;

	WCHAR klidLeft[KL_NAMELENGTH] = L"00000409";
	WCHAR klidRight[KL_NAMELENGTH] = L"00000419";
	ScopedHKey hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ShiftSwitch", 0, KEY_READ, hKey.addressof()) == ERROR_SUCCESS)
	{
		DWORD size = sizeof(klidLeft);
		RegQueryValueExW(hKey, L"LayoutLeft", nullptr, nullptr, reinterpret_cast<LPBYTE>(klidLeft), &size);
		size = sizeof(klidRight);
		RegQueryValueExW(hKey, L"LayoutRight", nullptr, nullptr, reinterpret_cast<LPBYTE>(klidRight), &size);
	}

	hklEnglish = LoadKeyboardLayout(klidLeft, 0);
	hklRussian = LoadKeyboardLayout(klidRight, 0);

	ProcessExclusionCache::Instance().Refresh();
	LOG_INFO(L"Loaded %zu excluded processes", ProcessExclusionCache::Instance().GetExcludedCount());

	dwCurrentProcess = GetCurrentProcessId();

	WMAPP_SWITCH_LAYOUT = RegisterWindowMessageW(MSG_SWITCH_LAYOUT_NAME);
	WMAPP_SYNC_PROFILE = RegisterWindowMessageW(MSG_SYNC_PROFILE_NAME);
	const auto hUser32 = GetModuleHandleW(L"User32");
	if (hUser32 != nullptr)
	{
		typedef BOOL(WINAPI* FN_ChangeWindowMessageFilter)(UINT, DWORD);
		const auto lpfn_ChangeWindowMessageFilter = reinterpret_cast<FN_ChangeWindowMessageFilter>(
			GetProcAddress(hUser32, "ChangeWindowMessageFilter"));
		if (lpfn_ChangeWindowMessageFilter != nullptr)
		{
			lpfn_ChangeWindowMessageFilter(WMAPP_SWITCH_LAYOUT, MSGFLT_ADD);
		}
	}

#ifdef _DEBUG
	wchar_t szPath[MAX_PATH] = L"";
	szPath[GetModuleFileNameW(nullptr, szPath, MAX_PATH)] = 0;
	std::wstringstream stream;
	stream << L"Attach: " << szPath;
	SendEvent(stream.str());
#endif

	return true;
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	EnsurePerProcessInit();
	if (code >= 0 && !bSkipProcess)
	{
		HOOK_CONTEXT ctx = {};
		ctx.inputHWND = ::GetForegroundWindow();
		ctx.inputThreadId = GetWindowThreadProcessId(ctx.inputHWND, &ctx.inputProcessId);
		checkIs32BitWindow(ctx);

		if (IsProcessExcluded(ctx.inputProcessId))
		{
			LOG_DEBUG(L"KeyboardProc: skipped excluded process PID=%u", ctx.inputProcessId);
			return CallNextHookEx(nullptr, code, wParam, lParam);
		}

#ifdef _WIN64
		if (!ctx.inputIs32bitProcess) {
#else
		if (ctx.inputIs32bitProcess) {
#endif
			const WORD whlParam = HIWORD(lParam);
			const bool down = (whlParam & KF_UP) == 0;
			const UINT scanCode = whlParam & 0x1FF;
			const auto vkCode = MapVirtualKey(scanCode, MAPVK_VSC_TO_VK_EX);

			ProcessKeyEvent(g_hookState, vkCode, down, LOWORD(lParam), ctx);
		}
		else
		{
			LOG_DEBUG(L"KeyboardProc: Skip for bits");
		}
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	EnsurePerProcessInit();
	if (code == HC_ACTION && !bSkipProcess)
	{
		const auto* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
		const bool isShift = kb->vkCode == VK_LSHIFT || kb->vkCode == VK_RSHIFT;

		if (isShift || g_lowLevelState.leftShiftDown || g_lowLevelState.rightShiftDown)
		{
			HOOK_CONTEXT ctx = {};
			ctx.lowLevel = true;
			ctx.inputHWND = ::GetForegroundWindow();
			ctx.inputThreadId = GetWindowThreadProcessId(ctx.inputHWND, &ctx.inputProcessId);

			if (IsProcessExcluded(ctx.inputProcessId))
			{
				LOG_DEBUG(L"LowLevelKeyboardProc: skipped excluded process PID=%u", ctx.inputProcessId);
				return CallNextHookEx(nullptr, code, wParam, lParam);
			}

			if (ProcessUtils::isAppContainerProcess(ctx.inputProcessId))
			{
				const bool down = (kb->flags & LLKHF_UP) == 0;
				ProcessKeyEvent(g_lowLevelState, kb->vkCode, down, 1, ctx);
			}
			else
			{
				g_lowLevelState.reset();
			}
		}
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam)
{
	EnsurePerProcessInit();
	if (code >= 0 && !bSkipProcess)
	{
		const MSG* lpMsg = reinterpret_cast<const MSG*>(lParam);
		if (lpMsg->message == WMAPP_SWITCH_LAYOUT)
		{
			DWORD targetProcessId = 0;
			GetWindowThreadProcessId(lpMsg->hwnd, &targetProcessId);
			if (targetProcessId == GetCurrentProcessId())
			{
				const auto hkl = reinterpret_cast<HKL>(lpMsg->wParam);
				ActivateLayoutEx(hkl, lpMsg->hwnd);
				LogSwitch(L"Process WMAPP_SWITCH_LAYOUT");
			}
		}
		else if (lpMsg->message == WMAPP_SYNC_PROFILE)
		{
			DWORD targetProcessId = 0;
			GetWindowThreadProcessId(lpMsg->hwnd, &targetProcessId);
			if (targetProcessId == GetCurrentProcessId())
			{
				const auto hkl = reinterpret_cast<HKL>(lpMsg->wParam);
				TsfSync::SyncTsfProfile(hkl);
				LogSwitch(L"Sync TSF profile");
			}
		}
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}

void SetLowLevelHook(const bool bSet)
{
	if (bSet)
	{
		if (hook_WH_KEYBOARD_LL)
			return;
		if (!hLowLevelHookMutex.isValid())
		{
			hLowLevelHookMutex.reset(CreateMutexW(nullptr, TRUE, L"Local\\ShiftSwitch_LowLevelHook"));
			if (!hLowLevelHookMutex.isValid())
			{
				LOG_ERROR(L"CreateMutex failed for LowLevelHook, error=%u", GetLastError());
				return;
			}
			if (GetLastError() == ERROR_ALREADY_EXISTS)
			{
				LOG_INFO(L"LowLevelHook already owned by another process");
				hLowLevelHookMutex.close();
				return;
			}
		}
		hook_WH_KEYBOARD_LL.reset(SetWindowsHookEx(
			WH_KEYBOARD_LL,
			LowLevelKeyboardProc,
			g_hModule,
			0
		));
		if (!hook_WH_KEYBOARD_LL && hLowLevelHookMutex.isValid())
		{
			LOG_ERROR(L"SetWindowsHookEx WH_KEYBOARD_LL failed, error=%u", GetLastError());
			ReleaseMutex(hLowLevelHookMutex);
			hLowLevelHookMutex.close();
		}
		else
		{
			LOG_INFO(L"LowLevelHook installed");
		}
	}
	else
	{
		hook_WH_KEYBOARD_LL.reset();
		if (hLowLevelHookMutex.isValid())
		{
			ReleaseMutex(hLowLevelHookMutex);
			hLowLevelHookMutex.close();
		}
		LOG_INFO(L"LowLevelHook removed");
	}
}

BOOL APIENTRY SetHook(BOOL bSet)
{
	if (bSet) {
		if (!InitializeOnce()) {
			LOG_ERROR(L"InitializeOnce failed");
			return false;
		}

		LogThreadStart();
		ScopeExit logThreadGuard([] { LogThreadStop(); });

		LOG_INFO(L"SetHook: enabling hooks");
		dwHookProcess = GetCurrentProcessId();

		hook_WH_KEYBOARD.reset(SetWindowsHookEx(
			WH_KEYBOARD,
			KeyboardProc,
			g_hModule,
			0
		));

		if (!hook_WH_KEYBOARD) {
			LOG_ERROR(L"Hook WH_KEYBOARD not set, error=%u", GetLastError());
			return false;
		}
		ScopeExit keyboardHookGuard([] { hook_WH_KEYBOARD.reset(); });

		hook_WH_GETMESSAGE.reset(SetWindowsHookEx(
			WH_GETMESSAGE,
			GetMsgProc,
			g_hModule,
			0
		));
		if (!hook_WH_GETMESSAGE) {
			LOG_ERROR(L"Hook WH_GETMESSAGE not set, error=%u", GetLastError());
			return false;
		}

		SetLowLevelHook(true);

		keyboardHookGuard.dismiss();
		logThreadGuard.dismiss();
	}
	else
	{
		LOG_INFO(L"SetHook: disabling hooks");
		hook_WH_GETMESSAGE.reset();
		hook_WH_KEYBOARD.reset();
		SetLowLevelHook(false);
		LogThreadStop();
	}

	return true;
}
