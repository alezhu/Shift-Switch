#pragma once
#include <memory>
#include <string>

#include "CLog.h"
#include "CMutex.h"
#include "CProcessBase.h"

class CApp
{
public:
	static CApp& getInstance();
	std::wstring getMutexName() const;
	int start(HINSTANCE hInstance, LPWSTR lpCmdLine);

	LPCTSTR getResourceString(
		const UINT uID,
		UINT* lpuSize = nullptr
	) const noexcept;
	HINSTANCE handle() const;
	CProcessBase* getProcess() const;
	HWND getParentHWND() const;
	CLog& getLog();
protected:
	CLog m_log;
	HINSTANCE m_hInstance = nullptr;
	LPWSTR m_lpCmdLine = nullptr;
	std::unique_ptr<CProcessBase> m_process;
	std::unique_ptr<CMutex> m_mutex;
	HWND m_hwndParentProcess = nullptr;

	CApp();
};
