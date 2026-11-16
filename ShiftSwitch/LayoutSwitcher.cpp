#include "pch.h"
#include "LayoutSwitcher.h"
#include "HookManager.h"
#include "AsyncLogger.h"
#include "ProcessUtils.h"
#include "CHandle.h"
#include "TsfSync.h"
#include <memory>

LayoutSwitchGuard::LayoutSwitchGuard()
{
	bSkipProcess.store(true, std::memory_order_relaxed);
}

LayoutSwitchGuard::~LayoutSwitchGuard()
{
	bSkipProcess.store(false, std::memory_order_relaxed);
}

struct SWITCH_LAYOUT_TASK {
	HKL hkl;
	HWND hwnd;
};

static DWORD WINAPI SwitchLayoutWorkItem(LPVOID param) {
	std::unique_ptr<SWITCH_LAYOUT_TASK> task(static_cast<SWITCH_LAYOUT_TASK*>(param));
	if (task == nullptr) return 0;
	LOG_DEBUG(L"SwitchLayoutWorkItem: thread=%u, hwnd=%p, hkl=%p",
		GetCurrentThreadId(), task->hwnd, task->hkl);
	{
		LayoutSwitchGuard guard;
		ActivateKeyboardLayout(task->hkl, KLF_SETFORPROCESS);
	}
	PostMessageW(task->hwnd, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_SYSCHARSET, reinterpret_cast<LPARAM>(task->hkl));
	PostMessageW(task->hwnd, WMAPP_SYNC_PROFILE, reinterpret_cast<WPARAM>(task->hkl), 0);
	return 0;
}

void ActivateLayoutEx(HKL hkl, HWND hwnd) {
	auto task = std::make_unique<SWITCH_LAYOUT_TASK>(SWITCH_LAYOUT_TASK{ hkl, hwnd });
	if (QueueUserWorkItem(SwitchLayoutWorkItem, task.get(), WT_EXECUTEDEFAULT)) {
		task.release();
	}
	else {
		LOG_ERROR(L"QueueUserWorkItem failed, error=%u", GetLastError());
		{
			LayoutSwitchGuard guard;
			ActivateKeyboardLayout(hkl, KLF_SETFORPROCESS);
		}
		PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_SYSCHARSET, reinterpret_cast<LPARAM>(hkl));
		PostMessageW(hwnd, WMAPP_SYNC_PROFILE, reinterpret_cast<WPARAM>(hkl), 0);
	}
}

HWND getInputWindow(const HOOK_CONTEXT& ctx) {
	GUITHREADINFO info = { sizeof(info) };
	if (GetGUIThreadInfo(ctx.inputThreadId, &info))
	{
		if (info.hwndFocus != nullptr)
			return info.hwndFocus;
		if (info.hwndActive != nullptr)
			return info.hwndActive;
	}
	return ctx.inputHWND;
}

void ActivateLayout(HKL hkl, const HOOK_CONTEXT& ctx) {
	const HWND target = getInputWindow(ctx);
	LOG_DEBUG(L"ActivateLayout: hkl=%p, lowLevel=%d, inputPid=%u, curPid=%u, target=%p, inputHwnd=%p",
		hkl, ctx.lowLevel, ctx.inputProcessId, dwCurrentProcess, target, ctx.inputHWND);

	// Asynchronously post standard WM_INPUTLANGCHANGEREQUEST to the target window (focused control or active window)
	PostMessageW(target, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_SYSCHARSET, reinterpret_cast<LPARAM>(hkl));
	if (ctx.inputHWND != nullptr && ctx.inputHWND != target)
	{
		PostMessageW(ctx.inputHWND, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_SYSCHARSET, reinterpret_cast<LPARAM>(hkl));
	}

	if (ctx.inputProcessId == dwCurrentProcess)
	{
		LogSwitch(L"ActivateKeyboardLayout");
		ActivateLayoutEx(hkl, target);
	}
	else
	{
		// Also post custom message to notify any injected hook DLL in the target process
		PostMessageW(target, WMAPP_SWITCH_LAYOUT, reinterpret_cast<WPARAM>(hkl), 0);
		if (ctx.inputHWND != nullptr && ctx.inputHWND != target)
		{
			PostMessageW(ctx.inputHWND, WMAPP_SWITCH_LAYOUT, reinterpret_cast<WPARAM>(hkl), 0);
		}
		LogSwitch(L"Post WM_INPUTLANGCHANGEREQUEST & WMAPP_SWITCH_LAYOUT");
	}

	TsfSync::SyncTsfProfile(hkl);
}

bool checkIsSomeModifKeyDownExcept(const BYTE except) {
	static const BYTE checks[] = {
		VK_LSHIFT, VK_RSHIFT,
		VK_LCONTROL, VK_RCONTROL,
		VK_LWIN, VK_RWIN,
		VK_LMENU, VK_RMENU,
	};
	BYTE kbdState[256];
	constexpr BYTE bDown = 0x80;
	if (GetKeyboardState(kbdState)) {
		for (const auto vkCode : checks)
		{
			if (vkCode != except) {
				if (kbdState[vkCode] == bDown)
					return true;
			}
		}
	}
	return false;
}

void checkIs32BitWindow(HOOK_CONTEXT& ctx)
{
	if (ctx.inputHWND == nullptr)
	{
		ctx.inputProcessId = GetCurrentProcessId();
		ctx.inputThreadId = GetCurrentThreadId();
	}
	const CHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, ctx.inputProcessId));
	if (process.isValid())
	{
		ctx.inputIs32bitProcess = ProcessUtils::is32bitProcess(process.get());
	}
}
