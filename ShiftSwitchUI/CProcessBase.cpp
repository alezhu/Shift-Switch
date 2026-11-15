#include "pch.h"
#include "CProcessBase.h"

#include <shlwapi.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include <memory>
#include "CApp.h"
#include "CHandle.h"
#include "ScopeExit.h"
#include "ScopedResource.h"
#include "AppSettings.h"
#include "Exclusions.h"
#include "LogOutput.h"
#include "resource.h"

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

static HWND s_hAboutDlg = nullptr;
static HWND s_hSettingsDlg = nullptr;
static HWND s_hExclusionsDlg = nullptr;

bool CProcessBase::createMainWindow()
{
	auto classId = registerMainWindowClass();
	if (0 != classId)
	{
		const CApp& app = CApp::getInstance();

		m_HWND = CreateWindowW(
			LPCWSTR(classId),
			app.getResourceString(IDS_APP_TITLE),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			0,
			CW_USEDEFAULT,
			0,
			getParentHWnd(),
			nullptr,
			app.handle(),
			nullptr
		);
	}
	return m_HWND != nullptr;
}

void CProcessBase::afterMainWindowCreate()
{
	UpdateWindow(m_HWND);
	setEnabled(true);
}

bool CProcessBase::setEnabled(bool enable)
{
	return enable ? m_hookController.Enable() : m_hookController.Disable();
}

bool CProcessBase::onIdle(int i)
{
	return false;
}

bool CProcessBase::isIdleMessage(MSG* msg) const
{
	// These messages should NOT cause idle processing
	switch (msg->message)
	{
	case WM_MOUSEMOVE:
	case WM_NCMOUSEMOVE:
	case WM_PAINT:
	case 0x0118:	// WM_SYSTIMER (caret blink)
		return false;
	default:
		return true;
	}
}

int CProcessBase::runMessageLoop()
{
	bool bDoIdle = true;
	int nIdleCount = 0;

	MSG msg;
	while (true)
	{
		while (bDoIdle && !::PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE))
		{
			if (!onIdle(nIdleCount++))
				bDoIdle = false;
		}

		const BOOL rc = ::GetMessage(&msg, nullptr, 0, 0);

		if (rc == -1)
		{
			continue;   // error, don't process
		}
		else if (rc == 0)
		{
			// WM_QUIT, exit message loop
			return static_cast<int>(msg.wParam);
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (isIdleMessage(&msg))
		{
			bDoIdle = true;
			nIdleCount = 0;
		}
	}
}

int CProcessBase::start()
{
	if (!createMainWindow()) {
		return 1;
	}
	afterMainWindowCreate();
	return runMessageLoop();
}

LRESULT CProcessBase::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	m_HWND = hWnd;
	switch (message) {
	case WM_CREATE:
		return on_WM_CREATE(wParam, lParam);
	case WM_COMMAND:
		return on_WM_COMMAND(wParam, lParam);
	case  WMAPP_ACTIVATE:
		return setEnabled(true) ? STATE_RESULT_OK : STATE_RESULT_ERROR;
	case  WMAPP_DEACTIVATE:
		return setEnabled(false) ? STATE_RESULT_OK : STATE_RESULT_ERROR;
	case WMAPP_QUERY_STATE:
		return isEnabled() ? STATE_RESULT_OK : STATE_RESULT_ERROR;
	case WM_DESTROY:
		m_hookController.Shutdown();
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

#ifdef _DEBUG
CLog& CProcessBase::getLog() const
{
	return CApp::getInstance().getLog();
}

void CProcessBase::log(LPCWSTR message)
{
	getLog().add(message);
}
#endif

LRESULT CProcessBase::on_WM_CREATE(WPARAM wp, LPARAM lp)
{
	return 0;
}

// Centers a window on the work area of the monitor nearest to the mouse cursor.
static void CenterWindowOnWorkArea(HWND hWnd)
{
	POINT pt = {};
	GetCursorPos(&pt);
	HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi = { sizeof(mi) };
	if (GetMonitorInfoW(hMonitor, &mi))
	{
		RECT rcDlg = {};
		GetWindowRect(hWnd, &rcDlg);
		const int dlgWidth = rcDlg.right - rcDlg.left;
		const int dlgHeight = rcDlg.bottom - rcDlg.top;
		const int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - dlgWidth) / 2;
		const int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - dlgHeight) / 2;
		SetWindowPos(hWnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	}
}

// Message handler for about box.
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		s_hAboutDlg = hDlg;
		// The version text is composed here: a version info block would show it
		// as well, but rc.exe cannot build that string from the numeric macros.
		const std::wstring text = L"Shift Switch, Version " L"" VERSION_STRING;
		SetDlgItemTextW(hDlg, IDC_VERSION_TEXT, text.c_str());

		SYSTEMTIME st = {};
		GetLocalTime(&st);
		const int currentYear = (st.wYear >= 2022) ? st.wYear : 2022;
		std::wstring copyright = L"Copyright \u00A9 2022";
		if (currentYear > 2022)
		{
			copyright += L"-" + std::to_wstring(currentYear);
		}
		copyright += L" by Alezhu";
		SetDlgItemTextW(hDlg, IDC_COPYRIGHT_TEXT, copyright.c_str());

		CenterWindowOnWorkArea(hDlg);

		return (INT_PTR)TRUE;
	}

	case WM_NOTIFY:
	{
		LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
		if (pnmh != nullptr && pnmh->idFrom == IDC_LINK_RELEASES && (pnmh->code == NM_CLICK || pnmh->code == NM_RETURN))
		{
			PNMLINK pNMLink = reinterpret_cast<PNMLINK>(lParam);
			const WCHAR* url = (pNMLink->item.szUrl[0] != L'\0')
				? pNMLink->item.szUrl
				: L"https://github.com/alezhu/Shift-Switch/releases";
			ShellExecuteW(hDlg, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
			return (INT_PTR)TRUE;
		}
		break;
	}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;

	case WM_DESTROY:
		s_hAboutDlg = nullptr;
		break;
	}
	return (INT_PTR)FALSE;
}

// Settings dialog: enumerate keyboard layouts and let user pick Left/Right Shift layouts.
struct LayoutItem
{
	WCHAR klid[KL_NAMELENGTH];
	WCHAR name[64];
};

static std::vector<LayoutItem> g_layouts;

// Remember the dialog positions in HKCU\Software\ShiftSwitch. The window is
// restored only when it fits the work area of the nearest monitor, so a
// monitor that was disconnected does not leave the dialog off-screen.
static void RestoreDialogPosition(HWND hDlg, const WCHAR* valueName, bool restoreSize = false)
{
	ScopedHKey hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ShiftSwitch", 0, KEY_READ, hKey.addressof()) != ERROR_SUCCESS)
		return;

	WCHAR buffer[64] = L"";
	DWORD size = sizeof(buffer);
	DWORD type = 0;
	const auto result = RegQueryValueExW(hKey, valueName, nullptr, &type,
		reinterpret_cast<LPBYTE>(buffer), &size);
	if (result != ERROR_SUCCESS || type != REG_SZ)
		return;

	int x = 0;
	int y = 0;
	int cx = 0;
	int cy = 0;
	const int parsed = swscanf_s(buffer, L"%d,%d,%d,%d", &x, &y, &cx, &cy);
	if (parsed < 2)
		return;

	RECT rect = {};
	if (!GetWindowRect(hDlg, &rect))
		return;
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	if (restoreSize && parsed >= 4 && cx > 0 && cy > 0)
	{
		width = cx;
		height = cy;
	}

	const POINT point = { x, y };
	const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info = { sizeof(info) };
	if (GetMonitorInfoW(monitor, &info))
	{
		const int maxW = info.rcWork.right - info.rcWork.left;
		const int maxH = info.rcWork.bottom - info.rcWork.top;
		if (width > maxW) width = maxW;
		if (height > maxH) height = maxH;

		if (x < info.rcWork.left)
			x = info.rcWork.left;
		if (y < info.rcWork.top)
			y = info.rcWork.top;
		if (x + width > info.rcWork.right)
			x = max(info.rcWork.left, info.rcWork.right - width);
		if (y + height > info.rcWork.bottom)
			y = max(info.rcWork.top, info.rcWork.bottom - height);
	}

	UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
	if (!restoreSize || parsed < 4)
		flags |= SWP_NOSIZE;

	SetWindowPos(hDlg, nullptr, x, y, width, height, flags);
}

