#include "pch.h"
#include "CApp.h"

#include "CProcessChild.h"
#include "CProcessMain.h"


CApp& CApp::getInstance()
{
	static CApp singleton{};
	return singleton;
}

std::wstring CApp::getMutexName() const
{
	WCHAR path[MAX_PATH] = L"";
	auto size = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
	CharUpperBuffW(path, size);
	auto lpStart = path;
	while (auto delim = wcsstr(lpStart, L"\\"))
	{
		*delim = L'_';
		lpStart = delim + 1;
	}
	return { path };
}

int CApp::start(HINSTANCE hInstance, LPWSTR lpCmdLine)
{
	m_hInstance = hInstance;
	m_lpCmdLine = lpCmdLine;

	auto mutexName = getMutexName();
	m_mutex = std::make_unique<CMutex>(nullptr, false, mutexName.c_str());
	if (!m_mutex->isValid() || ::GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return 1;
	}


	if (wcslen(lpCmdLine) > 0)
	{
		//May be Child process
		m_hwndParentProcess = HWND(_wtol(lpCmdLine));
		m_process = std::make_unique<CProcessChild>();
		//#ifdef _DEBUG
		//		{
		//			std::wstringstream stream;
		//			stream << L"Parent HWND: " << hwndParentProcess;
		//			log(stream.str().c_str());
		//		}
		//#endif
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
