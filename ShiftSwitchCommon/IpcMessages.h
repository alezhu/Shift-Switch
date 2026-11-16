#pragma once
#include <windows.h>

// UI Process Inter-Process Communication (IPC) Window Messages
// Used between ShiftSwitchUI (main) and ShiftSwitchUI64 (child helper)
constexpr UINT WMAPP_NOTIFYCALLBACK         = WM_APP + 1;
constexpr UINT WMAPP_SET_CHILD_PROCESS_HWND = WM_APP + 2;
constexpr UINT WMAPP_ACTIVATE               = WM_APP + 3;
constexpr UINT WMAPP_DEACTIVATE             = WM_APP + 4;
constexpr UINT WMAPP_CHILD_CLOSE            = WM_APP + 5;
constexpr UINT WMAPP_QUERY_STATE            = WM_APP + 6;

// Explicit result codes returned by IPC window message processing (LRESULT)
constexpr LRESULT STATE_RESULT_OK           = 1;
constexpr LRESULT STATE_RESULT_ERROR        = 0;

// Registered window message names for Hook DLL layout switching & TSF profile sync
inline constexpr const wchar_t* MSG_SWITCH_LAYOUT_NAME = L"WMAPP_SWITCH_LAYOUT";
inline constexpr const wchar_t* MSG_SYNC_PROFILE_NAME  = L"WMAPP_SYNC_PROFILE";

// Single-instance mutex names (per-session)
inline constexpr const wchar_t* SHIFTSWITCH_MAIN_MUTEX_NAME  = L"Local\\ShiftSwitch_MainProcess";
inline constexpr const wchar_t* SHIFTSWITCH_CHILD_MUTEX_NAME = L"Local\\ShiftSwitch_ChildProcess";