static void SaveDialogPosition(HWND hDlg, const WCHAR* valueName, bool saveSize = false)
{
	RECT rect = {};
	if (!GetWindowRect(hDlg, &rect))
		return;

	WCHAR buffer[64] = L"";
	if (saveSize)
		swprintf_s(buffer, L"%d,%d,%d,%d", rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
	else
		swprintf_s(buffer, L"%d,%d", rect.left, rect.top);

	ScopedHKey hKey;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ShiftSwitch", 0, nullptr, 0,
		KEY_WRITE, nullptr, hKey.addressof(), nullptr) != ERROR_SUCCESS)
		return;
	RegSetValueExW(hKey, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(buffer),
		static_cast<DWORD>((wcslen(buffer) + 1) * sizeof(WCHAR)));
}

static void PopulateLayoutCombo(HWND hCombo, const WCHAR* currentKlid)
{
	ComboBox_ResetContent(hCombo);
	for (size_t i = 0; i < g_layouts.size(); ++i)
	{
		int idx = ComboBox_AddString(hCombo, g_layouts[i].name);
		ComboBox_SetItemData(hCombo, idx, static_cast<LPARAM>(i));
		if (wcscmp(g_layouts[i].klid, currentKlid) == 0)
			ComboBox_SetCurSel(hCombo, idx);
	}
	if (ComboBox_GetCurSel(hCombo) == CB_ERR && !g_layouts.empty())
		ComboBox_SetCurSel(hCombo, 0);
}

// Human readable name of a layout: the localized language name, followed by the
// layout name when that adds information, for example a Dvorak variant of the
// same language. The KLID lookup through GetLocaleInfoEx does not work here
// because it expects a locale name such as "en-US", not a keyboard layout
// identifier.
static void GetLayoutName(HKL hkl, const WCHAR* klid, WCHAR* name, size_t cch)
{
	name[0] = L'\0';

	const LANGID langId = LOWORD(reinterpret_cast<ULONG_PTR>(hkl));
	if (langId != 0)
	{
		const LCID lcid = MAKELCID(langId, SORT_DEFAULT);
		if (GetLocaleInfoW(lcid, LOCALE_SLOCALIZEDDISPLAYNAME, name, static_cast<int>(cch)) == 0)
			GetLocaleInfoW(lcid, LOCALE_SLANGUAGE, name, static_cast<int>(cch));
	}

	if (name[0] == L'\0')
	{
		wcscpy_s(name, cch, klid);
		return;
	}

	// Layout specific name from the registry, so that several layouts of the
	// same language can be told apart. It may be an indirect string resource.
	WCHAR layoutName[128] = L"";
	ScopedHKey hKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		(L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\" + std::wstring(klid)).c_str(),
		0, KEY_READ, hKey.addressof()) == ERROR_SUCCESS)
	{
		DWORD size = sizeof(layoutName);
		DWORD type = 0;
		if (RegQueryValueExW(hKey, L"Layout Display Name", nullptr, &type,
			reinterpret_cast<LPBYTE>(layoutName), &size) != ERROR_SUCCESS || type != REG_SZ)
			layoutName[0] = L'\0';
	}

	if (layoutName[0] == L'@')
	{
		WCHAR expanded[128] = L"";
		if (SUCCEEDED(SHLoadIndirectString(layoutName, expanded, _countof(expanded), nullptr)))
			wcscpy_s(layoutName, expanded);
		else
			layoutName[0] = L'\0';
	}

	if (layoutName[0] == L'\0')
		return;

	// The language name already ends with the country ("English (United
	// States)"), so the layout name is only appended when it adds information,
	// for example for two layouts of the same language.
	WCHAR country[64] = L"";
	const WCHAR* open = wcschr(name, L'(');
	const WCHAR* close = open ? wcschr(open, L')') : nullptr;
	if (open && close && close > open + 1)
	{
		const size_t length = min(static_cast<size_t>(close - open - 1), _countof(country) - 1);
		wcsncpy_s(country, _countof(country), open + 1, length);
	}

	if (_wcsicmp(layoutName, name) != 0 && _wcsicmp(layoutName, country) != 0)
	{
		wcscat_s(name, cch, L" - ");
		wcscat_s(name, cch, layoutName);
	}
}

