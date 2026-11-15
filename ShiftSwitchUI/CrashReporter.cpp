#include "pch.h"
#include "CrashReporter.h"

#include <shlwapi.h>
#include <ShlObj.h>
#include <DbgHelp.h>
#include "LogOutput.h"

#pragma comment(lib, "dbghelp.lib")

CrashReporter g_CrashReporter;

void CrashReporter::Initialize(const wchar_t* version)
{
	if (m_initialized)
		return;

	m_version = version != nullptr ? version : L"unknown";
	m_previousFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilter);
	m_initialized = true;
	LOG_INFO_UI(L"Crash reporter enabled, version %s, dumps in %s", m_version.c_str(), GetDumpDirectory().c_str());
}

void CrashReporter::Shutdown()
{
	if (!m_initialized)
		return;

	SetUnhandledExceptionFilter(m_previousFilter);
	m_previousFilter = nullptr;
	m_initialized = false;
}

LONG WINAPI CrashReporter::UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
	const std::wstring dumpPath = WriteDump(exceptionInfo, L"Unhandled exception");
	if (!dumpPath.empty())
	{
		LOG_ERROR_UI(L"Crash dump written: %s", dumpPath.c_str());
	}

	// Let Windows Error Reporting run its own handler as well.
	return EXCEPTION_CONTINUE_SEARCH;
}

std::wstring CrashReporter::GetDumpDirectory()
{
	WCHAR localAppData[MAX_PATH] = L"";
	const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return L"";

	std::wstring path(localAppData);
	path += L"\\ShiftSwitch\\Crashes";
	return path;
}

bool CrashReporter::EnsureDirectory(const std::wstring& path)
{
	if (path.empty())
		return false;
	if (PathFileExistsW(path.c_str()))
		return true;

	const int result = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
	return result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS;
}

std::wstring CrashReporter::BuildDumpPath()
{
	const std::wstring directory = GetDumpDirectory();
	if (directory.empty())
		return L"";

	WCHAR moduleName[MAX_PATH] = L"";
	if (GetModuleFileNameW(nullptr, moduleName, MAX_PATH) == 0)
		return L"";
	LPCWSTR fileName = PathFindFileNameW(moduleName);
	std::wstring baseName(fileName);
	const size_t dot = baseName.rfind(L'.');
	if (dot != std::wstring::npos)
		baseName.resize(dot);

	SYSTEMTIME time = {};
	GetLocalTime(&time);

	WCHAR name[MAX_PATH] = L"";
	swprintf_s(name, L"%s_%04u%02u%02u_%02u%02u%02u_%u.dmp",
		baseName.c_str(),
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond,
		GetCurrentProcessId());

	std::wstring path(directory);
	path += L"\\";
	path += name;
	return path;
}

std::wstring CrashReporter::WriteDump(EXCEPTION_POINTERS* exceptionInfo, const wchar_t* reason)
{
	const std::wstring directory = GetDumpDirectory();
	if (!EnsureDirectory(directory))
	{
		LOG_ERROR_UI(L"Crash dump directory is not available: %s", directory.c_str());
		return L"";
	}

	const std::wstring dumpPath = BuildDumpPath();
	if (dumpPath.empty())
	{
		LOG_ERROR_UI(L"Crash dump path could not be built");
		return L"";
	}

	const HANDLE file = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
	{
		LOG_ERROR_UI(L"Crash dump file is not writable: %s, error=%u", dumpPath.c_str(), GetLastError());
		return L"";
	}

	MINIDUMP_EXCEPTION_INFORMATION exception = {};
	exception.ThreadId = GetCurrentThreadId();
	exception.ExceptionPointers = exceptionInfo;
	exception.ClientPointers = FALSE;

	const BOOL written = MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		file,
		static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory),
		exceptionInfo != nullptr ? &exception : nullptr,
		nullptr,
		nullptr);
	CloseHandle(file);

	if (!written)
	{
		LOG_ERROR_UI(L"MiniDumpWriteDump failed for %s, error=%u", reason, GetLastError());
		DeleteFileW(dumpPath.c_str());
		return L"";
	}

	return dumpPath;
}
