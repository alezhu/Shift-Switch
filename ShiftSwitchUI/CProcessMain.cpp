#include "pch.h"
#include "CProcessMain.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <sstream>

#include "CApp.h"
#include "CrashReporter.h"
#include "LogOutput.h"
#include "resource.h"

constexpr auto TRAY_ICON = 666;

CProcessMain::CProcessMain()
	: m_coordinator(m_hookController, *this)
{
}

CProcessMain::~CProcessMain()
{
	destroyTrayIcons();

	if (m_childProcessInfo.hThread != nullptr)
	{
		::CloseHandle(m_childProcessInfo.hThread);
		m_childProcessInfo.hThread = nullptr;
	}
	if (m_childProcessInfo.hProcess != nullptr)
	{
		::CloseHandle(m_childProcessInfo.hProcess);
		m_childProcessInfo.hProcess = nullptr;
	}
}

void CProcessMain::initTrayIcons()
{
	if (m_hIconActive == nullptr)
	{
		const auto hInst = CApp::getInstance().handle();
		m_hIconActive = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APP));
		if (m_hIconActive != nullptr && m_hIconInactive == nullptr)
		{
			m_hIconInactive = CreateGrayscaleIcon(m_hIconActive);
		}
	}
}

void CProcessMain::destroyTrayIcons()
{
	if (m_hIconInactive != nullptr)
	{
		DestroyIcon(m_hIconInactive);
		m_hIconInactive = nullptr;
	}
	m_hIconActive = nullptr;
}

HICON CProcessMain::getTrayIcon(bool active) const noexcept
{
	if (!active && m_hIconInactive != nullptr)
	{
		return m_hIconInactive;
	}
	return m_hIconActive;
}

bool CProcessMain::deleteTrayIcon()
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = m_HWND;
	nid.uID = TRAY_ICON;
	return Shell_NotifyIcon(NIM_DELETE, &nid);
}

bool CProcessMain::createTrayIcon()
{
	initTrayIcons();

	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = m_HWND;
	// add the icon, setting the icon, tooltip, and callback message.
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
	nid.uID = TRAY_ICON;
	nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
	nid.hIcon = getTrayIcon(m_coordinator.IsEnabled());
	const auto hInst = CApp::getInstance().handle();
	LoadStringW(hInst, IDS_APP_TITLE, nid.szTip, ARRAYSIZE(nid.szTip));

	if (!Shell_NotifyIcon(NIM_ADD, &nid))
		return false;
	nid.uVersion = NOTIFYICON_VERSION_4;
	return Shell_NotifyIcon(NIM_SETVERSION, &nid) != FALSE;
}

bool CProcessMain::updateTrayIcon()
{
	initTrayIcons();

	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = m_HWND;
	nid.uID = TRAY_ICON;
	nid.uFlags = NIF_ICON;
	nid.hIcon = getTrayIcon(m_coordinator.IsEnabled());
	return Shell_NotifyIcon(NIM_MODIFY, &nid) != FALSE;
}

bool CProcessMain::createChildProcess()
{
	//return;
	WCHAR path[MAX_PATH] = L"";
	auto size = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (size == 0 || size >= MAX_PATH)
	{
		LOG_ERROR_UI(L"Failed to get module file name, error=%u", ::GetLastError());
		return false;
	}

	//DWORD dwType = 0;
	//::GetBinaryTypeW(path + 1, &dwType);
	CharUpperBuffW(path, size);
	WCHAR ext[] = { L".EXE\0" };
	auto pointPos = wcsstr(path, ext);
	if (!pointPos)
	{
		LOG_ERROR_UI(L"Failed to locate .EXE extension in path: %s", path);
		return false;
	}

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

	stream << quote << path << quote << L' ' << reinterpret_cast<UINT_PTR>(m_HWND);
	auto cmdLine = stream.str();

	STARTUPINFOW startupInfo = { sizeof(STARTUPINFOW) };
	PROCESS_INFORMATION processInfo = {};
#pragma warning(suppress: 6335) // Ownership of processInfo.hProcess is transferred to m_childProcessInfo and closed in cleanup
	if (::CreateProcessW(
		nullptr, //path,
		cmdLine.data(), //hwndStr,
		nullptr,
		nullptr,
		FALSE,
		0,
		nullptr,
		nullptr,
		&startupInfo,
		&processInfo
	))
	{
		::CloseHandle(processInfo.hThread);
		if (m_childProcessInfo.hProcess != nullptr)
		{
			::CloseHandle(m_childProcessInfo.hProcess);
		}
		m_childProcessInfo = processInfo;
		m_childProcessInfo.hThread = nullptr;
		return true;
	}

	LOG_ERROR_UI(L"Failed to create child process: %s, error=%u", path, ::GetLastError());
	return false;
}
#ifndef 	_WIN64
#include "ProcessUtils.h"

