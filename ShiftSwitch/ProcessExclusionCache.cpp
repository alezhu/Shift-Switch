#include "pch.h"
#include "ProcessExclusionCache.h"
#include "AsyncLogger.h"
#include <algorithm>
#include <cwctype>

ProcessExclusionCache& ProcessExclusionCache::Instance()
{
	static ProcessExclusionCache instance;
	return instance;
}

bool ProcessExclusionCache::Refresh()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return RefreshLocked();
}

size_t ProcessExclusionCache::GetExcludedCount()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_excludedProcesses.size();
}

bool ProcessExclusionCache::RefreshLocked()
{
	std::vector<std::wstring> names;
	if (!m_exclusionsReader.Refresh(names))
		return false;

	m_excludedProcesses.swap(names);
	m_cache.clear();
	LOG_INFO(L"Exclusion list updated, %zu entries", m_excludedProcesses.size());
	return true;
}

void ProcessExclusionCache::TrimCacheLocked()
{
	if (m_cache.size() < MAX_CACHED_PROCESSES)
		return;

	for (auto it = m_cache.begin(); it != m_cache.end();)
	{
		if (WaitForSingleObject(it->second.process.get(), 0) != WAIT_TIMEOUT)
			it = m_cache.erase(it);
		else
			++it;
	}

	while (m_cache.size() >= MAX_CACHED_PROCESSES)
	{
		auto oldest = m_cache.begin();
		for (auto it = std::next(oldest); it != m_cache.end(); ++it)
		{
			if (it->second.lastUse < oldest->second.lastUse)
				oldest = it;
		}
		m_cache.erase(oldest);
	}
}

bool ProcessExclusionCache::IsProcessExcluded(DWORD processId)
{
	if (processId == 0)
		return false;

	std::lock_guard<std::mutex> lock(m_mutex);
	RefreshLocked();
	if (m_excludedProcesses.empty())
		return false;

	const ULONGLONG now = ++m_clock;
	const auto cached = m_cache.find(processId);
	if (cached != m_cache.end())
	{
		if (WaitForSingleObject(cached->second.process.get(), 0) == WAIT_TIMEOUT)
		{
			cached->second.lastUse = now;
			return cached->second.excluded;
		}
		m_cache.erase(cached);
	}

	const HANDLE rawProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, processId);
	if (rawProcess == nullptr)
		return false;
	ProcessHandle process(rawProcess, &::CloseHandle);

	WCHAR path[MAX_PATH];
	DWORD len = MAX_PATH;
	if (!QueryFullProcessImageNameW(process.get(), 0, path, &len))
		return false;

	const WCHAR* exeName = wcsrchr(path, L'\\');
	exeName = exeName ? exeName + 1 : path;

	std::wstring lowerName(exeName);
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

	const bool excluded = std::find(m_excludedProcesses.begin(), m_excludedProcesses.end(), lowerName)
		!= m_excludedProcesses.end();

	TrimCacheLocked();
	m_cache.emplace(processId, Entry{ std::move(process), excluded, now });
	return excluded;
}

bool IsProcessExcluded(DWORD processId)
{
	return ProcessExclusionCache::Instance().IsProcessExcluded(processId);
}
