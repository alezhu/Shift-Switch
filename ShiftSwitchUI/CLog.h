#pragma once
#include <string>
#include <vector>


class CLog
{
public:
	void add(LPCWSTR message);
	void attachToWindow(HWND hWND);
protected:
	void addMsgToListBox(HWND hWND, LPCWSTR message);
	std::vector<std::wstring> m_tempLog;
	HWND m_Hwnd = nullptr;
	std::wstring m_tempFile;
public:

	CLog();
};
