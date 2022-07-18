#include "pch.h"
#include "CLog.h"

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
}

void CLog::addMsgToListBox(HWND hWND, LPCWSTR message)
{
	if (::IsWindow(hWND)) {
		::SendMessageW(hWND, LB_ADDSTRING, 0, LPARAM(message));

		::SendMessageW(hWND, LB_SETCURSEL, ::SendMessageW(hWND, LB_GETCOUNT, 0, 0) - 1, 0);
	}
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