static void EnumKeyboardLayouts()
{
	g_layouts.clear();
	UINT count = GetKeyboardLayoutList(0, nullptr);
	if (count == 0) return;

	std::vector<HKL> hkls(count);
	GetKeyboardLayoutList(count, hkls.data());

	const HKL currentHkl = GetKeyboardLayout(0);
	WCHAR klid[KL_NAMELENGTH];

	for (HKL hkl : hkls)
	{
		// GetKeyboardLayoutNameW only reports the active layout, so every
		// layout has to be activated to read its identifier.
		if (ActivateKeyboardLayout(hkl, 0) == nullptr)
			continue;
		if (!GetKeyboardLayoutNameW(klid))
			continue;

		bool duplicate = false;
		for (const LayoutItem& item : g_layouts)
		{
			if (wcscmp(item.klid, klid) == 0)
			{
				duplicate = true;
				break;
			}
		}
		if (duplicate)
			continue;

		LayoutItem item;
		wcscpy_s(item.klid, klid);
		GetLayoutName(hkl, klid, item.name, _countof(item.name));
		g_layouts.push_back(item);
	}

	ActivateKeyboardLayout(currentHkl, 0);
}

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		s_hSettingsDlg = hDlg;
		RestoreDialogPosition(hDlg, L"SettingsPos");
		EnumKeyboardLayouts();

		WCHAR klidLeft[KL_NAMELENGTH] = L"00000409";
		WCHAR klidRight[KL_NAMELENGTH] = L"00000419";
		ScopedHKey hKey;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ShiftSwitch", 0, KEY_READ, hKey.addressof()) == ERROR_SUCCESS)
		{
			DWORD size = sizeof(klidLeft);
			RegQueryValueExW(hKey, L"LayoutLeft", nullptr, nullptr, reinterpret_cast<LPBYTE>(klidLeft), &size);
			size = sizeof(klidRight);
			RegQueryValueExW(hKey, L"LayoutRight", nullptr, nullptr, reinterpret_cast<LPBYTE>(klidRight), &size);
		}

		PopulateLayoutCombo(GetDlgItem(hDlg, IDC_COMBO_LEFT), klidLeft);
		PopulateLayoutCombo(GetDlgItem(hDlg, IDC_COMBO_RIGHT), klidRight);

		// Load and populate hotkey
		HotkeyConfig hotkeyConfig;
		LoadHotkeyConfig(HKEY_CURRENT_USER, SHIFTSWITCH_REG_KEY, hotkeyConfig);
		HWND hHotkey = GetDlgItem(hDlg, IDC_HOTKEY_TOGGLE);
		SendMessage(hHotkey, HKM_SETRULES, HKCOMB_NONE | HKCOMB_S, MAKELPARAM(HOTKEYF_CONTROL | HOTKEYF_ALT, 0));
		const BYTE hkMods = ModToHotkeyModifiers(hotkeyConfig.modifiers);
		const WORD hkVal = (hotkeyConfig.vk != 0) ? MAKEWORD(static_cast<BYTE>(hotkeyConfig.vk), hkMods) : 0;
		SendMessage(hHotkey, HKM_SETHOTKEY, hkVal, 0);

		// Load and populate tray double-click action
		TrayDblClickAction dblClickAction = TrayDblClickAction::ToggleActive;
		LoadTrayDblClickAction(HKEY_CURRENT_USER, SHIFTSWITCH_REG_KEY, dblClickAction);
		HWND hComboDbl = GetDlgItem(hDlg, IDC_COMBO_TRAY_DBLCLICK);
		ComboBox_AddString(hComboDbl, L"Activate / Deactivate");
		ComboBox_AddString(hComboDbl, L"Settings");
		ComboBox_AddString(hComboDbl, L"Exclusions");
		ComboBox_AddString(hComboDbl, L"About");
		ComboBox_SetCurSel(hComboDbl, static_cast<int>(dblClickAction));

		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_BTN_CLEAR_HOTKEY)
		{
			SendMessage(GetDlgItem(hDlg, IDC_HOTKEY_TOGGLE), HKM_SETHOTKEY, 0, 0);
			SetFocus(GetDlgItem(hDlg, IDC_HOTKEY_TOGGLE));
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDOK)
		{
			int leftIdx = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_COMBO_LEFT));
			int rightIdx = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_COMBO_RIGHT));
			if (leftIdx != CB_ERR && rightIdx != CB_ERR)
			{
				int leftData = static_cast<int>(ComboBox_GetItemData(GetDlgItem(hDlg, IDC_COMBO_LEFT), leftIdx));
				int rightData = static_cast<int>(ComboBox_GetItemData(GetDlgItem(hDlg, IDC_COMBO_RIGHT), rightIdx));
				if (leftData >= 0 && leftData < static_cast<int>(g_layouts.size()) &&
					rightData >= 0 && rightData < static_cast<int>(g_layouts.size()))
				{
					const LayoutItem* leftItem = &g_layouts[leftData];
					const LayoutItem* rightItem = &g_layouts[rightData];

					ScopedHKey hKey;
					if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ShiftSwitch", 0, nullptr, 0, KEY_WRITE, nullptr, hKey.addressof(), nullptr) == ERROR_SUCCESS)
					{
						RegSetValueExW(hKey, L"LayoutLeft", 0, REG_SZ, reinterpret_cast<const BYTE*>(leftItem->klid), static_cast<DWORD>((wcslen(leftItem->klid) + 1) * sizeof(WCHAR)));
						RegSetValueExW(hKey, L"LayoutRight", 0, REG_SZ, reinterpret_cast<const BYTE*>(rightItem->klid), static_cast<DWORD>((wcslen(rightItem->klid) + 1) * sizeof(WCHAR)));
					}
				}
			}

			// Save hotkey
			LRESULT hkResult = SendMessage(GetDlgItem(hDlg, IDC_HOTKEY_TOGGLE), HKM_GETHOTKEY, 0, 0);
			BYTE vk = LOBYTE(hkResult);
			BYTE hkMods = HIBYTE(hkResult);
			HotkeyConfig hotkeyConfig;
			hotkeyConfig.vk = vk;
			hotkeyConfig.modifiers = HotkeyModifiersToMod(hkMods);
			SaveHotkeyConfig(HKEY_CURRENT_USER, SHIFTSWITCH_REG_KEY, hotkeyConfig);

			// Save tray double-click action
			int dblIdx = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_COMBO_TRAY_DBLCLICK));
			if (dblIdx != CB_ERR && dblIdx >= 0 && dblIdx <= 3)
			{
				SaveTrayDblClickAction(HKEY_CURRENT_USER, SHIFTSWITCH_REG_KEY, static_cast<TrayDblClickAction>(dblIdx));
			}

			SaveDialogPosition(hDlg, L"SettingsPos");
			EndDialog(hDlg, IDOK);
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			SaveDialogPosition(hDlg, L"SettingsPos");
			EndDialog(hDlg, IDCANCEL);
			return (INT_PTR)TRUE;
		}
		break;

	case WM_DESTROY:
		s_hSettingsDlg = nullptr;
		break;
	}
	return (INT_PTR)FALSE;
}

// --- Exclusions and Running Processes Management ---

static std::wstring FindRunningProcessPath(const std::wstring& exeName)
{
	CHandle hSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (!hSnap.isValid())
		return L"";

	PROCESSENTRY32W pe = { sizeof(pe) };
	if (Process32FirstW(hSnap, &pe))
	{
		do
		{
			if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0)
			{
				CHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID));
				if (hProcess.isValid())
				{
					WCHAR path[MAX_PATH] = {};
					DWORD len = MAX_PATH;
					if (QueryFullProcessImageNameW(hProcess, 0, path, &len))
						return path;
				}
			}
		} while (Process32NextW(hSnap, &pe));
	}
	return L"";
}

static HICON ExtractProcessIcon(const std::wstring& exeName, const std::wstring& fullPath = L"")
{
	if (!fullPath.empty())
	{
		SHFILEINFOW sfi = {};
		if (SHGetFileInfoW(fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) && sfi.hIcon)
			return sfi.hIcon;
	}

	const std::wstring runningPath = FindRunningProcessPath(exeName);
	if (!runningPath.empty())
	{
		SHFILEINFOW sfi = {};
		if (SHGetFileInfoW(runningPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) && sfi.hIcon)
			return sfi.hIcon;
	}

	WCHAR foundPath[MAX_PATH] = {};
	if (SearchPathW(nullptr, exeName.c_str(), L".exe", MAX_PATH, foundPath, nullptr))
	{
		SHFILEINFOW sfi = {};
		if (SHGetFileInfoW(foundPath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) && sfi.hIcon)
			return sfi.hIcon;
	}

	std::wstring appPathKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeName;
	WCHAR regPath[MAX_PATH] = {};
	DWORD size = sizeof(regPath);
	if (RegGetValueW(HKEY_CURRENT_USER, appPathKey.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, regPath, &size) == ERROR_SUCCESS ||
		RegGetValueW(HKEY_LOCAL_MACHINE, appPathKey.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, regPath, &size) == ERROR_SUCCESS)
	{
		PathUnquoteSpacesW(regPath);
		SHFILEINFOW sfi = {};
		if (SHGetFileInfoW(regPath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) && sfi.hIcon)
			return sfi.hIcon;
	}

	SHFILEINFOW sfi = {};
	if (SHGetFileInfoW(L".exe", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
		SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES) && sfi.hIcon)
	{
		return sfi.hIcon;
	}

	return nullptr;
}

class ProcessIconList
{
public:
	ProcessIconList()
	{
		const int cx = GetSystemMetrics(SM_CXSMICON);
		const int cy = GetSystemMetrics(SM_CYSMICON);
		m_hImageList = ImageList_Create(cx, cy, ILC_COLOR32 | ILC_MASK, 32, 32);
	}

	~ProcessIconList()
	{
		if (m_hImageList)
		{
			ImageList_Destroy(m_hImageList);
			m_hImageList = nullptr;
		}
	}

	HIMAGELIST handle() const { return m_hImageList; }

	int getOrAddIcon(const std::wstring& exeName, const std::wstring& fullPath = L"")
	{
		std::wstring key = exeName;
		std::transform(key.begin(), key.end(), key.begin(), ::towlower);
		auto it = m_map.find(key);
		if (it != m_map.end())
			return it->second;

		HICON hIcon = ExtractProcessIcon(exeName, fullPath);
		int index = -1;
		if (hIcon && m_hImageList)
		{
			index = ImageList_AddIcon(m_hImageList, hIcon);
			DestroyIcon(hIcon);
		}
		if (index >= 0)
			m_map[key] = index;
		return index;
	}

private:
	HIMAGELIST m_hImageList = nullptr;
	std::map<std::wstring, int> m_map;
};

struct RunningProcessEntry
{
	DWORD pid = 0;
	std::wstring exeName;
	std::wstring lowerExe;
	std::wstring windowTitle;
	std::wstring lowerTitle;
	std::wstring fullPath;
	std::wstring lowerPath;
	int iconIndex = -1;
};

