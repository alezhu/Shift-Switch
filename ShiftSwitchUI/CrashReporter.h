#pragma once

#include <windows.h>
#include <string>

// Writes a minidump when the application hits an unhandled exception.
//
// The dump is stored under %LOCALAPPDATA%\ShiftSwitch\Crashes and its path is
// written to the debug log. The dump is not uploaded anywhere: reporting to a
// server or to GitHub needs credentials that this project does not have.
class CrashReporter
{
public:
	// Installs the unhandled exception filter. `version` is only logged.
	void Initialize(const wchar_t* version);
	void Shutdown();

	// Writes a dump for the given exception and returns its full path, or an
	// empty string when the dump could not be written.
	static std::wstring WriteDump(EXCEPTION_POINTERS* exceptionInfo, const wchar_t* reason);

private:
	static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
	static std::wstring GetDumpDirectory();
	static bool EnsureDirectory(const std::wstring& path);
	static std::wstring BuildDumpPath();

	std::wstring m_version;
	LPTOP_LEVEL_EXCEPTION_FILTER m_previousFilter = nullptr;
	bool m_initialized = false;
};

extern CrashReporter g_CrashReporter;
