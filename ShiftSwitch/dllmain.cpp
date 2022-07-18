// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <tchar.h>
#include <Shlwapi.h>
#include <map>
#include <thread>
#include "../ShiftSwitchUI/CMailslot.h"

HMODULE _hModule = nullptr;
HKL hklEnglish = nullptr;
HKL hklRussian = nullptr;
DWORD dwCurrentProcess = 0;
DWORD dwHookProcess = 0;
HHOOK hook_WH_KEYBOARD = nullptr;
HHOOK hook_WH_GETMESSAGE = nullptr;
UINT WMAPP_SWITCH_LAYOUT = 0;
bool bSkipProcess = false;

struct HOOK_CONTEXT {
	HWND inputHWND;
	DWORD inputThreadId;
	DWORD inputProcessId;
	bool inputIs32bitProcess;
};

#ifdef _DEBUG
CMailslot oChannel(false);
TCHAR szPath[MAX_PATH] = L"";
using namespace std::literals;

void SendEvent(const std::wstring_view& str) {
	oChannel.send(str);
}

static std::map<DWORD, std::wstring> keyNameMap;
std::wstring GetKeyName(const DWORD vkCode) {
	const auto search = keyNameMap.find(vkCode);
	if (search != keyNameMap.end()) {
		return search->second;
	}

	constexpr auto BUF_SIZE = 64;
	std::wstring result;
	TCHAR keyname[BUF_SIZE]{};
	const auto scanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
	const auto value = (scanCode << 16);
	GetKeyNameText(value, keyname, BUF_SIZE);
	result.assign(keyname);
	keyNameMap[vkCode] = result;
	return result;
}

static auto delimiter{ L" : " };
#ifdef 	_WIN64
static auto bits{ L"x64" };
#else
static auto bits{ L"x32" };
#endif

void LogEvent(const DWORD keyCode, const bool down, WORD iRepeatCount) {

	std::wstringstream stream;

	stream << szPath << delimiter << bits << delimiter << ((down) ? L'D' : L'U');
	const auto keyName = GetKeyName(keyCode);
	stream << delimiter << keyName << delimiter << iRepeatCount;
	const auto str = stream.str();
	SendEvent(str);
}

void LogSwitch(const WCHAR* wsSwitch) {
	std::wstringstream stream;
	stream << szPath << delimiter << bits << delimiter << wsSwitch;
	const auto str = stream.str();
	SendEvent(str);
}

#endif // _DEBUG

HMODULE g_hKernel = nullptr;
HMODULE getKernel32Handle()
{
	if (g_hKernel == nullptr)
	{
		g_hKernel = GetModuleHandleW(L"kernel32.dll");
	}
	return g_hKernel;
}



bool checkIsSomeModifKeyDownExcept(const BYTE except) {
	static const BYTE checks[] = {
		VK_LSHIFT , VK_RSHIFT ,//VK_SHIFT ,
		VK_LCONTROL , VK_RCONTROL ,//VK_CONTROL,
		VK_LWIN, VK_RWIN,
		VK_LMENU,VK_RMENU, //VK_MENU
	};
	BYTE kbdState[256];
	constexpr BYTE bDown = 0x80;
	if (GetKeyboardState(kbdState)) {
		for (const auto vkCode : checks)
		{
			if (vkCode != except) {
				if (kbdState[vkCode] == bDown)
					return true;
			}
		}
	}
	return false;
}

void ActivateLayoutEx(const _In_ HKL hkl, const HWND hwnd) {
	auto lambda = [hkl, hwnd] {
		ActivateKeyboardLayout(hkl, KLF_SETFORPROCESS);
		Sleep(100);
		PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_SYSCHARSET, LPARAM(hkl));
		//PostMessageW(HWND_BROADCAST, WM_INPUTLANGCHANGE, 0, LPARAM(hkl));
	};
	std::thread(lambda).detach();

}
void ActivateLayout(const _In_ HKL hkl, const HOOK_CONTEXT& ctx) {
	if (ctx.inputProcessId == dwCurrentProcess)
	{
		ActivateLayoutEx(hkl, ctx.inputHWND);
#ifdef _DEBUG
		LogSwitch(L"ActivateKeyboardLayout");
#endif
	}
	else //if (processID == dwHookProcess)
	{
		PostMessageW(ctx.inputHWND, WMAPP_SWITCH_LAYOUT, reinterpret_cast<WPARAM>(hkl), 0);
#ifdef _DEBUG
		LogSwitch(L"Post WMAPP_SWITCH_LAYOUT");
#endif
	}
}


bool LeftShiftDown = false;
bool RightShiftDown = false;