static bool CompareRunningProcesses(const RunningProcessEntry& a, const RunningProcessEntry& b, int sortColumn, bool sortAscending)
{
	int cmp = 0;
	switch (sortColumn)
	{
	case 0: // Process name
		cmp = _wcsicmp(a.exeName.c_str(), b.exeName.c_str());
		break;

	case 1: // Window title
		if (!a.windowTitle.empty() && b.windowTitle.empty())
			return true;
		if (a.windowTitle.empty() && !b.windowTitle.empty())
			return false;
		cmp = _wcsicmp(a.windowTitle.c_str(), b.windowTitle.c_str());
		break;

	case 2: // PID
		if (a.pid < b.pid) cmp = -1;
		else if (a.pid > b.pid) cmp = 1;
		else cmp = 0;
		break;

	case 3: // Full path
		if (!a.fullPath.empty() && b.fullPath.empty())
			return true;
		if (a.fullPath.empty() && !b.fullPath.empty())
			return false;
		cmp = _wcsicmp(a.fullPath.c_str(), b.fullPath.c_str());
		break;

	default:
		break;
	}

	// Tie-breakers
	if (cmp == 0)
	{
		cmp = _wcsicmp(a.exeName.c_str(), b.exeName.c_str());
	}
	if (cmp == 0)
	{
		if (a.pid < b.pid) cmp = -1;
		else if (a.pid > b.pid) cmp = 1;
	}

	return sortAscending ? (cmp < 0) : (cmp > 0);
}

static BOOL CALLBACK EnumWindowsProcessCallback(HWND hwnd, LPARAM lParam)
{
	if (!IsWindowVisible(hwnd))
		return TRUE;

	const int len = GetWindowTextLengthW(hwnd);
	if (len == 0)
		return TRUE;

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == 0)
		return TRUE;

	auto* map = reinterpret_cast<std::map<DWORD, std::wstring>*>(lParam);
	if (map->find(pid) == map->end())
	{
		std::vector<WCHAR> title(static_cast<size_t>(len) + 1);
		GetWindowTextW(hwnd, title.data(), len + 1);
		if (title[0] != L'\0')
			(*map)[pid] = title.data();
	}
	return TRUE;
}

static std::vector<RunningProcessEntry> EnumerateRunningProcesses(ProcessIconList& iconList)
{
	std::map<DWORD, std::wstring> pidToTitle;
	EnumWindows(EnumWindowsProcessCallback, reinterpret_cast<LPARAM>(&pidToTitle));

	CHandle hSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (!hSnap.isValid())
		return {};

	const DWORD currentPid = GetCurrentProcessId();
	std::map<std::wstring, RunningProcessEntry> uniqueMap;

	PROCESSENTRY32W pe = { sizeof(pe) };
	if (Process32FirstW(hSnap, &pe))
	{
		do
		{
			if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4 || pe.th32ProcessID == currentPid)
				continue;
			if (pe.szExeFile[0] == L'\0')
				continue;
			if (_wcsicmp(pe.szExeFile, L"[System Process]") == 0 ||
				_wcsicmp(pe.szExeFile, L"System") == 0 ||
				_wcsicmp(pe.szExeFile, L"Registry") == 0 ||
				_wcsicmp(pe.szExeFile, L"Memory Compression") == 0)
				continue;

			std::wstring exeName = pe.szExeFile;
			std::wstring key = exeName;
			std::transform(key.begin(), key.end(), key.begin(), ::towlower);

			std::wstring fullPath;
			CHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID));
			if (hProcess.isValid())
			{
				WCHAR pathBuf[MAX_PATH] = {};
				DWORD pathLen = MAX_PATH;
				if (QueryFullProcessImageNameW(hProcess, 0, pathBuf, &pathLen))
					fullPath = pathBuf;
			}

			std::wstring title;
			auto itTitle = pidToTitle.find(pe.th32ProcessID);
			if (itTitle != pidToTitle.end())
				title = itTitle->second;

			auto existing = uniqueMap.find(key);
			if (existing == uniqueMap.end())
			{
				RunningProcessEntry entry;
				entry.pid = pe.th32ProcessID;
				entry.exeName = exeName;
				entry.lowerExe = key;
				entry.windowTitle = title;
				entry.lowerTitle = title;
				std::transform(entry.lowerTitle.begin(), entry.lowerTitle.end(), entry.lowerTitle.begin(), ::towlower);
				entry.fullPath = fullPath;
				entry.lowerPath = fullPath;
				std::transform(entry.lowerPath.begin(), entry.lowerPath.end(), entry.lowerPath.begin(), ::towlower);
				uniqueMap[key] = std::move(entry);
			}
			else
			{
				if (existing->second.windowTitle.empty() && !title.empty())
				{
					existing->second.pid = pe.th32ProcessID;
					existing->second.windowTitle = title;
					existing->second.lowerTitle = title;
					std::transform(existing->second.lowerTitle.begin(), existing->second.lowerTitle.end(),
						existing->second.lowerTitle.begin(), ::towlower);
				}
				if (existing->second.fullPath.empty() && !fullPath.empty())
				{
					existing->second.fullPath = fullPath;
					existing->second.lowerPath = fullPath;
					std::transform(existing->second.lowerPath.begin(), existing->second.lowerPath.end(),
						existing->second.lowerPath.begin(), ::towlower);
				}
			}
		} while (Process32NextW(hSnap, &pe));
	}

	std::vector<RunningProcessEntry> result;
	result.reserve(uniqueMap.size());
	for (auto& pair : uniqueMap)
	{
		pair.second.iconIndex = iconList.getOrAddIcon(pair.second.exeName, pair.second.fullPath);
		result.push_back(std::move(pair.second));
	}

	std::sort(result.begin(), result.end(), [](const RunningProcessEntry& a, const RunningProcessEntry& b) {
		return CompareRunningProcesses(a, b, 0, true);
	});

	return result;
}

static bool IsExclusionInListView(HWND hList, const std::wstring& name)
{
	const int count = ListView_GetItemCount(hList);
	for (int i = 0; i < count; ++i)
	{
		WCHAR buf[260] = {};
		ListView_GetItemText(hList, i, 0, buf, ARRAYSIZE(buf));
		if (_wcsicmp(buf, name.c_str()) == 0)
			return true;
	}
	return false;
}

static void AddExclusionItemToListView(HWND hList, ProcessIconList& iconList, const std::wstring& name)
{
	const int iconIndex = iconList.getOrAddIcon(name);
	LVITEMW item = {};
	item.mask = LVIF_TEXT | LVIF_IMAGE;
	item.iItem = ListView_GetItemCount(hList);
	item.iSubItem = 0;
	item.pszText = const_cast<LPWSTR>(name.c_str());
	item.iImage = iconIndex;
	ListView_InsertItem(hList, &item);
}

static void SetListViewSortArrow(HWND hList, int columnIndex, bool ascending)
{
	HWND hHeader = ListView_GetHeader(hList);
	if (!hHeader)
		return;

	const int colCount = Header_GetItemCount(hHeader);
	for (int i = 0; i < colCount; ++i)
	{
		HDITEMW hdi = {};
		hdi.mask = HDI_FORMAT;
		if (Header_GetItem(hHeader, i, &hdi))
		{
			const UINT oldFmt = hdi.fmt;
			hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
			if (i == columnIndex)
			{
				hdi.fmt |= (ascending ? HDF_SORTUP : HDF_SORTDOWN);
			}
			if (hdi.fmt != oldFmt)
			{
				Header_SetItem(hHeader, i, &hdi);
			}
		}
	}
	InvalidateRect(hHeader, nullptr, TRUE);
}

// --- Select Running Processes Dialog ---

