#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <map>
#include "Exclusions.h"

class ProcessExclusionCache
{
public:
	using ProcessHandle = std::unique_ptr<void, decltype(&::CloseHandle)>;

	struct Entry
	{
		ProcessHandle process;
		bool excluded = false;
		ULONGLONG lastUse = 0;
	};

	static ProcessExclusionCache& Instance();

	bool IsProcessExcluded(DWORD processId);
	bool Refresh();
	size_t GetExcludedCount();

private:
	ProcessExclusionCache() = default;
	bool RefreshLocked();
	void TrimCacheLocked();

	std::vector<std::wstring> m_excludedProcesses;
	CExclusionsReader m_exclusionsReader;
	std::mutex m_mutex;
	std::map<DWORD, Entry> m_cache;
	ULONGLONG m_clock = 0;
	static constexpr size_t MAX_CACHED_PROCESSES = 64;
};

bool IsProcessExcluded(DWORD processId);
