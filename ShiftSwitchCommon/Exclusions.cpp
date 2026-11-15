#include "pch.h"
#include "Exclusions.h"
#include "CHandle.h"
#include "ScopeExit.h"
#include "ScopedResource.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>

namespace
{
constexpr WCHAR EXCLUSIONS_KEY[] = L"Software\\ShiftSwitch";
constexpr WCHAR EXCLUSIONS_VALUE[] = L"Exclude";
constexpr WCHAR EXCLUSIONS_MAPPING_NAME[] = L"Local\\ShiftSwitch.Exclusions";
constexpr WCHAR EXCLUSIONS_MUTEX_NAME[] = L"Local\\ShiftSwitch.ExclusionsMutex";

void ToLowerCase(std::wstring& name)
{
	std::transform(name.begin(), name.end(), name.begin(), ::towlower);
}

LONG ReadSharedVersion(const EXCLUSIONS_SHARED* shared)
{
	// The view is mapped read-only, so an interlocked read that would write the
	// comparand back is not an option. A volatile load is enough: the writer
	// bumps the version after the data, and the value is re-checked under the
	// mutex before the list is copied.
	return *reinterpret_cast<const volatile LONG*>(&shared->version);
}
}

std::vector<std::wstring> LoadExclusionsFromRegistry()
{
	std::vector<std::wstring> names;

	ScopedHKey hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, EXCLUSIONS_KEY, 0, KEY_READ, hKey.addressof()) != ERROR_SUCCESS)
		return names;

	DWORD type = 0;
	DWORD size = 0;
	if (RegQueryValueExW(hKey, EXCLUSIONS_VALUE, nullptr, &type, nullptr, &size) == ERROR_SUCCESS
		&& type == REG_MULTI_SZ && size > 0)
	{
		std::vector<BYTE> buffer(size);
		if (RegQueryValueExW(hKey, EXCLUSIONS_VALUE, nullptr, &type, buffer.data(), &size) == ERROR_SUCCESS)
		{
			const WCHAR* current = reinterpret_cast<const WCHAR*>(buffer.data());
			const WCHAR* end = current + size / sizeof(WCHAR);
			while (current < end && *current != L'\0')
			{
				names.emplace_back(current);
				current += wcslen(current) + 1;
			}
		}
	}

	return names;
}

void SaveExclusionsToRegistry(const std::vector<std::wstring>& names)
{
	ScopedHKey hKey;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, EXCLUSIONS_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, hKey.addressof(), nullptr) != ERROR_SUCCESS)
		return;

	if (names.empty())
	{
		RegDeleteValueW(hKey, EXCLUSIONS_VALUE);
		return;
	}

	std::vector<WCHAR> buffer;
	buffer.reserve(names.size() * 16 + 2);
	for (const std::wstring& name : names)
	{
		buffer.insert(buffer.end(), name.begin(), name.end());
		buffer.push_back(L'\0');
	}
	buffer.push_back(L'\0');

	RegSetValueExW(hKey, EXCLUSIONS_VALUE, 0, REG_MULTI_SZ,
		reinterpret_cast<const BYTE*>(buffer.data()),
		static_cast<DWORD>(buffer.size() * sizeof(WCHAR)));
}

static CHandle g_publishedMutex;
static CHandle g_publishedMapping;

void ClosePublishedExclusions()
{
	g_publishedMapping.close();
	g_publishedMutex.close();
}