void Proccess(const DWORD keyCode, const bool down, WORD iRepeatCount, HOOK_CONTEXT& ctx) {
#ifdef _DEBUG
	LogEvent(keyCode, down, iRepeatCount);
#endif // _DEBUG
	if (down) {
		switch (keyCode)
		{
		case VK_LSHIFT:
			RightShiftDown = false;
			if (!checkIsSomeModifKeyDownExcept(VK_LSHIFT)) {
				LeftShiftDown = true;
			}
			break;
		case VK_RSHIFT:
			LeftShiftDown = false;
			if (!checkIsSomeModifKeyDownExcept(VK_RSHIFT)) {
				RightShiftDown = true;
			}
			break;
		default:
			LeftShiftDown = false;
			RightShiftDown = false;
			break;
		}

	}
	else {
		switch (keyCode)
		{
		case VK_LSHIFT:
			if (LeftShiftDown) {
				//Switch to English
				const auto hCurKL = GetKeyboardLayout(ctx.inputThreadId);
				if (hCurKL != hklEnglish) {
					ActivateLayout(hklEnglish, ctx);
#ifdef _DEBUG
					LogSwitch(L"Switch to English");
#endif // _DEBUG
				}
			}
			LeftShiftDown = false;
			break;
		case VK_RSHIFT:
			if (RightShiftDown) {
				//Switch to Russian
				const auto hCurKL = GetKeyboardLayout(ctx.inputThreadId);
				if (hCurKL != hklRussian) {
					ActivateLayout(hklRussian, ctx);
#ifdef _DEBUG
					LogSwitch(L"Switch to Russian");
#endif // _DEBUG
				}
			}
			RightShiftDown = false;
			break;
		default:

			break;
		}
	}

}

typedef BOOL(WINAPI* FN_IsWow64Process)(HANDLE hProcess, PBOOL  Wow64Process);
FN_IsWow64Process lpfn_IsWow64Process = nullptr;
bool bIsWow64Loaded = false;

bool isWow64Process(HANDLE hProcess)
{
	if (!bIsWow64Loaded)
	{
		const auto hKernel = getKernel32Handle();
		if (hKernel != nullptr) {
			lpfn_IsWow64Process = (FN_IsWow64Process)GetProcAddress(hKernel, "IsWow64Process");
		}
		bIsWow64Loaded = true;
	}
	if (lpfn_IsWow64Process != nullptr)
	{
		BOOL bResult = FALSE;
		if (lpfn_IsWow64Process(hProcess, &bResult))
		{
			return bResult;
		}
	}
	return  false;
}

int iSystemBit = 0;

void detectSystemBit() {
	if (iSystemBit != 0) return;
	SYSTEM_INFO sysInfo = {};

	const auto hKernel = getKernel32Handle();
	if (hKernel != nullptr) {
		const auto lpFN_GetNativeSystemInfo = (VOID(WINAPI*)(LPSYSTEM_INFO lpSystemInfo))GetProcAddress(
			hKernel, "GetNativeSystemInfo");
		if (lpFN_GetNativeSystemInfo != nullptr) {
			lpFN_GetNativeSystemInfo(&sysInfo);
			goto test;
		}
	}
	GetSystemInfo(&sysInfo);


test:
	iSystemBit = sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL
		? 32
		: sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
		? 64
		: -1;

}

bool is32BitSystem() {
	detectSystemBit();
	return iSystemBit == 32;
}

bool is64BitSystem() {
	detectSystemBit();
	return iSystemBit == 64;

}

bool is64bitProcess(HANDLE hProcess)
{
	return !isWow64Process(hProcess) && is64BitSystem();
}

bool is32bitProcess(HANDLE hProcess)
{
	return isWow64Process(hProcess) || is32BitSystem();
}

void checkIs32BitWindow(HOOK_CONTEXT& ctx)
{
	if (ctx.inputHWND == nullptr)
	{
		ctx.inputProcessId = GetCurrentProcessId();
		ctx.inputThreadId = GetCurrentThreadId();
	}
	const CHandle process(OpenProcess(PROCESS_QUERY_INFORMATION, false, ctx.inputProcessId));
	if (process.isValid())
	{
		ctx.inputIs32bitProcess = is32bitProcess(process.get());
	}
}