bool CProcessMain::isWow64()
{
	return ProcessUtils::isWow64Process(GetCurrentProcess());
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
	if (!createChildProcess())
	{
		LOG_ERROR_UI(L"Failed to launch 32-bit child process, rolling back main hook");
		setEnabled(false);
	}
#else
	//In Win32-process start child win64-process only if run in 64bit Windows
	if (isWow64())
	{
		if (!createChildProcess())
		{
			LOG_ERROR_UI(L"Failed to launch 64-bit child process, rolling back main hook");
			setEnabled(false);
		}
	}
#endif

	// Load configuration and register global hotkey
	loadConfig();
	registerGlobalHotkey();

	// Write a minidump to %LOCALAPPDATA%\ShiftSwitch\Crashes on a crash.
	// VERSION_STRING comes from the project file as a narrow literal.
	g_CrashReporter.Initialize(L"" VERSION_STRING);
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

bool CProcessMain::HasChild() const
{
	return m_hwndChildProcess != nullptr && ::IsWindow(m_hwndChildProcess);
}

bool CProcessMain::EnableChild()
{
	if (!HasChild())
		return false;

	DWORD_PTR result = 0;
	constexpr UINT IPC_TIMEOUT_MS = 2000;
	const LRESULT lr = SendMessageTimeoutW(
		m_hwndChildProcess,
		WMAPP_ACTIVATE,
		0,
		0,
		SMTO_BLOCK | SMTO_ABORTIFHUNG,
		IPC_TIMEOUT_MS,
		&result
	);

	if (!lr)
	{
		LOG_ERROR_UI(L"SendMessageTimeout(WMAPP_ACTIVATE) failed, error=%u", GetLastError());
		return false;
	}
	return result == STATE_RESULT_OK;
}

bool CProcessMain::DisableChild()
{
	if (!HasChild())
		return false;

	DWORD_PTR result = 0;
	constexpr UINT IPC_TIMEOUT_MS = 2000;
	const LRESULT lr = SendMessageTimeoutW(
		m_hwndChildProcess,
		WMAPP_DEACTIVATE,
		0,
		0,
		SMTO_BLOCK | SMTO_ABORTIFHUNG,
		IPC_TIMEOUT_MS,
		&result
	);

	if (!lr)
	{
		LOG_ERROR_UI(L"SendMessageTimeout(WMAPP_DEACTIVATE) failed, error=%u", GetLastError());
		return false;
	}
	return result == STATE_RESULT_OK;
}

bool CProcessMain::QueryChildState(bool& isActive)
{
	if (!HasChild())
		return false;

	DWORD_PTR result = 0;
	constexpr UINT IPC_TIMEOUT_MS = 2000;
	const LRESULT lr = SendMessageTimeoutW(
		m_hwndChildProcess,
		WMAPP_QUERY_STATE,
		0,
		0,
		SMTO_BLOCK | SMTO_ABORTIFHUNG,
		IPC_TIMEOUT_MS,
		&result
	);

	if (!lr)
		return false;

	isActive = (result == STATE_RESULT_OK);
	return true;
}

bool CProcessMain::setEnabled(bool enable)
{
	const bool success = m_coordinator.SetEnabled(enable);
	if (!success)
	{
		LOG_ERROR_UI(L"Atomic setEnabled(%d) failed across x86/x64 processes", enable);
	}
	else
	{
		LOG_INFO_UI(L"Atomic setEnabled(%d) succeeded across x86/x64 processes", enable);
		updateTrayIcon();
	}
	return success;
}

void CProcessMain::onChildProcessHwndReceived(HWND hwnd)
{
	m_hwndChildProcess = hwnd;
	LOG_INFO_UI(L"Child process HWND registered: %p", hwnd);
	if (!m_coordinator.OnChildRegistered())
	{
		LOG_ERROR_UI(L"Failed to synchronize child hook state on registration, rolling back main hook");
		updateTrayIcon();
	}
	else
	{
		LOG_INFO_UI(L"Child hook state successfully synchronized on registration");
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
	static bool s_isMenuShowing = false;
	if (s_isMenuShowing)
	{
		return;
	}

	const auto hInst = CApp::getInstance().handle();
	const auto hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDC_SHIFTSWITCHUI));
	if (hMenu != nullptr)
	{
		const auto hPopupMenu = GetSubMenu(hMenu, 0);
		if (hPopupMenu != nullptr) {
			s_isMenuShowing = true;

			// Ensure our window is foreground before TrackPopupMenu (KB135788)
			SetForegroundWindow(m_HWND);
			ShowWindow(m_HWND, SW_HIDE);

			const bool active = m_coordinator.IsEnabled();
			EnableMenuItem(hPopupMenu, IDM_ACTIVATE, MF_BYCOMMAND | (!active ? MF_ENABLED : MF_DISABLED));
			EnableMenuItem(hPopupMenu, IDM_DEACTIVATE, MF_BYCOMMAND | (!active ? MF_DISABLED : MF_ENABLED));

			POINT menuPt = {};
			if (pt.x == -1 && pt.y == -1)
			{
				NOTIFYICONIDENTIFIER nid = { sizeof(nid) };
				nid.hWnd = m_HWND;
				nid.uID = TRAY_ICON;
				RECT rcIcon = {};
				if (Shell_NotifyIconGetRect(&nid, &rcIcon) == S_OK)
				{
					menuPt.x = (rcIcon.left + rcIcon.right) / 2;
					menuPt.y = (rcIcon.top + rcIcon.bottom) / 2;
				}
				else
				{
					GetCursorPos(&menuPt);
				}
			}
			else
			{
				GetCursorPos(&menuPt);
			}

			// Show popup menu exactly like CLCL (menu_show):
			// TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD without rcExclude
			const UINT cmd = TrackPopupMenu(hPopupMenu,
				TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
				menuPt.x, menuPt.y, 0, m_HWND, nullptr);

			PostMessageW(m_HWND, WM_NULL, 0, 0);

			if (cmd != 0)
			{
				SendMessageW(m_HWND, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
			}

			s_isMenuShowing = false;
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

void CProcessMain::loadConfig()
{
	LoadHotkeyConfig(HKEY_CURRENT_USER, SHIFTSWITCH_REG_KEY, m_hotkeyConfig);
	LoadTrayDblClickAction(HKEY_CURRENT_USER, SHIFTSWITCH_REG_KEY, m_trayDblClickAction);
}

bool CProcessMain::registerGlobalHotkey()
{
	unregisterGlobalHotkey();

	if (m_hotkeyConfig.vk == 0)
	{
		LOG_INFO_UI(L"Global hotkey disabled (None)");
		return true;
	}

	if (::RegisterHotKey(m_HWND, TOGGLE_HOTKEY_ID, m_hotkeyConfig.modifiers, m_hotkeyConfig.vk))
	{
		m_hotkeyRegistered = true;
		LOG_INFO_UI(L"Global hotkey registered: VK=0x%02X, Mod=0x%04X", m_hotkeyConfig.vk, m_hotkeyConfig.modifiers);
		return true;
	}
	else
	{
		LOG_ERROR_UI(L"Failed to register global hotkey (VK=0x%02X, Mod=0x%04X), error=%u",
			m_hotkeyConfig.vk, m_hotkeyConfig.modifiers, ::GetLastError());
		return false;
	}
}

void CProcessMain::unregisterGlobalHotkey()
{
	if (m_hotkeyRegistered)
	{
		::UnregisterHotKey(m_HWND, TOGGLE_HOTKEY_ID);
		m_hotkeyRegistered = false;
	}
}

void CProcessMain::executeTrayDblClickAction()
{
	switch (m_trayDblClickAction)
	{
	case TrayDblClickAction::ToggleActive:
	{
		const bool targetState = !m_coordinator.IsEnabled();
		LOG_INFO_UI(L"Tray double-click triggered: switching hooks to %s", targetState ? L"Enabled" : L"Disabled");
		setEnabled(targetState);
		break;
	}
	case TrayDblClickAction::Settings:
		PostMessageW(m_HWND, WM_COMMAND, MAKEWPARAM(IDM_SETTINGS, 0), 0);
		break;
	case TrayDblClickAction::Exclusions:
		PostMessageW(m_HWND, WM_COMMAND, MAKEWPARAM(IDM_EXCLUSIONS, 0), 0);
		break;
	case TrayDblClickAction::About:
		PostMessageW(m_HWND, WM_COMMAND, MAKEWPARAM(IDM_ABOUT, 0), 0);
		break;
	}
}

LRESULT CProcessMain::on_WM_COMMAND(WPARAM wp, LPARAM lp)
{
	const int wmId = LOWORD(wp);
	const LRESULT lr = CProcessBase::on_WM_COMMAND(wp, lp);
	if (wmId == IDM_SETTINGS)
	{
		loadConfig();
		if (!registerGlobalHotkey() && m_hotkeyConfig.vk != 0)
		{
			MessageBoxW(m_HWND,
				L"Could not register the selected global hotkey.\nIt may already be in use by another application.",
				L"Shift Switch",
				MB_ICONWARNING | MB_OK);
		}
	}
	return lr;
}

LRESULT CProcessMain::on_WMAPP_NOTIFYCALLBACK(WPARAM wp, LPARAM lp)
{
	switch (LOWORD(lp)) {
	case WM_CONTEXTMENU:
	case WM_RBUTTONUP:
	{
		POINT const pt = { GET_X_LPARAM(static_cast<LPARAM>(wp)), GET_Y_LPARAM(static_cast<LPARAM>(wp)) };
		showContextMenu(pt);
		break;
	}
	case WM_LBUTTONDBLCLK:
	{
		executeTrayDblClickAction();
		break;
	}
	}
	return true;
}

void CProcessMain::on_WM_HOTKEY(WPARAM wParam, LPARAM lParam)
{
	if (wParam == TOGGLE_HOTKEY_ID)
	{
		const bool targetState = !m_coordinator.IsEnabled();
		LOG_INFO_UI(L"Global hotkey triggered: switching hooks to %s", targetState ? L"Enabled" : L"Disabled");
		setEnabled(targetState);
	}
}

LRESULT CProcessMain::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WMAPP_NOTIFYCALLBACK:
		return on_WMAPP_NOTIFYCALLBACK(wParam, lParam);
	case WMAPP_SET_CHILD_PROCESS_HWND:
		onChildProcessHwndReceived(reinterpret_cast<HWND>(wParam));
		return 0;
	case WM_HOTKEY:
		on_WM_HOTKEY(wParam, lParam);
		break;
#ifdef _DEBUG
	case WM_SIZE:
		on_WM_SIZE(wParam, lParam);
		break;
#endif
	case WM_DESTROY:
		unregisterGlobalHotkey();
		if (m_hwndChildProcess != nullptr && ::IsWindow(m_hwndChildProcess))
		{
			DWORD_PTR result = 0;
			constexpr UINT IPC_TIMEOUT_MS = 1000;
			SendMessageTimeoutW(m_hwndChildProcess, WMAPP_CHILD_CLOSE, 0, 0,
				SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &result);
		}
		m_hwndChildProcess = nullptr;

		if (m_childProcessInfo.hProcess != nullptr)
		{
			constexpr DWORD CHILD_WAIT_TIMEOUT_MS = 2000;
			const DWORD waitResult = WaitForSingleObject(m_childProcessInfo.hProcess, CHILD_WAIT_TIMEOUT_MS);
			if (waitResult == WAIT_TIMEOUT)
			{
				LOG_WARN_UI(L"Child process (PID %u) did not exit within timeout, terminating", m_childProcessInfo.dwProcessId);
				TerminateProcess(m_childProcessInfo.hProcess, 0);
			}
			CloseHandle(m_childProcessInfo.hProcess);
			m_childProcessInfo.hProcess = nullptr;
		}

		deleteTrayIcon();
		destroyTrayIcons();
		g_CrashReporter.Shutdown();
		break;
	}
	return CProcessBase::WndProc(hWnd, message, wParam, lParam);
}