struct SelectProcessResizeHelper
{
	POINT minSize = { 350, 240 };
	RECT initialClient = {};
	RECT rcFilterLabel = {};
	RECT rcFilterEdit = {};
	RECT rcList = {};
	RECT rcOk = {};
	RECT rcCancel = {};
	bool initialized = false;
};

struct SelectProcessDlgState
{
	SelectProcessResizeHelper resizeHelper;
	ProcessIconList iconList;
	std::vector<RunningProcessEntry> allProcesses;
	std::vector<std::wstring>* pSelectedOutputs = nullptr;
	int sortColumn = 0;
	bool sortAscending = true;
};

static void SortRunningProcesses(SelectProcessDlgState* state)
{
	std::sort(state->allProcesses.begin(), state->allProcesses.end(),
		[col = state->sortColumn, asc = state->sortAscending](const RunningProcessEntry& a, const RunningProcessEntry& b) {
			return CompareRunningProcesses(a, b, col, asc);
		});
}

static void PopulateRunningProcessList(HWND hDlg, SelectProcessDlgState* state, const std::wstring& filter)
{
	HWND hList = GetDlgItem(hDlg, IDC_LIST_RUNNING);

	std::set<std::wstring> selectedProcesses;
	int selIdx = -1;
	while ((selIdx = ListView_GetNextItem(hList, selIdx, LVNI_SELECTED)) != -1)
	{
		WCHAR buf[260] = {};
		ListView_GetItemText(hList, selIdx, 0, buf, ARRAYSIZE(buf));
		if (buf[0])
			selectedProcesses.insert(buf);
	}

	SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
	ListView_DeleteAllItems(hList);

	std::wstring lowerFilter = filter;
	std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::towlower);

	int itemIndex = 0;
	for (const auto& proc : state->allProcesses)
	{
		if (!lowerFilter.empty())
		{
			if (proc.lowerExe.find(lowerFilter) == std::wstring::npos &&
				proc.lowerTitle.find(lowerFilter) == std::wstring::npos &&
				proc.lowerPath.find(lowerFilter) == std::wstring::npos)
			{
				continue;
			}
		}

		LVITEMW item = {};
		item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
		item.iItem = itemIndex;
		item.iSubItem = 0;
		item.pszText = const_cast<LPWSTR>(proc.exeName.c_str());
		item.iImage = proc.iconIndex;
		if (selectedProcesses.find(proc.exeName) != selectedProcesses.end())
		{
			item.state = LVIS_SELECTED;
			item.stateMask = LVIS_SELECTED;
		}
		ListView_InsertItem(hList, &item);

		ListView_SetItemText(hList, itemIndex, 1, const_cast<LPWSTR>(proc.windowTitle.c_str()));

		WCHAR pidBuf[16] = {};
		swprintf_s(pidBuf, L"%u", proc.pid);
		ListView_SetItemText(hList, itemIndex, 2, pidBuf);

		ListView_SetItemText(hList, itemIndex, 3, const_cast<LPWSTR>(proc.fullPath.c_str()));

		++itemIndex;
	}

	SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(hList, nullptr, TRUE);
}

INT_PTR CALLBACK SelectRunningProcessesDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		auto* state = new SelectProcessDlgState();
		state->pSelectedOutputs = reinterpret_cast<std::vector<std::wstring>*>(lParam);
		SetWindowLongPtr(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(state));

		RECT rcWindow = {};
		GetWindowRect(hDlg, &rcWindow);
		state->resizeHelper.minSize.x = rcWindow.right - rcWindow.left;
		state->resizeHelper.minSize.y = rcWindow.bottom - rcWindow.top;
		GetClientRect(hDlg, &state->resizeHelper.initialClient);

		auto getControlRect = [hDlg](int id, RECT& rc) {
			HWND hCtrl = GetDlgItem(hDlg, id);
			if (hCtrl)
			{
				GetWindowRect(hCtrl, &rc);
				MapWindowPoints(nullptr, hDlg, reinterpret_cast<LPPOINT>(&rc), 2);
			}
		};
		getControlRect(IDC_STATIC, state->resizeHelper.rcFilterLabel);
		getControlRect(IDC_EDIT_FILTER, state->resizeHelper.rcFilterEdit);
		getControlRect(IDC_LIST_RUNNING, state->resizeHelper.rcList);
		getControlRect(IDOK, state->resizeHelper.rcOk);
		getControlRect(IDCANCEL, state->resizeHelper.rcCancel);
		state->resizeHelper.initialized = true;

		RestoreDialogPosition(hDlg, L"SelectProcessPos", true);

		HWND hList = GetDlgItem(hDlg, IDC_LIST_RUNNING);
		ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		ListView_SetImageList(hList, state->iconList.handle(), LVSIL_SMALL);

		LVCOLUMNW col = {};
		col.mask = LVCF_TEXT | LVCF_WIDTH;
		col.pszText = const_cast<LPWSTR>(L"Process");
		col.cx = 130;
		ListView_InsertColumn(hList, 0, &col);

		col.pszText = const_cast<LPWSTR>(L"Window Title");
		col.cx = 160;
		ListView_InsertColumn(hList, 1, &col);

		col.pszText = const_cast<LPWSTR>(L"PID");
		col.cx = 50;
		ListView_InsertColumn(hList, 2, &col);

		col.pszText = const_cast<LPWSTR>(L"Path");
		col.cx = 200;
		ListView_InsertColumn(hList, 3, &col);

		state->allProcesses = EnumerateRunningProcesses(state->iconList);
		state->sortColumn = 0;
		state->sortAscending = true;
		SetListViewSortArrow(hList, state->sortColumn, state->sortAscending);
		PopulateRunningProcessList(hDlg, state, L"");

		SetFocus(GetDlgItem(hDlg, IDC_EDIT_FILTER));
		return (INT_PTR)FALSE;
	}

	case WM_GETMINMAXINFO:
	{
		auto* state = reinterpret_cast<SelectProcessDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
		if (state && state->resizeHelper.initialized)
		{
			auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
			mmi->ptMinTrackSize = state->resizeHelper.minSize;
			return (INT_PTR)TRUE;
		}
		break;
	}

	case WM_SIZE:
	{
		if (wParam == SIZE_MINIMIZED)
			break;
		auto* state = reinterpret_cast<SelectProcessDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
		if (!state || !state->resizeHelper.initialized)
			break;

		const int newCx = LOWORD(lParam);
		const int newCy = HIWORD(lParam);
		const int deltaX = newCx - state->resizeHelper.initialClient.right;
		const int deltaY = newCy - state->resizeHelper.initialClient.bottom;

		HWND hFilterEdit = GetDlgItem(hDlg, IDC_EDIT_FILTER);
		HWND hList = GetDlgItem(hDlg, IDC_LIST_RUNNING);
		HWND hOk = GetDlgItem(hDlg, IDOK);
		HWND hCancel = GetDlgItem(hDlg, IDCANCEL);

		const int editW = (state->resizeHelper.rcFilterEdit.right - state->resizeHelper.rcFilterEdit.left) + deltaX;
		const int editH = state->resizeHelper.rcFilterEdit.bottom - state->resizeHelper.rcFilterEdit.top;

		const int okW = state->resizeHelper.rcOk.right - state->resizeHelper.rcOk.left;
		const int okH = state->resizeHelper.rcOk.bottom - state->resizeHelper.rcOk.top;
		const int cancelW = state->resizeHelper.rcCancel.right - state->resizeHelper.rcCancel.left;
		const int cancelH = state->resizeHelper.rcCancel.bottom - state->resizeHelper.rcCancel.top;
		const int okX = state->resizeHelper.rcOk.left + deltaX;
		const int okY = state->resizeHelper.rcOk.top + deltaY;
		const int cancelX = state->resizeHelper.rcCancel.left + deltaX;
		const int cancelY = state->resizeHelper.rcCancel.top + deltaY;

		const int listW = (state->resizeHelper.rcList.right - state->resizeHelper.rcList.left) + deltaX;
		const int listH = (state->resizeHelper.rcList.bottom - state->resizeHelper.rcList.top) + deltaY;

		HDWP hdwp = BeginDeferWindowPos(4);
		if (hdwp)
		{
			hdwp = DeferWindowPos(hdwp, hFilterEdit, nullptr, state->resizeHelper.rcFilterEdit.left,
				state->resizeHelper.rcFilterEdit.top, editW, editH, SWP_NOZORDER | SWP_NOACTIVATE);
			hdwp = DeferWindowPos(hdwp, hList, nullptr, state->resizeHelper.rcList.left,
				state->resizeHelper.rcList.top, listW, listH, SWP_NOZORDER | SWP_NOACTIVATE);
			hdwp = DeferWindowPos(hdwp, hOk, nullptr, okX, okY, okW, okH, SWP_NOZORDER | SWP_NOACTIVATE);
			hdwp = DeferWindowPos(hdwp, hCancel, nullptr, cancelX, cancelY, cancelW, cancelH, SWP_NOZORDER | SWP_NOACTIVATE);
			EndDeferWindowPos(hdwp);
		}

		const int extraCol = (deltaX > 0) ? (deltaX / 2) : 0;
		ListView_SetColumnWidth(hList, 0, 130);
		ListView_SetColumnWidth(hList, 1, 160 + extraCol);
		ListView_SetColumnWidth(hList, 2, 50);
		ListView_SetColumnWidth(hList, 3, 200 + extraCol);

		InvalidateRect(hDlg, nullptr, TRUE);
		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
	{
		auto* state = reinterpret_cast<SelectProcessDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
		if (LOWORD(wParam) == IDC_EDIT_FILTER && HIWORD(wParam) == EN_CHANGE && state)
		{
			WCHAR filterBuf[260] = {};
			GetWindowTextW(GetDlgItem(hDlg, IDC_EDIT_FILTER), filterBuf, ARRAYSIZE(filterBuf));
			PopulateRunningProcessList(hDlg, state, filterBuf);
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDOK && state)
		{
			HWND hList = GetDlgItem(hDlg, IDC_LIST_RUNNING);
			int idx = -1;
			while ((idx = ListView_GetNextItem(hList, idx, LVNI_SELECTED)) != -1)
			{
				WCHAR buf[260] = {};
				ListView_GetItemText(hList, idx, 0, buf, ARRAYSIZE(buf));
				if (buf[0] && state->pSelectedOutputs)
					state->pSelectedOutputs->push_back(buf);
			}
			SaveDialogPosition(hDlg, L"SelectProcessPos", true);
			EndDialog(hDlg, IDOK);
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			SaveDialogPosition(hDlg, L"SelectProcessPos", true);
			EndDialog(hDlg, IDCANCEL);
			return (INT_PTR)TRUE;
		}
		break;
	}

	case WM_NOTIFY:
	{
		auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
		if (nmhdr->idFrom == IDC_LIST_RUNNING)
		{
			if (nmhdr->code == LVN_COLUMNCLICK)
			{
				auto* state = reinterpret_cast<SelectProcessDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
				if (state)
				{
					auto* nmlv = reinterpret_cast<NMLISTVIEW*>(lParam);
					const int col = nmlv->iSubItem;
					if (state->sortColumn == col)
					{
						state->sortAscending = !state->sortAscending;
					}
					else
					{
						state->sortColumn = col;
						state->sortAscending = true;
					}
					SortRunningProcesses(state);
					SetListViewSortArrow(nmhdr->hwndFrom, state->sortColumn, state->sortAscending);

					WCHAR filterBuf[260] = {};
					GetWindowTextW(GetDlgItem(hDlg, IDC_EDIT_FILTER), filterBuf, ARRAYSIZE(filterBuf));
					PopulateRunningProcessList(hDlg, state, filterBuf);
				}
				return (INT_PTR)TRUE;
			}
			else if (nmhdr->code == NM_DBLCLK)
			{
				auto* nmia = reinterpret_cast<NMITEMACTIVATE*>(lParam);
				if (nmia->iItem >= 0)
				{
					SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
					return (INT_PTR)TRUE;
				}
			}
		}
		break;
	}

	case WM_DESTROY:
	{
		auto* state = reinterpret_cast<SelectProcessDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
		if (state)
		{
			delete state;
			SetWindowLongPtr(hDlg, DWLP_USER, 0);
		}
		return (INT_PTR)TRUE;
	}
	}
	return (INT_PTR)FALSE;
}

