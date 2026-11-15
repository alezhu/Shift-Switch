#pragma once
#include <windows.h>

namespace ProcessUtils
{
	bool isWow64Process(HANDLE hProcess);
	bool is64bitProcess(HANDLE hProcess);
	bool is32bitProcess(HANDLE hProcess);
	bool is32BitSystem();
	bool is64BitSystem();
	bool isAppContainerProcess(DWORD processId);
}
