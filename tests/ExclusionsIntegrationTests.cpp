#include "TestHarness.h"
#include "ExclusionsIntegrationTests.h"
#include "Exclusions.h"
#include "ScopeExit.h"
#include "CHandle.h"

#include <windows.h>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>

namespace
{
	// Test 1: Registry round-trip persistence (SaveExclusionsToRegistry, LoadExclusionsFromRegistry)
	void testRegistryPersistenceRoundTrip()
	{
		// Backup original registry state and restore at scope exit
		const std::vector<std::wstring> original = LoadExclusionsFromRegistry();
		ScopeExit registryRestoreGuard([&] {
			SaveExclusionsToRegistry(original);
		});

		// 1.1: Save and load list of multiple items
		const std::vector<std::wstring> sample = {
			L"notepad.exe",
			L"TheBrain.exe",
			L"HitFilmExpress.exe",
			L"app with spaces.exe"
		};
		SaveExclusionsToRegistry(sample);

		const std::vector<std::wstring> loaded = LoadExclusionsFromRegistry();
		CHECK_EQUAL(loaded.size(), sample.size());
		for (size_t i = 0; i < sample.size() && i < loaded.size(); ++i)
		{
			CHECK(loaded[i] == sample[i]);
		}

		// 1.2: Save empty list -> verify value is deleted from registry
		SaveExclusionsToRegistry({});
		const std::vector<std::wstring> emptyLoaded = LoadExclusionsFromRegistry();
		CHECK_TRUE(emptyLoaded.empty());

		HKEY hKey = nullptr;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ShiftSwitch", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			DWORD type = 0;
			const LONG queryResult = RegQueryValueExW(hKey, L"Exclude", nullptr, &type, nullptr, nullptr);
			CHECK_EQUAL(queryResult, ERROR_FILE_NOT_FOUND);
			RegCloseKey(hKey);
		}