// --- Manual Add Exclusion Dialog ---

static WCHAR g_inputExclusionBuf[260];

INT_PTR CALLBACK InputExclusionDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		SetWindowTextW(GetDlgItem(hDlg, IDC_EDIT_EXCLUSION), L"");
		SetFocus(GetDlgItem(hDlg, IDC_EDIT_EXCLUSION));
		return (INT_PTR)FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			GetWindowTextW(GetDlgItem(hDlg, IDC_EDIT_EXCLUSION), g_inputExclusionBuf, ARRAYSIZE(g_inputExclusionBuf));
			PathUnquoteSpacesW(g_inputExclusionBuf);
			{
				const WCHAR* fileName = PathFindFileNameW(g_inputExclusionBuf);
				if (fileName && fileName != g_inputExclusionBuf)
					wcsncpy_s(g_inputExclusionBuf, fileName, _TRUNCATE);
			}
			EndDialog(hDlg, IDOK);
			return (INT_PTR)TRUE;

		case IDCANCEL:
			g_inputExclusionBuf[0] = L'\0';
			EndDialog(hDlg, IDCANCEL);
			return (INT_PTR)TRUE;

		case IDC_BTN_BROWSE:
		{
			WCHAR filePath[MAX_PATH] = L"";
			OPENFILENAMEW ofn = { sizeof(ofn) };
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hDlg;
			ofn.lpstrFilter = L"Executable Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
			ofn.lpstrFile = filePath;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING;
			if (GetOpenFileNameW(&ofn))
			{
				const WCHAR* fileName = PathFindFileNameW(filePath);
				SetWindowTextW(GetDlgItem(hDlg, IDC_EDIT_EXCLUSION), fileName);
			}
			SetFocus(GetDlgItem(hDlg, IDC_EDIT_EXCLUSION));
			return (INT_PTR)TRUE;
		}
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// --- Main Exclusions Dialog ---

struct ExclusionsResizeHelper
{
	POINT minSize = { 350, 220 };
	RECT initialClient = {};
	RECT rcHeader = {};
	RECT rcList = {};
	RECT rcBtnRunning = {};
	RECT rcBtnManual = {};
	RECT rcBtnRemove = {};
	RECT rcOk = {};
	RECT rcCancel = {};
	bool initialized = false;
};

struct ExclusionsDlgState
{
	ExclusionsResizeHelper resizeHelper;
	ProcessIconList iconList;
	int sortColumn = 0;
	bool sortAscending = true;
};

static void RefreshExclusionsList(HWND hList, ProcessIconList& iconList, bool ascending)
{
	std::vector<std::wstring> names = LoadExclusionsFromRegistry();
	std::sort(names.begin(), names.end(), [ascending](const std::wstring& a, const std::wstring& b) {
		const int cmp = _wcsicmp(a.c_str(), b.c_str());
		return ascending ? (cmp < 0) : (cmp > 0);
	});

	ListView_DeleteAllItems(hList);
	for (const std::wstring& name : names)
	{
		AddExclusionItemToListView(hList, iconList, name);
	}
}