void PublishExclusions(const std::vector<std::wstring>& names)
{
	if (!g_publishedMutex.isValid())
	{
		g_publishedMutex.reset(CreateMutexW(nullptr, FALSE, EXCLUSIONS_MUTEX_NAME));
		if (!g_publishedMutex.isValid())
			return;
	}

	const DWORD wait = WaitForSingleObject(g_publishedMutex, 2000);
	if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
		return;

	ScopeExit mutexReleaseGuard([&] {
		ReleaseMutex(g_publishedMutex);
	});

	if (!g_publishedMapping.isValid())
	{
		g_publishedMapping.reset(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
			0, sizeof(EXCLUSIONS_SHARED), EXCLUSIONS_MAPPING_NAME));
	}

	if (g_publishedMapping.isValid())
	{
		auto* shared = static_cast<EXCLUSIONS_SHARED*>(
			MapViewOfFile(g_publishedMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(EXCLUSIONS_SHARED)));
		if (shared != nullptr)
		{
			ScopeExit viewGuard([&] {
				UnmapViewOfFile(shared);
			});

			shared->formatVersion = EXCLUSIONS_FORMAT_VERSION;

			// The list is stored as exe names in lowercase, the same form the
			// hook DLL uses when it compares the focused process.
			const DWORD count = min(static_cast<DWORD>(names.size()), EXCLUSIONS_MAX_COUNT);
			shared->count = count;
			for (DWORD i = 0; i < count; ++i)
			{
				std::wstring name = names[i];
				ToLowerCase(name);
				wcsncpy_s(shared->names[i], EXCLUSIONS_MAX_NAME, name.c_str(), _TRUNCATE);
			}

			MemoryBarrier();
			InterlockedIncrement(&shared->version);
		}
	}
}

CExclusionsReader::~CExclusionsReader()
{
	if (m_shared != nullptr)
		UnmapViewOfFile(m_shared);
	if (m_mapping != nullptr)
		CloseHandle(m_mapping);
	if (m_mutex != nullptr)
		CloseHandle(m_mutex);
}

bool CExclusionsReader::Map()
{
	if (m_shared != nullptr)
		return true;

	// The section appears when the UI process starts. Retry at most once per
	// second, so a missing section does not cost a syscall per key press.
	const DWORD now = GetTickCount();
	if (m_lastMapAttempt != 0 && now - m_lastMapAttempt < 1000)
		return false;
	m_lastMapAttempt = now != 0 ? now : 1;

	CHandle mutex(OpenMutexW(SYNCHRONIZE, FALSE, EXCLUSIONS_MUTEX_NAME));
	if (!mutex.isValid())
		return false;

	CHandle mapping(OpenFileMappingW(FILE_MAP_READ, FALSE, EXCLUSIONS_MAPPING_NAME));
	if (!mapping.isValid())
		return false;

	const auto* shared = static_cast<const EXCLUSIONS_SHARED*>(
		MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(EXCLUSIONS_SHARED)));
	if (shared == nullptr || shared->formatVersion != EXCLUSIONS_FORMAT_VERSION)
	{
		if (shared != nullptr)
			UnmapViewOfFile(shared);
		return false;
	}

	m_mutex = mutex.release();
	m_mapping = mapping.release();
	m_shared = shared;
	return true;
}

bool CExclusionsReader::Refresh(std::vector<std::wstring>& names)
{
	if (!Map())
		return false;

	// Common case: nothing was published since the previous call. This check is
	// a plain load, so the hot path of the hook does not take the mutex.
	const LONG version = ReadSharedVersion(m_shared);
	if (m_hasVersion && version == m_version)
		return false;

	// A new version is worth the mutex, but only a try: if the UI is rewriting
	// the list right now, the next key press picks the list up instead.
	if (WaitForSingleObject(m_mutex, 0) != WAIT_OBJECT_0)
		return false;

	ScopeExit mutexReleaseGuard([this] {
		ReleaseMutex(m_mutex);
	});

	bool changed = false;
	const LONG publishedVersion = m_shared->version;
	if (!m_hasVersion || publishedVersion != m_version)
	{
		const DWORD count = min(m_shared->count, EXCLUSIONS_MAX_COUNT);
		names.clear();
		names.reserve(count);
		for (DWORD i = 0; i < count; ++i)
		{
			const WCHAR* name = m_shared->names[i];
			names.emplace_back(name, wcsnlen(name, EXCLUSIONS_MAX_NAME));
		}
		m_version = publishedVersion;
		m_hasVersion = true;
		changed = true;
	}

	return changed;
}
