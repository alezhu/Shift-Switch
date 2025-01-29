#include "pch.h"
#include "CLog.h"
#include  <fstream>
#include <iostream>
#include <mutex>

std::mutex g_log_mutex;

void CLog::add(LPCWSTR message)
{
	if (message == nullptr) return;
	if (m_Hwnd != nullptr)
	{
		addMsgToListBox(m_Hwnd, message);
	}
	else
	{
		m_tempLog.emplace_back(std::wstring{ message });
	}
	std::lock_guard<std::mutex> guard(g_log_mutex);
	std::wofstream log(m_tempFile, std::ios::out | std::ios::app);
	log << message << std::endl;
	log.close();

}

void CLog::addMsgToListBox(HWND hWND, LPCWSTR message)
{
	if (::IsWindow(hWND)) {
		::SendMessageW(hWND, LB_ADDSTRING, 0, LPARAM(message));

		::SendMessageW(hWND, LB_SETCURSEL, ::SendMessageW(hWND, LB_GETCOUNT, 0, 0) - 1, 0);
	}
}

CLog::CLog()
{
	wchar_t path[MAX_PATH];
	wchar_t fullpath[MAX_PATH];
	auto pathLength = GetTempPath(MAX_PATH, path);
#ifdef 	_WIN64
	GetTempFileNameW(path, L"S64", 0, fullpath);
#else
	GetTempFileNameW(path, L"S32", 0, fullpath);
#endif
	m_tempFile.assign(fullpath);
}

void CLog::attachToWindow(HWND hWND)
{
	m_Hwnd = hWND;
	if (m_Hwnd == nullptr || !::IsWindow(hWND)) return;

	for (const auto& value : m_tempLog)
	{
		addMsgToListBox(hWND, value.c_str());
	}
	m_tempLog.clear();
}

