#include "pch.h"
#include "CProcessMain.h"

#include <commctrl.h>
#include <shellapi.h>
#include <sstream>

#include "CApp.h"
#include "resource.h"

constexpr auto TRAY_ICON = 666;

bool CProcessMain::deleteTrayIcon()
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = m_HWND;
	nid.uID = TRAY_ICON;
	return Shell_NotifyIcon(NIM_DELETE, &nid);
}

bool CProcessMain::createTrayIcon()
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = m_HWND;
	// add the icon, setting the icon, tooltip, and callback message.
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
	nid.uID = TRAY_ICON;
	nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
	auto hInst = CApp::getInstance().handle();
	nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APP));
	LoadStringW(hInst, IDS_APP_TITLE, nid.szTip, ARRAYSIZE(nid.szTip));

	Shell_NotifyIcon(NIM_ADD, &nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
	return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

void CProcessMain::createChildProcess()
{
	//return;
	WCHAR path[MAX_PATH] = L"";
	auto size = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
	//DWORD dwType = 0;
	//::GetBinaryTypeW(path + 1, &dwType);
	CharUpperBuffW(path, size);
	WCHAR ext[] = { L".EXE\0" };
	auto pointPos = wcsstr(path, ext);

#ifdef 	_WIN64
	//Start x86
	pointPos -= 2;
	wcsncpy_s(pointPos, path + MAX_PATH - pointPos, ext, ARRAYSIZE(ext));

#else
	//Start x64
	wcsncpy_s(pointPos, path + MAX_PATH - pointPos, L"64", 2);
	pointPos += 2;

	wcsncpy_s(pointPos, path + MAX_PATH - pointPos, ext, ARRAYSIZE(ext));
#endif
	constexpr auto quote = L'"';
	std::wstringstream stream;

	stream << quote << path << quote << L' ' << int(m_HWND);

	STARTUPINFOW startupInfo = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION processInfo = { nullptr };
	::CreateProcessW(
		nullptr, //path,
		stream.str().data(), //hwndStr,
		nullptr,
		nullptr,
		false,
		0,
		nullptr,
		nullptr,
		&startupInfo,
		&processInfo
	);
}
#ifndef 	_WIN64
typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process = nullptr;

bool CProcessMain::isWow64()
{
	BOOL bIsWow64 = FALSE;

	//IsWow64Process is not available on all supported versions of Windows.
	//Use GetModuleHandle to get a handle to the DLL that contains the function
	//and GetProcAddress to get a pointer to the function if available.
	if (fnIsWow64Process == nullptr) {
		fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
			GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

		if (nullptr != fnIsWow64Process)
		{
			if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
			{
				//handle error
			}
		}
	}
	return bIsWow64;
}
#endif

void CProcessMain::afterMainWindowCreate()
{

#ifdef _DEBUG
	RECT rect;
	GetClientRect(m_HWND, &rect);
	m_hLogHWND = CreateWindowExW(
		WS_EX_CLIENTEDGE,
		WC_LISTBOXW,
		nullptr,
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_NOSEL,
		0,
		0,
		rect.right, rect.bottom,
		m_HWND,
		nullptr,
		CApp::getInstance().handle(),
		nullptr);
	CApp::getInstance().getLog().attachToWindow(m_hLogHWND);
	ShowWindow(m_HWND, SW_SHOW);
#endif
	CProcessBase::afterMainWindowCreate();

	//delete old tray icon
	deleteTrayIcon();

	// add the notification icon
	createTrayIcon();

#ifdef 	_WIN64
	//In Win64-process start child win32-process always
	createChildProcess();
#else
	//In Win32-process start child win64-process only if run in 64bit Windows
	if (isWow64())
	{
		createChildProcess();
	}
#endif

}
#ifdef _DEBUG
void CProcessMain::readMailslot()
{
	const auto handle = m_mailSlot.get();
	auto& log = CApp::getInstance().getLog();
	while (m_mailSlot.isValid())
	{
		DWORD dwMaxMsgSize;
		DWORD dwNextSize;
		DWORD dwMessageCount;
		DWORD dwReadTimeout;

		auto result = GetMailslotInfo(
			handle,
			&dwMaxMsgSize,
			&dwNextSize,
			&dwMessageCount,
			&dwReadTimeout
		);
		if (!result || dwNextSize == 0 || dwNextSize == MAILSLOT_NO_MESSAGE || dwMessageCount == 0)
		{
			break;
		}
		auto buffer = std::make_unique<BYTE[]>(dwNextSize);
		buffer[0] = 0;
		DWORD dwReaded = 0;
		const auto lpBuffer = buffer.get();
		result = ReadFile(
			handle,
			lpBuffer,
			dwNextSize,
			&dwReaded,
			nullptr
		);
		if (!result) {
			break;
		}
		log.add((LPCTSTR)lpBuffer);
	}
}
#endif

void CProcessMain::afterActivateHook()
{
	CProcessBase::afterActivateHook();
	if (m_hwndChildProcess != nullptr)
	{
		PostMessage(m_hwndChildProcess, WMAPP_ACTIVATE, 0, 0);
	}
}

void CProcessMain::afterDeactivateHook()
{
	CProcessBase::afterDeactivateHook();
	if (m_hwndChildProcess != nullptr)
	{
		PostMessage(m_hwndChildProcess, WMAPP_DEACTIVATE, 0, 0);
	}
}
#ifdef _DEBUG
bool CProcessMain::onIdle(int i)
{
	readMailslot();
	return CProcessBase::onIdle(i);
}
#endif

void CProcessMain::showContextMenu(const POINT& pt) const
{
	const auto hInst = CApp::getInstance().handle();
	const auto hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDC_SHIFTSWITCHUI));
	if (hMenu != nullptr)
	{
		const auto hPopupMenu = GetSubMenu(hMenu, 0);
		if (hPopupMenu != nullptr) {
			// our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
			SetForegroundWindow(m_HWND);

			EnableMenuItem(hPopupMenu, IDM_ACTIVATE, MF_BYCOMMAND | (!m_bActivated ? MF_ENABLED : MF_DISABLED));
			EnableMenuItem(hPopupMenu, IDM_DEACTIVATE, MF_BYCOMMAND | (!m_bActivated ? MF_DISABLED : MF_ENABLED));

			// respect menu drop alignment
			UINT uFlags = TPM_RIGHTBUTTON;
			if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
			{
				uFlags |= TPM_RIGHTALIGN;
			}
			else
			{
				uFlags |= TPM_LEFTALIGN;
			}
			//ClientToScreen(hwnd, (LPPOINT)&pt);
			TrackPopupMenuEx(hPopupMenu, uFlags, pt.x, pt.y, m_HWND, nullptr);
		}
		DestroyMenu(hMenu);
	}
}
#ifdef _DEBUG
void CProcessMain::on_WM_SIZE(WPARAM wp, LPARAM lp)
{
	//RECT rect;
	//::GetClientRect(m_HWND, &rect);
	::MoveWindow(m_hLogHWND, 0, 0, LOWORD(lp), HIWORD(lp), false);
	//::MoveWindow(m_hLogHWND, 0, 0, rect.right, rect.bottom, false);
}
#endif 

LRESULT CProcessMain::on_WMAPP_NOTIFYCALLBACK(WPARAM wp, LPARAM lp)
{
	switch (LOWORD(lp)) {
	case WM_CONTEXTMENU:
		POINT const pt = { LOWORD(wp), HIWORD(wp) };
		showContextMenu(pt);
		break;
	}
	return true;
}

LRESULT CProcessMain::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WMAPP_NOTIFYCALLBACK:
		return on_WMAPP_NOTIFYCALLBACK(wParam, lParam);
	case WMAPP_SET_CHILD_PROCESS_HWND:
		m_hwndChildProcess = HWND(wParam);
		return 0;
#ifdef _DEBUG
	case WM_SIZE:
		on_WM_SIZE(wParam, lParam);
		break;
#endif
	case WM_DESTROY:
		while (::IsWindow(m_hwndChildProcess))
		{
			//DestroyWindow(m_hwndChildProcess);
			PostMessageW(m_hwndChildProcess, WMAPP_CHILD_CLOSE, 0, 0);
		}
		m_hwndChildProcess = nullptr;
		deleteTrayIcon();
		break;
	}
	return CProcessBase::WndProc(hWnd, message, wParam, lParam);
}
