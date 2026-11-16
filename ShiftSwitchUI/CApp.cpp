#include "pch.h"
#include "CApp.h"

#include "CProcessChild.h"
#include "CProcessMain.h"
#include "IpcMessages.h"
#include "resource.h"

#include <commctrl.h>
#include <cwchar>


CApp& CApp::getInstance()
{
	static CApp singleton{};
	return singleton;
}

std::wstring CApp::getMutexName(bool isChild) const
{
	return isChild ? SHIFTSWITCH_CHILD_MUTEX_NAME : SHIFTSWITCH_MAIN_MUTEX_NAME;
}

int CApp::start(HINSTANCE hInstance, LPWSTR lpCmdLine)
{
	m_hInstance = hInstance;
	m_lpCmdLine = lpCmdLine;

	INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_HOTKEY_CLASS | ICC_LINK_CLASS };
	InitCommonControlsEx(&icex);

	bool isChild = false;
	if (lpCmdLine != nullptr && wcslen(lpCmdLine) > 0)
	{
		const auto parentHwnd = reinterpret_cast<HWND>(std::wcstoull(lpCmdLine, nullptr, 10));
		if (parentHwnd != nullptr && ::IsWindow(parentHwnd))
		{
			WCHAR className[64] = L"";
			const auto expectedClassName = getResourceString(IDC_SHIFTSWITCHUI);
			if (::GetClassNameW(parentHwnd, className, static_cast<int>(ARRAYSIZE(className))) > 0
				&& expectedClassName != nullptr && wcscmp(className, expectedClassName) == 0)
			{
				isChild = true;
				m_hwndParentProcess = parentHwnd;
			}
			else
			{
				// Passed window handle is not a ShiftSwitch main window
				return 1;
			}
		}
		else
		{
			// Passed argument is not a valid window handle
			return 1;
		}
	}

	const auto mutexName = getMutexName(isChild);
	m_mutex = std::make_unique<CMutex>(nullptr, false, mutexName.c_str());
	if (!m_mutex->isValid() || ::GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return 1;
	}

	if (isChild)
	{
		m_process = std::make_unique<CProcessChild>();
	}
	else
	{
		m_process = std::make_unique<CProcessMain>();
	}

	return m_process->start();
}

LPCTSTR CApp::getResourceString(const UINT uID, UINT* lpuSize) const noexcept
{
	LPTSTR buff = nullptr;
	const auto len = LoadString(m_hInstance, uID, LPTSTR(&buff), 0);
	if (lpuSize != nullptr)
	{
		*lpuSize = len - 1;
	}
	return buff;
}

HINSTANCE CApp::handle() const
{
	return m_hInstance;
}

CProcessBase* CApp::getProcess() const
{
	return m_process.get();
}

HWND CApp::getParentHWND() const
{
	return m_hwndParentProcess;
}

CLog& CApp::getLog()
{
	return m_log;
}

CApp::CApp()
{
}