LRESULT CALLBACK KeyboardProc(
	int    code,
	WPARAM wParam,
	LPARAM lParam
)
{
	if (code >= 0 && !bSkipProcess)
	{
		HOOK_CONTEXT ctx = {};
		ctx.inputHWND = ::GetForegroundWindow();
		ctx.inputThreadId = GetWindowThreadProcessId(ctx.inputHWND, &ctx.inputProcessId);
		checkIs32BitWindow(ctx);

#ifdef 	_WIN64
		//If it is 64bit hook dll and we got call from 32bit process - skip it
		if (!ctx.inputIs32bitProcess) {
#else
		//If it is 32bit hook dll and we got call from 64bit process - skip it
		if (ctx.inputIs32bitProcess) {
#endif
			const WORD whlParam = HIWORD(lParam);
			const bool down = (whlParam & KF_UP) == 0;
			//wParam does not allow because it is not contains Left-Right flag
			const UINT scanCode = whlParam & 0x1FF;
			const auto vkCode = MapVirtualKey(scanCode, MAPVK_VSC_TO_VK_EX);

			//bool prev_down = (whlParam & KF_REPEAT) != 0;


			//if (down ^ prev_down) {
			Proccess(vkCode, down, LOWORD(lParam), ctx);
			//}
		}
#ifdef _DEBUG
		else
		{
			LogSwitch(L"Skip for bits");
		}
#endif
	}
	return  CallNextHookEx(nullptr, code, wParam, lParam);
}

void onProcessAttach() {
	hklEnglish = LoadKeyboardLayout(L"00000409", 0);
	hklRussian = LoadKeyboardLayout(L"00000419", 0);

	dwCurrentProcess = GetCurrentProcessId();

	WMAPP_SWITCH_LAYOUT = RegisterWindowMessageW(L"WMAPP_SWITCH_LAYOUT");
	const auto hUser32 = GetModuleHandleW(L"User32");
	if (hUser32 != nullptr)
	{
		typedef BOOL(WINAPI* FN_ChangeWindowMessageFilter)(UINT, DWORD);
		const auto lpfn_ChangeWindowMessageFilter = reinterpret_cast<FN_ChangeWindowMessageFilter>(GetProcAddress(hUser32, "ChangeWindowMessageFilter"));
		if (lpfn_ChangeWindowMessageFilter != nullptr)
		{
			lpfn_ChangeWindowMessageFilter(WMAPP_SWITCH_LAYOUT, MSGFLT_ADD);
		}
	}

#ifdef _DEBUG

	szPath[GetModuleFileName(nullptr, szPath, MAX_PATH)] = 0;
	//PathStripPath(szPath);
	std::wstringstream stream;
	stream << L"Attach: " << szPath;
	SendEvent(stream.str());

	//if (StrStrIW(szPath, L"devenv.exe") != nullptr || StrStrIW(szPath, L"msvsmon.exe") != nullptr)
	//{
	//	bSkipProcess = true;
	//	SendEvent(L"SkipProcess"sv);
	//}
#endif // _DEBUG


}

void onProcessDetach() {
#ifdef _DEBUG
	std::wstringstream stream;
	stream << L"Detach: " << szPath;
	SendEvent(stream.str());
	oChannel.close();
#endif // _DEBUG
}


BOOL APIENTRY DllMain(
	HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
) {
	_hModule = hModule;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		onProcessAttach();
		break;
	case DLL_PROCESS_DETACH:
		onProcessDetach();
		break;
	case DLL_THREAD_DETACH:
	case DLL_THREAD_ATTACH:
		break;
	}
	return TRUE;
}

void _unhookWindowsHook(HHOOK & hook)
{
	if (hook != nullptr) {
		UnhookWindowsHookEx(hook);
		hook = nullptr;
	}
}


LRESULT CALLBACK GetMsgProc(
	_In_ int    code,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
) {
	if (code >= 0 && !bSkipProcess)
	{
		const MSG* lpMsg = reinterpret_cast<MSG*>(lParam);
		if (lpMsg->message == WMAPP_SWITCH_LAYOUT)
		{
			const auto hkl = reinterpret_cast<HKL>(lpMsg->wParam);
			ActivateLayoutEx(hkl, lpMsg->hwnd);
#ifdef _DEBUG
			LogSwitch(L"Process WMAPP_SWITCH_LAYOUT");
#endif 
		}
	}
	return  CallNextHookEx(nullptr, code, wParam, lParam);
}


BOOL APIENTRY SetHook(BOOL bSet)
{
#ifdef _DEBUG
	std::wstringstream stream;
#endif // _DEBUG
	if (bSet) {
#ifdef _DEBUG
		stream << L"SetHook: " << szPath;
#endif // _DEBUG
		dwHookProcess = GetCurrentProcessId();

		hook_WH_KEYBOARD = SetWindowsHookEx(
			WH_KEYBOARD,
			KeyboardProc,
			_hModule,
			0
		);

		if (hook_WH_KEYBOARD == nullptr) {
#ifdef _DEBUG
			stream << L"Hook WH_KEYBOARD not set";
#endif
			return false;
		}
		else
		{
			hook_WH_GETMESSAGE = SetWindowsHookEx(
				WH_GETMESSAGE,
				GetMsgProc,
				_hModule,
				0
			);
			if (hook_WH_GETMESSAGE == nullptr) {
#ifdef _DEBUG
				stream << L"Hook WH_GETMESSAGE not set";
#endif			
				return false;
			}
		}

	}
	else
	{
#ifdef _DEBUG
		stream << L"UnsetHook: " << szPath;
#endif // _DEBUG
		_unhookWindowsHook(hook_WH_GETMESSAGE);
		_unhookWindowsHook(hook_WH_KEYBOARD);
	}

#ifdef _DEBUG
	SendEvent(stream.str());
#endif // _DEBUG
	return true;
}
