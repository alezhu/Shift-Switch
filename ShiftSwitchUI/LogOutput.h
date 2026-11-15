#pragma once

#include <windows.h>
#include <stdarg.h>

// Logging shared by the UI processes. Messages go to the debugger (DebugView,
// Visual Studio Output window) in every build, the same way the hook DLL does.
inline void LogOutputUI(const wchar_t* format, ...)
{
	wchar_t buffer[512];
	va_list args;
	va_start(args, format);
	vswprintf_s(buffer, format, args);
	va_end(args);
	OutputDebugStringW(buffer);
	OutputDebugStringW(L"\n");
}

#define LOG_ERROR_UI(fmt, ...) LogOutputUI(L"[ERROR] " fmt, __VA_ARGS__)
#define LOG_WARN_UI(fmt, ...)  LogOutputUI(L"[WARN] " fmt, __VA_ARGS__)
#define LOG_INFO_UI(fmt, ...)  LogOutputUI(L"[INFO] " fmt, __VA_ARGS__)
