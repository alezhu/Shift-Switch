#pragma once
#include <windows.h>
#include <atomic>

struct HOOK_CONTEXT {
	HWND inputHWND;
	DWORD inputThreadId;
	DWORD inputProcessId;
	bool inputIs32bitProcess;
	bool lowLevel;
};

// RAII guard to prevent re-entrancy during layout switching.
// Only protects the ActivateKeyboardLayout call itself, not the follow-up messages.
class LayoutSwitchGuard
{
public:
	LayoutSwitchGuard();
	~LayoutSwitchGuard();
};

void ActivateLayoutEx(HKL hkl, HWND hwnd);
void ActivateLayout(HKL hkl, const HOOK_CONTEXT& ctx);
HWND getInputWindow(const HOOK_CONTEXT& ctx);
bool checkIsSomeModifKeyDownExcept(BYTE except);
void checkIs32BitWindow(HOOK_CONTEXT& ctx);