static void SortExclusionsListView(HWND hList, ProcessIconList& iconList, bool ascending)
{
	const int count = ListView_GetItemCount(hList);
	if (count <= 1)
		return;

	struct ItemData
	{
		std::wstring name;
		int iconIndex = -1;
		bool selected = false;
	};

	std::vector<ItemData> items;
	items.reserve(count);

	for (int i = 0; i < count; ++i)
	{
		WCHAR buf[260] = {};
		ListView_GetItemText(hList, i, 0, buf, ARRAYSIZE(buf));
		const UINT itemState = ListView_GetItemState(hList, i, LVIS_SELECTED);
		const int icon = iconList.getOrAddIcon(buf);
		items.push_back({ buf, icon, (itemState & LVIS_SELECTED) != 0 });
	}

	std::sort(items.begin(), items.end(), [ascending](const ItemData& a, const ItemData& b) {
		const int cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
		return ascending ? (cmp < 0) : (cmp > 0);
	});

	SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
	ListView_DeleteAllItems(hList);

	for (int i = 0; i < static_cast<int>(items.size()); ++i)
	{
		LVITEMW item = {};
		item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
		item.iItem = i;
		item.iSubItem = 0;
		item.pszText = const_cast<LPWSTR>(items[i].name.c_str());
		item.iImage = items[i].iconIndex;
		if (items[i].selected)
		{
			item.state = LVIS_SELECTED;
			item.stateMask = LVIS_SELECTED;
		}
		ListView_InsertItem(hList, &item);
	}

	SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(hList, nullptr, TRUE);
}

static void SaveExclusionsList(HWND hList)
{
	std::vector<std::wstring> names;
	const int count = ListView_GetItemCount(hList);
	names.reserve(count);
	for (int i = 0; i < count; ++i)
	{
		WCHAR buf[260] = {};
		ListView_GetItemText(hList, i, 0, buf, ARRAYSIZE(buf));
		if (buf[0])
			names.emplace_back(buf);
	}

	SaveExclusionsToRegistry(names);
	PublishExclusions(names);
}

INT_PTR CALLBACK ExclusionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		s_hExclusionsDlg = hDlg;
		auto* state = new ExclusionsDlgState();
		SetWindowLongPtr(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(state));

		RECT rcWindow = {};
		GetWindowRect(hDlg, &rcWindow);
		state->resizeHelper.minSize.x = rcWindow.right - rcWindow.left;
		state->resizeHelper.minSize.y = rcWindow.bottom - rcWindow.top;
		GetClientRect(hDlg, &state->resizeHelper.initialClient);

		auto getControlRect = [hDlg](int id, RECT& rc) {
			HWND hCtrl = GetDlgItem(hDlg, id);
			if (hCtrl)
			{
				GetWindowRect(hCtrl, &rc);
				MapWindowPoints(nullptr, hDlg, reinterpret_cast<LPPOINT>(&rc), 2);
			}
		};
		getControlRect(IDC_STATIC, state->resizeHelper.rcHeader);
		getControlRect(IDC_LIST_EXCLUSIONS, state->resizeHelper.rcList);
		getControlRect(IDC_BTN_ADD_RUNNING, state->resizeHelper.rcBtnRunning);
		getControlRect(IDC_BTN_ADD_MANUAL, state->resizeHelper.rcBtnManual);
		getControlRect(IDC_BTN_REMOVE, state->resizeHelper.rcBtnRemove);
		getControlRect(IDOK, state->resizeHelper.rcOk);
		getControlRect(IDCANCEL, state->resizeHelper.rcCancel);
		state->resizeHelper.initialized = true;

		HWND hList = GetDlgItem(hDlg, IDC_LIST_EXCLUSIONS);
		ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		ListView_SetImageList(hList, state->iconList.handle(), LVSIL_SMALL);

		RECT rcListClient = {};
		GetClientRect(hList, &rcListClient);
		int colWidth = (rcListClient.right - rcListClient.left) - GetSystemMetrics(SM_CXVSCROLL) - 4;
		if (colWidth < 100)
			colWidth = 100;

		LVCOLUMNW col = {};
		col.mask = LVCF_TEXT | LVCF_WIDTH;
		col.pszText = const_cast<LPWSTR>(L"Process");
		col.cx = colWidth;
		ListView_InsertColumn(hList, 0, &col);

		state->sortColumn = 0;
		state->sortAscending = true;
		RefreshExclusionsList(hList, state->iconList, state->sortAscending);
		SetListViewSortArrow(hList, 0, state->sortAscending);
		RestoreDialogPosition(hDlg, L"ExclusionsPos", true);
		return (INT_PTR)TRUE;
	}

	case WM_GETMINMAXINFO:
	{
		auto* state = reinterpret_cast<ExclusionsDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
		if (state && state->resizeHelper.initialized)
		{
			auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
			mmi->ptMinTrackSize = state->resizeHelper.minSize;
			return (INT_PTR)TRUE;
		}
		break;
	}

	case WM_SIZE:
	{
		if (wParam == SIZE_MINIMIZED)
			break;
		auto* state = reinterpret_cast<ExclusionsDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
		if (!state || !state->resizeHelper.initialized)
			break;

		const int newCx = LOWORD(lParam);
		const int newCy = HIWORD(lParam);
		const int deltaX = newCx - state->resizeHelper.initialClient.right;
		const int deltaY = newCy - state->resizeHelper.initialClient.bottom;

		HWND hHeader = GetDlgItem(hDlg, IDC_STATIC);
		HWND hList = GetDlgItem(hDlg, IDC_LIST_EXCLUSIONS);
		HWND hBtnRunning = GetDlgItem(hDlg, IDC_BTN_ADD_RUNNING);
		HWND hBtnManual = GetDlgItem(hDlg, IDC_BTN_ADD_MANUAL);
		HWND hBtnRemove = GetDlgItem(hDlg, IDC_BTN_REMOVE);
		HWND hOk = GetDlgItem(hDlg, IDOK);
		HWND hCancel = GetDlgItem(hDlg, IDCANCEL);

		const int headerW = (state->resizeHelper.rcHeader.right - state->resizeHelper.rcHeader.left) + deltaX;
		SetWindowPos(hHeader, nullptr, 0, 0, headerW, state->resizeHelper.rcHeader.bottom - state->resizeHelper.rcHeader.top,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

		const int btnW = state->resizeHelper.rcBtnRunning.right - state->resizeHelper.rcBtnRunning.left;
		const int btnH = state->resizeHelper.rcBtnRunning.bottom - state->resizeHelper.rcBtnRunning.top;
		const int btnX = state->resizeHelper.rcBtnRunning.left + deltaX;

		const int okW = state->resizeHelper.rcOk.right - state->resizeHelper.rcOk.left;
		const int okH = state->resizeHelper.rcOk.bottom - state->resizeHelper.rcOk.top;
		const int cancelW = state->resizeHelper.rcCancel.right - state->resizeHelper.rcCancel.left;
		const int cancelH = state->resizeHelper.rcCancel.bottom - state->resizeHelper.rcCancel.top;
		const int okX = state->resizeHelper.rcOk.left + deltaX;
		const int okY = state->resizeHelper.rcOk.top + deltaY;
		const int cancelX = state->resizeHelper.rcCancel.left + deltaX;
		const int cancelY = state->resizeHelper.rcCancel.top + deltaY;

		const int listW = (state->resizeHelper.rcList.right - state->resizeHelper.rcList.left) + deltaX;
		const int listH = (state->resizeHelper.rcList.bottom - state->resizeHelper.rcList.top) + deltaY;

		HDWP hdwp = BeginDeferWindowPos(6);
		if (hdwp)
		{
			hdwp = DeferWindowPos(hdwp, hList, nullptr, state->resizeHelper.rcList.left,
				state->resizeHelper.rcList.top, listW, listH, SWP_NOZORDER | SWP_NOACTIVATE);
			hdwp = DeferWindowPos(hdwp, hBtnRunning, nullptr, btnX, state->resizeHelper.rcBtnRunning.top,
				btnW, btnH, SWP_NOZORDER | SWP_NOACTIVATE);
			hdwp = DeferWindowPos(hdwp, hBtnManual, nullptr, btnX, state->resizeHelper.rcBtnManual.top,
				btnW, btnH, SWP_NOZORDER | SWP_NOACTIVATE);
			hdwp = DeferWindowPos(hdwp, hBtnRemove, nullptr, btnX, state->resizeHelper.rcBtnRemove.top,
				btnW, btnH, SWP_NOZORDER | SWP_NOACTIVATE);
			hdwp = DeferWindowPos(hdwp, hOk, nullptr, okX, okY, okW, okH, SWP_NOZORDER | SWP_NOACTIVATE);
			hdwp = DeferWindowPos(hdwp, hCancel, nullptr, cancelX, cancelY, cancelW, cancelH, SWP_NOZORDER | SWP_NOACTIVATE);
			EndDeferWindowPos(hdwp);
		}

		RECT rcListClient = {};
		GetClientRect(hList, &rcListClient);
		int colWidth = (rcListClient.right - rcListClient.left) - GetSystemMetrics(SM_CXVSCROLL) - 4;
		if (colWidth < 100)
			colWidth = 100;
		ListView_SetColumnWidth(hList, 0, colWidth);
		InvalidateRect(hDlg, nullptr, TRUE);
		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
	{
		auto* state = reinterpret_cast<ExclusionsDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
		switch (LOWORD(wParam))
		{
		case IDOK:
			SaveExclusionsList(GetDlgItem(hDlg, IDC_LIST_EXCLUSIONS));
			SaveDialogPosition(hDlg, L"ExclusionsPos", true);
			EndDialog(hDlg, IDOK);
			return (INT_PTR)TRUE;

		case IDCANCEL:
			SaveDialogPosition(hDlg, L"ExclusionsPos", true);
			EndDialog(hDlg, IDCANCEL);
			return (INT_PTR)TRUE;

		case IDC_BTN_ADD_RUNNING:
		{
			std::vector<std::wstring> selected;
			if (DialogBoxParamW(CApp::getInstance().handle(), MAKEINTRESOURCE(IDD_SELECT_PROCESS),
				hDlg, SelectRunningProcessesDlgProc, reinterpret_cast<LPARAM>(&selected)) == IDOK)
			{
				HWND hList = GetDlgItem(hDlg, IDC_LIST_EXCLUSIONS);
				bool added = false;
				for (const auto& name : selected)
				{
					if (!IsExclusionInListView(hList, name) && state)
					{
						AddExclusionItemToListView(hList, state->iconList, name);
						added = true;
					}
				}
				if (added && state)
				{
					SortExclusionsListView(hList, state->iconList, state->sortAscending);
				}
			}
			return (INT_PTR)TRUE;
		}

		case IDC_BTN_ADD_MANUAL:
		{
			g_inputExclusionBuf[0] = L'\0';
			if (DialogBox(CApp::getInstance().handle(), MAKEINTRESOURCE(IDD_INPUT_EXCLUSION),
				hDlg, InputExclusionDlgProc) == IDOK && g_inputExclusionBuf[0])
			{
				HWND hList = GetDlgItem(hDlg, IDC_LIST_EXCLUSIONS);
				if (!IsExclusionInListView(hList, g_inputExclusionBuf) && state)
				{
					AddExclusionItemToListView(hList, state->iconList, g_inputExclusionBuf);
					SortExclusionsListView(hList, state->iconList, state->sortAscending);
				}
			}
			return (INT_PTR)TRUE;
		}

		case IDC_BTN_REMOVE:
		{
			HWND hList = GetDlgItem(hDlg, IDC_LIST_EXCLUSIONS);
			std::vector<int> selected;
			int idx = -1;
			while ((idx = ListView_GetNextItem(hList, idx, LVNI_SELECTED)) != -1)
				selected.push_back(idx);
			for (auto it = selected.rbegin(); it != selected.rend(); ++it)
				ListView_DeleteItem(hList, *it);
			return (INT_PTR)TRUE;
		}
		}
		break;
	}

	case WM_NOTIFY:
	{
		auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
		if (nmhdr->idFrom == IDC_LIST_EXCLUSIONS)
		{
			if (nmhdr->code == LVN_COLUMNCLICK)
			{
				auto* state = reinterpret_cast<ExclusionsDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
				if (state)
				{
					state->sortAscending = !state->sortAscending;
					SortExclusionsListView(nmhdr->hwndFrom, state->iconList, state->sortAscending);
					SetListViewSortArrow(nmhdr->hwndFrom, 0, state->sortAscending);
				}
				return (INT_PTR)TRUE;
			}
			else if (nmhdr->code == LVN_KEYDOWN)
			{
				auto* nmkd = reinterpret_cast<NMLVKEYDOWN*>(lParam);
				if (nmkd->wVKey == VK_DELETE)
				{
					HWND hList = nmhdr->hwndFrom;
					std::vector<int> selected;
					int idx = -1;
					while ((idx = ListView_GetNextItem(hList, idx, LVNI_SELECTED)) != -1)
						selected.push_back(idx);
					for (auto it = selected.rbegin(); it != selected.rend(); ++it)
						ListView_DeleteItem(hList, *it);
					return (INT_PTR)TRUE;
				}
			}
		}
		break;
	}

	case WM_DESTROY:
	{
		s_hExclusionsDlg = nullptr;
		auto* state = reinterpret_cast<ExclusionsDlgState*>(GetWindowLongPtr(hDlg, DWLP_USER));
		if (state)
		{
			delete state;
			SetWindowLongPtr(hDlg, DWLP_USER, 0);
		}
		return (INT_PTR)TRUE;
	}
	}
	return (INT_PTR)FALSE;
}

