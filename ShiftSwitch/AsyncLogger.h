#pragma once
#include <windows.h>
#include <string>
#include <string_view>

void LogOutput(const wchar_t* format, ...);

#ifdef _DEBUG
#define LOG_DEBUG(fmt, ...) LogOutput(fmt, __VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#define LOG_ERROR(fmt, ...) LogOutput(L"[ERROR] " fmt, __VA_ARGS__)
#define LOG_WARN(fmt, ...)  LogOutput(L"[WARN] " fmt, __VA_ARGS__)
#define LOG_INFO(fmt, ...)  LogOutput(L"[INFO] " fmt, __VA_ARGS__)

#define LogSwitch(ws) LogSwitchAsync(ws)

void LogInit(const wchar_t* dllPath);
void LogSwitchAsync(const wchar_t* wsSwitch);
void LogThreadStart();
void LogThreadStop();

#ifdef _DEBUG
void SendEvent(const std::wstring_view& str);
std::wstring GetKeyName(DWORD vkCode);
void LogEvent(DWORD keyCode, bool down, WORD iRepeatCount);
#endif