		// 1.3: Save single item
		const std::vector<std::wstring> single = { L"single_target.exe" };
		SaveExclusionsToRegistry(single);
		const std::vector<std::wstring> singleLoaded = LoadExclusionsFromRegistry();
		CHECK_EQUAL(singleLoaded.size(), 1);
		if (!singleLoaded.empty())
		{
			CHECK(singleLoaded[0] == L"single_target.exe");
		}
	}

	// Test 2: Publishing to shared memory and initial refresh by reader
	void testPublishAndRefreshBasic()
	{
		const std::vector<std::wstring> sample = {
			L"Notepad.EXE",
			L"Code.exe",
			L"TheBrain.EXE"
		};
		PublishExclusions(sample);

		CExclusionsReader reader;
		std::vector<std::wstring> readList;

		// First refresh must detect changes and populate lowercase names
		const bool initialRefresh = reader.Refresh(readList);
		CHECK_TRUE(initialRefresh);
		CHECK_EQUAL(readList.size(), sample.size());

		// Verify lowercase normalization (hook DLL compares case-insensitively using lowercase)
		if (readList.size() == 3)
		{
			CHECK(readList[0] == L"notepad.exe");
			CHECK(readList[1] == L"code.exe");
			CHECK(readList[2] == L"thebrain.exe");
		}

		// Fast path: subsequent refresh without publish returns false and keeps data
		const bool secondRefresh = reader.Refresh(readList);
		CHECK_FALSE(secondRefresh);
		CHECK_EQUAL(readList.size(), 3);
	}

	// Test 3: Dynamic publishing updates picked up by reader
	void testPublishDynamicUpdate()
	{
		CExclusionsReader reader;
		std::vector<std::wstring> readList;

		// Initial list
		PublishExclusions({ L"app1.exe", L"app2.exe" });
		CHECK_TRUE(reader.Refresh(readList));
		CHECK_EQUAL(readList.size(), 2);

		// Dynamic update: new application added
		PublishExclusions({ L"app1.exe", L"app2.exe", L"app3.exe" });
		const bool updatedRefresh = reader.Refresh(readList);
		CHECK_TRUE(updatedRefresh);
		CHECK_EQUAL(readList.size(), 3);
		if (readList.size() == 3)
		{
			CHECK(readList[2] == L"app3.exe");
		}

		// Dynamic update: applications cleared
		PublishExclusions({});
		const bool clearedRefresh = reader.Refresh(readList);
		CHECK_TRUE(clearedRefresh);
		CHECK_TRUE(readList.empty());

		// Fast path on empty list
		CHECK_FALSE(reader.Refresh(readList));
	}

	// Test 4: Capacity boundaries (EXCLUSIONS_MAX_COUNT and EXCLUSIONS_MAX_NAME)
	void testPublishCapacityBoundaries()
	{
		CExclusionsReader reader;
		std::vector<std::wstring> readList;

		// 4.1: Test exceeding EXCLUSIONS_MAX_COUNT (128)
		std::vector<std::wstring> overflowList;
		overflowList.reserve(150);
		for (int i = 0; i < 150; ++i)
		{
			overflowList.push_back(L"app_" + std::to_wstring(i) + L".exe");
		}

		PublishExclusions(overflowList);
		CHECK_TRUE(reader.Refresh(readList));
		CHECK_EQUAL(readList.size(), EXCLUSIONS_MAX_COUNT); // Clamped to 128
		if (!readList.empty())
		{
			CHECK(readList.front() == L"app_0.exe");
			CHECK(readList.back() == L"app_127.exe");
		}

		// 4.2: Test very long process name (> 128 characters)
		const std::wstring longName(200, L'x');
		PublishExclusions({ longName });
		CHECK_TRUE(reader.Refresh(readList));
		CHECK_EQUAL(readList.size(), 1);
		if (!readList.empty())
		{
			CHECK(readList[0].length() < EXCLUSIONS_MAX_NAME);
			CHECK(readList[0].length() == EXCLUSIONS_MAX_NAME - 1);
		}
	}

	// Test 5: Multi-threaded concurrent readers with active updates
	void testConcurrentReadersMultiThreaded()
	{
		std::atomic<bool> stopWorkers{ false };
		std::atomic<int> successfulRefreshes{ 0 };

		constexpr int NUM_WORKERS = 4;
		std::vector<std::thread> workers;
		workers.reserve(NUM_WORKERS);

		for (int w = 0; w < NUM_WORKERS; ++w)
		{
			workers.emplace_back([&] {
				CExclusionsReader workerReader;
				std::vector<std::wstring> workerList;
				while (!stopWorkers.load(std::memory_order_relaxed))
				{
					if (workerReader.Refresh(workerList))
					{
						successfulRefreshes.fetch_add(1, std::memory_order_relaxed);
					}
					Sleep(1);
				}
			});
		}

		// Publisher thread performs 10 dynamic updates
		for (int iter = 0; iter < 10; ++iter)
		{
			std::vector<std::wstring> currentBatch;
			const int count = (iter % 5) + 1;
			for (int c = 0; c < count; ++c)
			{
				currentBatch.push_back(L"test_worker_" + std::to_wstring(c) + L".exe");
			}
			PublishExclusions(currentBatch);
			Sleep(5);
		}

		stopWorkers.store(true, std::memory_order_release);
		for (auto& t : workers)
		{
			if (t.joinable())
			{
				t.join();
			}
		}

		CHECK_TRUE(successfulRefreshes.load() > 0);

		// Final reader verifies the last state
		CExclusionsReader finalReader;
		std::vector<std::wstring> finalList;
		CHECK_TRUE(finalReader.Refresh(finalList));
		CHECK_FALSE(finalList.empty());
	}

	// Test 6: Exclusion list sorting verification (ascending and descending case-insensitive)
	void testExclusionsSortingLogic()
	{
		std::vector<std::wstring> list = {
			L"notepad.exe",
			L"Code.exe",
			L"atom.exe",
			L"ZBrush.exe",
			L"calc.exe"
		};

		// Ascending sort
		std::sort(list.begin(), list.end(), [](const std::wstring& a, const std::wstring& b) {
			return _wcsicmp(a.c_str(), b.c_str()) < 0;
		});

		CHECK(list[0] == L"atom.exe");
		CHECK(list[1] == L"calc.exe");
		CHECK(list[2] == L"Code.exe");
		CHECK(list[3] == L"notepad.exe");
		CHECK(list[4] == L"ZBrush.exe");

		// Descending sort
		std::sort(list.begin(), list.end(), [](const std::wstring& a, const std::wstring& b) {
			return _wcsicmp(a.c_str(), b.c_str()) > 0;
		});

		CHECK(list[0] == L"ZBrush.exe");
		CHECK(list[1] == L"notepad.exe");
		CHECK(list[2] == L"Code.exe");
		CHECK(list[3] == L"calc.exe");
		CHECK(list[4] == L"atom.exe");
	}
}

void runExclusionsIntegrationTests()
{
	std::printf("--- Exclusions Integration Tests ---\n");
	testRegistryPersistenceRoundTrip();
	testPublishAndRefreshBasic();
	testPublishDynamicUpdate();
	testPublishCapacityBoundaries();
	testConcurrentReadersMultiThreaded();
	testExclusionsSortingLogic();
}
