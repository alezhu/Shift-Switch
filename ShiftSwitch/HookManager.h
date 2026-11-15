#pragma once
#include <windows.h>
#include <atomic>
#include "ShiftTap.h"

extern HMODULE g_hModule;
extern HKL hklEnglish;
extern HKL hklRussian;
extern DWORD dwCurrentProcess;
extern DWORD dwHookProcess;
extern UINT WMAPP_SWITCH_LAYOUT;
extern UINT WMAPP_SYNC_PROFILE;
extern std::atomic<bool> bSkipProcess;

void EnsurePerProcessInit();
bool InitializeOnce();
BOOL APIENTRY SetHook(BOOL bSet);
void SetLowLevelHook(bool bSet);

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam);
