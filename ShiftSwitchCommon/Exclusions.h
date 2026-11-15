#pragma once

#include <Windows.h>
#include <string>
#include <vector>

// The exclusion list is owned by the UI process. It keeps the list in
// HKCU\Software\ShiftSwitch\Exclude and publishes it into a per-session shared
// memory section. The hook DLL never reads the registry: every copy of it,
// including the copies injected into other processes, reads the shared
// snapshot and refreshes its local copy when the published version changes.

constexpr DWORD EXCLUSIONS_FORMAT_VERSION = 1;
constexpr DWORD EXCLUSIONS_MAX_COUNT = 128;
constexpr DWORD EXCLUSIONS_MAX_NAME = 128;

// Fixed layout, shared between the 32-bit and the 64-bit processes.
struct EXCLUSIONS_SHARED
{
	LONG formatVersion;
	volatile LONG version;
	DWORD count;
	WCHAR names[EXCLUSIONS_MAX_COUNT][EXCLUSIONS_MAX_NAME];
};

// UI side: registry persistence and publishing for the hook DLL.
std::vector<std::wstring> LoadExclusionsFromRegistry();
void SaveExclusionsToRegistry(const std::vector<std::wstring>& names);
void PublishExclusions(const std::vector<std::wstring>& names);
void ClosePublishedExclusions();

// Hook DLL side: reads the published snapshot.
class CExclusionsReader
{
public:
	CExclusionsReader() = default;
	~CExclusionsReader();
	CExclusionsReader(const CExclusionsReader&) = delete;
	CExclusionsReader& operator=(const CExclusionsReader&) = delete;

	// Replaces `names` with the published list when its version changed.
	// Returns true when the list changed.
	bool Refresh(std::vector<std::wstring>& names);

private:
	bool Map();

	HANDLE m_mutex = nullptr;
	HANDLE m_mapping = nullptr;
	const EXCLUSIONS_SHARED* m_shared = nullptr;
	LONG m_version = 0;
	bool m_hasVersion = false;
	DWORD m_lastMapAttempt = 0;
};
