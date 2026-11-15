#include "pch.h"
#include "ProcessUtils.h"
#include "CHandle.h"

namespace ProcessUtils
{
namespace
{
	typedef BOOL(WINAPI* FN_IsWow64Process)(HANDLE hProcess, PBOOL Wow64Process);
	FN_IsWow64Process lpfn_IsWow64Process = nullptr;
	bool bIsWow64Loaded = false;
	HMODULE g_hKernel = nullptr;

	HMODULE getKernel32Handle()
	{
		if (g_hKernel == nullptr)
		{
			g_hKernel = GetModuleHandleW(L"kernel32.dll");
		}
		return g_hKernel;
	}

	int iSystemBit = 0;

	void detectSystemBit()
	{
		if (iSystemBit != 0) return;
		SYSTEM_INFO sysInfo = {};

		const auto hKernel = getKernel32Handle();
		if (hKernel != nullptr) {
			const auto lpFN_GetNativeSystemInfo = reinterpret_cast<VOID(WINAPI*)(LPSYSTEM_INFO)>(
				GetProcAddress(hKernel, "GetNativeSystemInfo"));
			if (lpFN_GetNativeSystemInfo != nullptr) {
				lpFN_GetNativeSystemInfo(&sysInfo);
				goto done;
			}
		}
		GetSystemInfo(&sysInfo);

	done:
		iSystemBit = sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL
			? 32
			: sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
			? 64
			: -1;
	}
}

bool isWow64Process(HANDLE hProcess)
{
	if (!bIsWow64Loaded)
	{
		const auto hKernel = getKernel32Handle();
		if (hKernel != nullptr) {
			lpfn_IsWow64Process = reinterpret_cast<FN_IsWow64Process>(GetProcAddress(hKernel, "IsWow64Process"));
		}
		bIsWow64Loaded = true;
	}
	if (lpfn_IsWow64Process != nullptr)
	{
		BOOL bResult = FALSE;
		if (lpfn_IsWow64Process(hProcess, &bResult))
		{
			return bResult != FALSE;
		}
	}
	return false;
}

bool is32BitSystem()
{
	detectSystemBit();
	return iSystemBit == 32;
}

bool is64BitSystem()
{
	detectSystemBit();
	return iSystemBit == 64;
}

bool is64bitProcess(HANDLE hProcess)
{
	return !isWow64Process(hProcess) && is64BitSystem();
}

bool is32bitProcess(HANDLE hProcess)
{
	return isWow64Process(hProcess) || is32BitSystem();
}

bool isAppContainerProcess(const DWORD processId)
{
	if (processId == 0)
		return false;
	const CHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
	if (!process.isValid())
		return false;
	HANDLE rawToken = nullptr;
	if (!OpenProcessToken(process.get(), TOKEN_QUERY, &rawToken))
		return false;
	const CHandle token(rawToken);
	DWORD value = 0;
	DWORD size = sizeof(value);
	// TokenIsAppContainer = 29; the enum value is not available in old SDKs.
	const BOOL result = GetTokenInformation(token.get(),
		static_cast<TOKEN_INFORMATION_CLASS>(29), &value, size, &size);
	return result != FALSE && value != 0;
}
}
