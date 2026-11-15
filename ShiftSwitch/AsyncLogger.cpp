#include "pch.h"
#include "AsyncLogger.h"
#include "CMailslot.h"
#include <atomic>
#include <map>
#include <sstream>
#include <vector>
#include <cstdarg>
#include <cstdio>

static wchar_t g_dllPath[MAX_PATH] = L"";

void LogInit(const wchar_t* dllPath)
{
	if (dllPath != nullptr)
	{
		wcscpy_s(g_dllPath, dllPath);
	}
}

void LogOutput(const wchar_t* format, ...)
{
	wchar_t buffer[512];
	va_list args;
	va_start(args, format);
	vswprintf_s(buffer, format, args);
	va_end(args);
	OutputDebugStringW(buffer);
	OutputDebugStringW(L"\n");
}

static const int LOG_BUFFER_SIZE = 2048; // Power of 2
static const int LOG_ENTRY_MAX = 512;    // Max chars per entry

struct LogEntry {
	wchar_t text[LOG_ENTRY_MAX];
	int len;
};

static LogEntry g_logBuffer[LOG_BUFFER_SIZE];
static std::atomic<int> g_logHead{0};
static std::atomic<int> g_logTail{0};
static HANDLE g_logThread = nullptr;
static std::atomic<bool> g_logThreadRunning{false};

void LogSwitchAsync(const wchar_t* wsSwitch)
{
	wchar_t msg[LOG_ENTRY_MAX];
	wcscpy_s(msg, g_dllPath);
	wcscat_s(msg, L" : ");
#ifdef _WIN64
	wcscat_s(msg, L"x64");
#else
	wcscat_s(msg, L"x32");
#endif
	wcscat_s(msg, L" : ");
	wcscat_s(msg, wsSwitch);

	int head = g_logHead.load(std::memory_order_relaxed);
	int nextHead = (head + 1) & (LOG_BUFFER_SIZE - 1);
	if (nextHead != g_logTail.load(std::memory_order_acquire)) {
		wcsncpy_s(g_logBuffer[head].text, LOG_ENTRY_MAX, msg, _TRUNCATE);
		g_logBuffer[head].len = static_cast<int>(wcslen(msg));
		g_logHead.store(nextHead, std::memory_order_release);
	}
}

static DWORD WINAPI LogThreadProc(LPVOID param)
{
	wchar_t localBuf[LOG_ENTRY_MAX];
	auto* mailslot = static_cast<CMailslot*>(param);
	while (g_logThreadRunning.load(std::memory_order_relaxed)) {
		int tail = g_logTail.load(std::memory_order_relaxed);
		int head = g_logHead.load(std::memory_order_acquire);
		if (tail == head) {
			Sleep(10);
			continue;
		}
		wcsncpy_s(localBuf, LOG_ENTRY_MAX, g_logBuffer[tail].text, g_logBuffer[tail].len);
		localBuf[g_logBuffer[tail].len] = L'\0';
		g_logTail.store((tail + 1) & (LOG_BUFFER_SIZE - 1), std::memory_order_release);
		
		if (mailslot && mailslot->isValid()) {
			DWORD written;
			WriteFile(mailslot->get(), localBuf, static_cast<DWORD>((g_logBuffer[tail].len + 1) * sizeof(wchar_t)), &written, nullptr);
		}
	}
	delete mailslot;
	return 0;
}

void LogThreadStart()
{
	if (g_logThreadRunning.exchange(true)) return;
	auto* mailslot = new CMailslot(false);
	g_logThread = CreateThread(nullptr, 0, LogThreadProc, mailslot, 0, nullptr);
}

void LogThreadStop()
{
	if (!g_logThreadRunning.exchange(false)) return;
	if (g_logThread) {
		WaitForSingleObject(g_logThread, 2000);
		CloseHandle(g_logThread);
		g_logThread = nullptr;
	}
}

#ifdef _DEBUG
static CMailslot oChannel(false);
static TCHAR szPath[MAX_PATH] = L"";

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
#ifdef _WIN64
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
#endif
