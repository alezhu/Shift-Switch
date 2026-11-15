#include "pch.h"
#include "HookController.h"
#include <shlwapi.h>
#include "Exclusions.h"
#include "LogOutput.h"
#include "ScopeExit.h"

HookController::HookController(std::wstring customDllPath)
	: m_customDllPath(std::move(customDllPath))
{
}

HookController::~HookController()
{
	Shutdown();
}

HookController::HookController(HookController&& other) noexcept
	: m_customDllPath(std::move(other.m_customDllPath)),
	  m_hLib(other.m_hLib),
	  m_pfnSETHOOK(other.m_pfnSETHOOK),
	  m_bActivated(other.m_bActivated)
{
	other.m_hLib = nullptr;
	other.m_pfnSETHOOK = nullptr;
	other.m_bActivated = false;
}

HookController& HookController::operator=(HookController&& other) noexcept
{
	if (this != &other)
	{
		Shutdown();
		m_customDllPath = std::move(other.m_customDllPath);
		m_hLib = other.m_hLib;
		m_pfnSETHOOK = other.m_pfnSETHOOK;
		m_bActivated = other.m_bActivated;
		other.m_hLib = nullptr;
		other.m_pfnSETHOOK = nullptr;
		other.m_bActivated = false;
	}
	return *this;
}

std::wstring HookController::GetDefaultHookDllPath()
{
	WCHAR exePath[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	PathRemoveFileSpecW(exePath);

#ifdef _WIN64
	wcscat_s(exePath, L"\\ShiftSwitch64.dll");
#else
	wcscat_s(exePath, L"\\ShiftSwitch.dll");
#endif
	return std::wstring(exePath);
}

bool HookController::Enable()
{
	if (m_bActivated)
	{
		return true;
	}

	const std::wstring dllPath = m_customDllPath.empty() ? GetDefaultHookDllPath() : m_customDllPath;
	LOG_INFO_UI(L"Loading hook DLL: %s", dllPath.c_str());

	// The exclusion list lives in this process; publish the current snapshot
	// before the hook starts, so every injected copy of the DLL can read it.
	PublishExclusions(LoadExclusionsFromRegistry());

	m_hLib = LoadLibraryW(dllPath.c_str());
	if (m_hLib == nullptr)
	{
		LOG_ERROR_UI(L"Hook DLL not loaded: %s, error=%u", dllPath.c_str(), GetLastError());
		return false;
	}

	ScopeExit rollback([this] {
		if (m_hLib != nullptr)
		{
			FreeLibrary(m_hLib);
			m_hLib = nullptr;
		}
		m_pfnSETHOOK = nullptr;
		m_bActivated = false;
	});

	m_pfnSETHOOK = reinterpret_cast<SETHOOK>(GetProcAddress(m_hLib, "SetHook"));
	if (m_pfnSETHOOK == nullptr)
	{
		LOG_ERROR_UI(L"HookProc SetHook not found, error=%u", GetLastError());
		return false;
	}

	m_bActivated = (m_pfnSETHOOK(TRUE) != FALSE);
	if (!m_bActivated)
	{
		LOG_ERROR_UI(L"Hook SetHook(TRUE) failed");
		return false;
	}

	rollback.dismiss();
	LOG_INFO_UI(L"Hook activated successfully");
	return true;
}

bool HookController::Disable()
{
	if (!m_bActivated)
	{
		return true;
	}

	if (m_pfnSETHOOK != nullptr)
	{
		if (!m_pfnSETHOOK(FALSE))
		{
			LOG_ERROR_UI(L"Hook SetHook(FALSE) failed");
			return false;
		}
	}

	if (m_hLib != nullptr)
	{
		FreeLibrary(m_hLib);
		m_hLib = nullptr;
	}

	m_pfnSETHOOK = nullptr;
	m_bActivated = false;

	LOG_INFO_UI(L"Hook deactivated successfully");
	return true;
}

void HookController::Shutdown()
{
	Disable();
}