LRESULT CProcessBase::on_WM_COMMAND(WPARAM wp, LPARAM lp)
{
	int wmId = LOWORD(wp);
	// Parse the menu selections:
	switch (wmId)
	{
	case IDM_ACTIVATE:
		setEnabled(true);
		break;
	case IDM_DEACTIVATE:
		setEnabled(false);
		break;
	case IDM_ABOUT:
		if (s_hAboutDlg != nullptr && ::IsWindow(s_hAboutDlg))
		{
			::SetForegroundWindow(s_hAboutDlg);
			::BringWindowToTop(s_hAboutDlg);
			break;
		}
		DialogBox(CApp::getInstance().handle(), MAKEINTRESOURCE(IDD_ABOUTBOX), m_HWND, AboutDlgProc);
		break;
	case IDM_SETTINGS:
		if (s_hSettingsDlg != nullptr && ::IsWindow(s_hSettingsDlg))
		{
			::SetForegroundWindow(s_hSettingsDlg);
			::BringWindowToTop(s_hSettingsDlg);
			break;
		}
		DialogBox(CApp::getInstance().handle(), MAKEINTRESOURCE(IDD_SETTINGS), m_HWND, SettingsDlgProc);
		break;
	case IDM_EXCLUSIONS:
		if (s_hExclusionsDlg != nullptr && ::IsWindow(s_hExclusionsDlg))
		{
			::SetForegroundWindow(s_hExclusionsDlg);
			::BringWindowToTop(s_hExclusionsDlg);
			break;
		}
		DialogBox(CApp::getInstance().handle(), MAKEINTRESOURCE(IDD_EXCLUSIONS), m_HWND, ExclusionsDlgProc);
		break;
	case IDM_EXIT:
		DestroyWindow(m_HWND);
		break;
	default:
		return false;
	}
	return true;
}


LRESULT CALLBACK ProcessWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return CApp::getInstance().getProcess()->WndProc(hWnd, message, wParam, lParam);
}

ATOM CProcessBase::registerMainWindowClass()
{
	WNDCLASSEXW wcex{ sizeof(WNDCLASSEX) };
	const CApp& app = CApp::getInstance();
	const auto hInstance = app.handle();
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = ProcessWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = HBRUSH(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SHIFTSWITCHUI);
	wcex.lpszClassName = app.getResourceString(IDC_SHIFTSWITCHUI);
	wcex.hIconSm = wcex.hIcon; // LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

HWND CProcessBase::getParentHWnd()
{
	return nullptr;
}
