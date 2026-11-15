#include "TestHarness.h"
#include "IpcIntegrationTests.h"
#include "HookLifecycleIntegrationTests.h"
#include "HookController.h"
#include "StateCoordinator.h"
#include "ScopeExit.h"
#include "CHandle.h"
#include "IpcMessages.h"

#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

constexpr UINT IPC_TIMEOUT_MS = 3000;

namespace
{
	const wchar_t CHILD_WND_CLASS[] = L"ShiftSwitchTestChildWndClass";
	const wchar_t PARENT_WND_CLASS[] = L"ShiftSwitchTestParentWndClass";

	// Configure UIPI filters for test window messages
	void setupMessageFilters()
	{
		const auto hUser32 = GetModuleHandleW(L"User32");
		if (hUser32 != nullptr)
		{
			typedef BOOL(WINAPI* FN_ChangeWindowMessageFilter)(UINT, DWORD);
			const auto lpfn = reinterpret_cast<FN_ChangeWindowMessageFilter>(
				GetProcAddress(hUser32, "ChangeWindowMessageFilter"));
			if (lpfn != nullptr)
			{
				lpfn(WMAPP_SET_CHILD_PROCESS_HWND, MSGFLT_ADD);
				lpfn(WMAPP_ACTIVATE, MSGFLT_ADD);
				lpfn(WMAPP_DEACTIVATE, MSGFLT_ADD);
				lpfn(WMAPP_CHILD_CLOSE, MSGFLT_ADD);
				lpfn(WMAPP_QUERY_STATE, MSGFLT_ADD);
			}
		}
	}

	// State for in-test child window procedure
	struct TestChildContext
	{
		HWND parentHwnd = nullptr;
		HookController* hook = nullptr;
		bool failActivate = false;
		bool failDeactivate = false;
		int activateCount = 0;
		int deactivateCount = 0;
		int queryCount = 0;
		int closeCount = 0;
	};

	LRESULT CALLBACK TestChildWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		auto* ctx = reinterpret_cast<TestChildContext*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

		switch (msg)
		{
		case WM_CREATE:
		{
			auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
			ctx = reinterpret_cast<TestChildContext*>(cs->lpCreateParams);
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
			if (ctx != nullptr && ctx->parentHwnd != nullptr && ::IsWindow(ctx->parentHwnd))
			{
				PostMessageW(ctx->parentHwnd, WMAPP_SET_CHILD_PROCESS_HWND, reinterpret_cast<WPARAM>(hWnd), 0);
			}
			return 0;
		}
		case WMAPP_ACTIVATE:
		{
			if (ctx == nullptr) return STATE_RESULT_ERROR;
			++ctx->activateCount;
			if (ctx->failActivate) return STATE_RESULT_ERROR;
			if (ctx->hook != nullptr)
			{
				return ctx->hook->Enable() ? STATE_RESULT_OK : STATE_RESULT_ERROR;
			}
			return STATE_RESULT_OK;
		}
		case WMAPP_DEACTIVATE:
		{
			if (ctx == nullptr) return STATE_RESULT_ERROR;
			++ctx->deactivateCount;
			if (ctx->failDeactivate) return STATE_RESULT_ERROR;
			if (ctx->hook != nullptr)
			{
				return ctx->hook->Disable() ? STATE_RESULT_OK : STATE_RESULT_ERROR;
			}
			return STATE_RESULT_OK;
		}
		case WMAPP_QUERY_STATE:
		{
			if (ctx == nullptr) return STATE_RESULT_ERROR;
			++ctx->queryCount;
			if (ctx->hook != nullptr)
			{
				return ctx->hook->IsActive() ? STATE_RESULT_OK : STATE_RESULT_ERROR;
			}
			return STATE_RESULT_OK;
		}
		case WMAPP_CHILD_CLOSE:
		{
			if (ctx != nullptr)
			{
				++ctx->closeCount;
				if (ctx->hook != nullptr)
				{
					ctx->hook->Shutdown();
				}
			}
			DestroyWindow(hWnd);
			return STATE_RESULT_OK;
		}
		case WM_DESTROY:
		{
			if (ctx != nullptr && ctx->hook != nullptr)
			{
				ctx->hook->Shutdown();
			}
			PostQuitMessage(0);
			return 0;
		}
		}
		return DefWindowProcW(hWnd, msg, wp, lp);
	}

	struct TestParentContext
	{
		HWND childHwnd = nullptr;
		std::atomic<bool> childRegistered{ false };
	};

	LRESULT CALLBACK TestParentWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		auto* ctx = reinterpret_cast<TestParentContext*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

		switch (msg)
		{
		case WM_CREATE:
		{
			auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
			ctx = reinterpret_cast<TestParentContext*>(cs->lpCreateParams);
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
			return 0;
		}
		case WMAPP_SET_CHILD_PROCESS_HWND:
		{
			if (ctx != nullptr)
			{
				ctx->childHwnd = reinterpret_cast<HWND>(wp);
				ctx->childRegistered.store(true, std::memory_order_release);
			}
			return 0;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
		}
		return DefWindowProcW(hWnd, msg, wp, lp);
	}

	// Implementation of IChildProcessHook sending real Win32 messages
	class WindowChildProcessHook : public IChildProcessHook
	{
	public:
		explicit WindowChildProcessHook(HWND& childHwnd) : m_childHwnd(childHwnd) {}

		bool HasChild() const override
		{
			return m_childHwnd != nullptr && ::IsWindow(m_childHwnd);
		}

		bool EnableChild() override
		{
			if (!HasChild()) return false;
			DWORD_PTR result = 0;
			const LRESULT lr = SendMessageTimeoutW(
				m_childHwnd, WMAPP_ACTIVATE, 0, 0,
				SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &result);
			return lr != 0 && result == STATE_RESULT_OK;
		}

		bool DisableChild() override
		{
			if (!HasChild()) return false;
			DWORD_PTR result = 0;
			const LRESULT lr = SendMessageTimeoutW(
				m_childHwnd, WMAPP_DEACTIVATE, 0, 0,
				SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &result);
			return lr != 0 && result == STATE_RESULT_OK;
		}

		bool QueryChildState(bool& isActive) override
		{
			if (!HasChild()) return false;
			DWORD_PTR result = 0;
			const LRESULT lr = SendMessageTimeoutW(
				m_childHwnd, WMAPP_QUERY_STATE, 0, 0,
				SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &result);
			if (lr == 0) return false;
			isActive = (result == STATE_RESULT_OK);
			return true;
		}

	private:
		HWND& m_childHwnd;
	};

	class TestLocalHookController : public ILocalHookController
	{
	public:
		bool active = false;
		bool failEnable = false;
		bool failDisable = false;

		bool Enable() override
		{
			if (failEnable) return false;
			active = true;
			return true;
		}

		bool Disable() override
		{
			if (failDisable) return false;
			active = false;
			return true;
		}

		bool IsActive() const noexcept override
		{
			return active;
		}
	};
}

// Child worker entry point (invoked when tests are spawned as child process)
int runChildWorker(int argc, wchar_t* argv[])
{
	if (argc < 3) return 1;

	setupMessageFilters();

	const HWND parentHwnd = reinterpret_cast<HWND>(std::wcstoull(argv[2], nullptr, 10));
	if (!::IsWindow(parentHwnd)) return 2;

	const HINSTANCE hInst = GetModuleHandleW(nullptr);

	WNDCLASSEXW wcex = { sizeof(wcex) };
	wcex.lpfnWndProc = TestChildWndProc;
	wcex.hInstance = hInst;
	wcex.lpszClassName = CHILD_WND_CLASS;
	RegisterClassExW(&wcex);

	const std::wstring dllPath = FindHookDll();
	HookController hookController(dllPath);

	TestChildContext ctx;
	ctx.parentHwnd = parentHwnd;
	ctx.hook = &hookController;

	const HWND hWnd = CreateWindowExW(
		0, CHILD_WND_CLASS, L"ShiftSwitchChildWorker",
		WS_OVERLAPPEDWINDOW, 0, 0, 0, 0,
		nullptr, nullptr, hInst, &ctx);

	if (hWnd == nullptr) return 3;

	// Message loop with idle orphan detection matching CProcessChild::onIdle
	bool bDoIdle = true;
	int idleCount = 0;
	MSG msg;
	while (true)
	{
		while (bDoIdle && !::PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE))
		{
			// Orphan detection: if parent window destroyed, terminate child window
			if (!::IsWindow(parentHwnd))
			{
				hookController.Shutdown();
				DestroyWindow(hWnd);
				bDoIdle = false;
				break;
			}
			++idleCount;
			Sleep(5);
		}

		const BOOL rc = GetMessageW(&msg, nullptr, 0, 0);
		if (rc == -1) continue;
		if (rc == 0) break; // WM_QUIT

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		bDoIdle = true;
	}

	UnregisterClassW(CHILD_WND_CLASS, hInst);
	return 0;
}

namespace
{
	// Test 1: In-process dual-thread window IPC message handshake & teardown
	void testInProcessWindowIpcProtocol()
	{
		setupMessageFilters();
		const HINSTANCE hInst = GetModuleHandleW(nullptr);

		// Register parent class
		WNDCLASSEXW pwcex = { sizeof(pwcex) };
		pwcex.lpfnWndProc = TestParentWndProc;
		pwcex.hInstance = hInst;
		pwcex.lpszClassName = PARENT_WND_CLASS;
		RegisterClassExW(&pwcex);
		ScopeExit unregParentClass([&] { UnregisterClassW(PARENT_WND_CLASS, hInst); });

		// Register child class
		WNDCLASSEXW cwcex = { sizeof(cwcex) };
		cwcex.lpfnWndProc = TestChildWndProc;
		cwcex.hInstance = hInst;
		cwcex.lpszClassName = CHILD_WND_CLASS;
		RegisterClassExW(&cwcex);
		ScopeExit unregChildClass([&] { UnregisterClassW(CHILD_WND_CLASS, hInst); });

		TestParentContext parentCtx;
		const HWND parentHwnd = CreateWindowExW(
			0, PARENT_WND_CLASS, L"ShiftSwitchTestParent",
			0, 0, 0, 0, 0,
			nullptr, nullptr, hInst, &parentCtx);
		CHECK_TRUE(parentHwnd != nullptr);
		ScopeExit destroyParent([&] { if (::IsWindow(parentHwnd)) DestroyWindow(parentHwnd); });

		TestChildContext childCtx;
		childCtx.parentHwnd = parentHwnd;

		HWND childHwnd = nullptr;
		std::atomic<bool> childThreadReady{ false };
		std::thread childThread([&] {
			childHwnd = CreateWindowExW(
				0, CHILD_WND_CLASS, L"ShiftSwitchTestChild",
				0, 0, 0, 0, 0,
				nullptr, nullptr, hInst, &childCtx);
			childThreadReady.store(true, std::memory_order_release);

			MSG msg;
			while (GetMessageW(&msg, nullptr, 0, 0) > 0)
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		});

		while (!childThreadReady.load(std::memory_order_acquire))
		{
			Sleep(5);
		}

		// Process parent messages to receive WMAPP_SET_CHILD_PROCESS_HWND
		const ULONGLONG startTime = GetTickCount64();
		while (!parentCtx.childRegistered.load(std::memory_order_acquire) && GetTickCount64() - startTime < 3000)
		{
			MSG msg;
			if (PeekMessageW(&msg, parentHwnd, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			Sleep(5);
		}

		CHECK_TRUE(parentCtx.childRegistered.load());
		CHECK_EQUAL(parentCtx.childHwnd, childHwnd);
		CHECK_TRUE(::IsWindow(childHwnd));

		// 1.1: Activate message
		DWORD_PTR activateResult = 0;
		LRESULT lr = SendMessageTimeoutW(
			childHwnd, WMAPP_ACTIVATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &activateResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(activateResult, STATE_RESULT_OK);
		CHECK_EQUAL(childCtx.activateCount, 1);

		// 1.2: Query state
		DWORD_PTR queryResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_QUERY_STATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &queryResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(queryResult, STATE_RESULT_OK);

		// 1.3: Deactivate message
		DWORD_PTR deactivateResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_DEACTIVATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &deactivateResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(deactivateResult, STATE_RESULT_OK);
		CHECK_EQUAL(childCtx.deactivateCount, 1);

		// 1.4: Teardown via WMAPP_CHILD_CLOSE
		DWORD_PTR closeResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_CHILD_CLOSE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &closeResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(closeResult, STATE_RESULT_OK);
		CHECK_EQUAL(childCtx.closeCount, 1);

		if (childThread.joinable())
		{
			childThread.join();
		}
		CHECK_FALSE(::IsWindow(childHwnd));
	}

	// Test 2: StateCoordinator integration with real window IPC
	void testStateCoordinatorWindowIpcIntegration()
	{
		const HINSTANCE hInst = GetModuleHandleW(nullptr);

		WNDCLASSEXW cwcex = { sizeof(cwcex) };
		cwcex.lpfnWndProc = TestChildWndProc;
		cwcex.hInstance = hInst;
		cwcex.lpszClassName = L"ShiftSwitchCoordTestChild";
		RegisterClassExW(&cwcex);
		ScopeExit unregChildClass([&] { UnregisterClassW(L"ShiftSwitchCoordTestChild", hInst); });

		TestChildContext childCtx;
		HWND childHwnd = nullptr;
		std::atomic<bool> childThreadReady{ false };

		std::thread childThread([&] {
			childHwnd = CreateWindowExW(
				0, L"ShiftSwitchCoordTestChild", L"CoordTestChild",
				0, 0, 0, 0, 0,
				nullptr, nullptr, hInst, &childCtx);
			childThreadReady.store(true, std::memory_order_release);

			MSG msg;
			while (GetMessageW(&msg, nullptr, 0, 0) > 0)
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		});

		while (!childThreadReady.load(std::memory_order_acquire))
		{
			Sleep(5);
		}

		TestLocalHookController localHook;
		WindowChildProcessHook childHook(childHwnd);
		StateCoordinator coordinator(localHook, childHook);

		// 2.1: Enable both parent and child
		CHECK_FALSE(coordinator.IsEnabled());
		CHECK_TRUE(coordinator.SetEnabled(true));
		CHECK_TRUE(coordinator.IsEnabled());
		CHECK_TRUE(localHook.active);
		CHECK_EQUAL(childCtx.activateCount, 1);

		// 2.2: Disable both parent and child
		CHECK_TRUE(coordinator.SetEnabled(false));
		CHECK_FALSE(coordinator.IsEnabled());
		CHECK_FALSE(localHook.active);
		CHECK_EQUAL(childCtx.deactivateCount, 1);

		// 2.3: Rollback on child enable failure
		childCtx.failActivate = true;
		CHECK_FALSE(coordinator.SetEnabled(true));
		CHECK_FALSE(coordinator.IsEnabled());
		CHECK_FALSE(localHook.active); // Local hook was enabled then rolled back to disabled!
		childCtx.failActivate = false;

		// 2.4: Rollback on child disable failure
		CHECK_TRUE(coordinator.SetEnabled(true)); // Both enabled
		childCtx.failDeactivate = true;
		CHECK_FALSE(coordinator.SetEnabled(false));
		CHECK_TRUE(coordinator.IsEnabled());
		CHECK_TRUE(localHook.active); // Local hook was disabled then rolled back to enabled!
		childCtx.failDeactivate = false;

		// 2.5: Rollback on registration when child enable fails
		localHook.active = true;
		childCtx.failActivate = true;
		CHECK_FALSE(coordinator.OnChildRegistered());
		CHECK_FALSE(coordinator.IsEnabled());
		CHECK_FALSE(localHook.active); // Local hook was enabled then rolled back to disabled!
		childCtx.failActivate = false;

		// 2.6: Successful OnChildRegistered when child succeeds
		localHook.active = true;
		CHECK_TRUE(coordinator.OnChildRegistered());
		CHECK_TRUE(coordinator.IsEnabled());
		CHECK_TRUE(localHook.active);

		// Clean teardown of child
		DWORD_PTR res = 0;
		SendMessageTimeoutW(childHwnd, WMAPP_CHILD_CLOSE, 0, 0, SMTO_BLOCK, IPC_TIMEOUT_MS, &res);
		if (childThread.joinable())
		{
			childThread.join();
		}
	}

	// Test 3: True Cross-Process Win32 IPC (Spawning self as child process worker)
	void testCrossProcessIpcAndGracefulTeardown()
	{
		setupMessageFilters();
		const HINSTANCE hInst = GetModuleHandleW(nullptr);

		WNDCLASSEXW pwcex = { sizeof(pwcex) };
		pwcex.lpfnWndProc = TestParentWndProc;
		pwcex.hInstance = hInst;
		pwcex.lpszClassName = L"ShiftSwitchCrossProcessParentClass";
		RegisterClassExW(&pwcex);
		ScopeExit unregParentClass([&] { UnregisterClassW(L"ShiftSwitchCrossProcessParentClass", hInst); });

		TestParentContext parentCtx;
		const HWND parentHwnd = CreateWindowExW(
			0, L"ShiftSwitchCrossProcessParentClass", L"CrossProcessParent",
			0, 0, 0, 0, 0,
			nullptr, nullptr, hInst, &parentCtx);
		CHECK_TRUE(parentHwnd != nullptr);
		ScopeExit destroyParent([&] { if (::IsWindow(parentHwnd)) DestroyWindow(parentHwnd); });

		// Build child process command line: "TestsExe" --test-child-worker <parentHwnd>
		wchar_t exePath[MAX_PATH] = L"";
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);

		std::wstring cmdLine = L"\"";
		cmdLine += exePath;
		cmdLine += L"\" --test-child-worker ";
		cmdLine += std::to_wstring(reinterpret_cast<UINT_PTR>(parentHwnd));

		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = {};
		std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
		cmdBuffer.push_back(L'\0');

		const BOOL procCreated = CreateProcessW(
			nullptr, cmdBuffer.data(),
			nullptr, nullptr, FALSE, 0,
			nullptr, nullptr, &si, &pi);

		CHECK_TRUE(procCreated != FALSE);
		if (!procCreated) return;

		CloseHandle(pi.hThread);
		CHandle childProc(pi.hProcess);

		// Ensure child process is terminated on test exit if anything fails
		ScopeExit childGuard([&] {
			if (childProc.isValid())
			{
				TerminateProcess(childProc, 99);
			}
		});

		// Wait for child process to register its HWND via WMAPP_SET_CHILD_PROCESS_HWND
		const ULONGLONG startTime = GetTickCount64();
		while (!parentCtx.childRegistered.load(std::memory_order_acquire) && GetTickCount64() - startTime < 5000)
		{
			MSG msg;
			if (PeekMessageW(&msg, parentHwnd, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			Sleep(10);
		}

		CHECK_TRUE(parentCtx.childRegistered.load());
		const HWND childHwnd = parentCtx.childHwnd;
		CHECK_TRUE(childHwnd != nullptr);
		CHECK_TRUE(::IsWindow(childHwnd));
		if (childHwnd == nullptr)
		{
			return;
		}

		// Verify child HWND belongs to the child process PID
		DWORD childProcId = 0;
		GetWindowThreadProcessId(childHwnd, &childProcId);
		CHECK_EQUAL(childProcId, pi.dwProcessId);

		// 3.1: Cross-process activate
		DWORD_PTR activateResult = 0;
		LRESULT lr = SendMessageTimeoutW(
			childHwnd, WMAPP_ACTIVATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &activateResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(activateResult, STATE_RESULT_OK);

		// 3.2: Cross-process query state (should be active)
		DWORD_PTR queryResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_QUERY_STATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &queryResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(queryResult, STATE_RESULT_OK);

		// 3.3: Cross-process deactivate
		DWORD_PTR deactivateResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_DEACTIVATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &deactivateResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(deactivateResult, STATE_RESULT_OK);

		// 3.4: Cross-process query state (should be inactive)
		queryResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_QUERY_STATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &queryResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(queryResult, STATE_RESULT_ERROR);

		// 3.5: Graceful cross-process teardown via WMAPP_CHILD_CLOSE
		DWORD_PTR closeResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_CHILD_CLOSE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &closeResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(closeResult, STATE_RESULT_OK);

		// Wait for child process to exit cleanly
		const DWORD waitRes = WaitForSingleObject(childProc, 5000);
		CHECK_EQUAL(waitRes, WAIT_OBJECT_0);

		DWORD exitCode = 1;
		GetExitCodeProcess(childProc, &exitCode);
		CHECK_EQUAL(exitCode, 0);

		childGuard.dismiss(); // Child exited cleanly, no need to terminate
	}

	// Test 4: Cross-process orphan detection and teardown (parent window disappears)
	void testCrossProcessOrphanTeardown()
	{
		setupMessageFilters();
		const HINSTANCE hInst = GetModuleHandleW(nullptr);

		WNDCLASSEXW pwcex = { sizeof(pwcex) };
		pwcex.lpfnWndProc = TestParentWndProc;
		pwcex.hInstance = hInst;
		pwcex.lpszClassName = L"ShiftSwitchOrphanParentClass";
		RegisterClassExW(&pwcex);
		ScopeExit unregParentClass([&] { UnregisterClassW(L"ShiftSwitchOrphanParentClass", hInst); });

		TestParentContext parentCtx;
		HWND parentHwnd = CreateWindowExW(
			0, L"ShiftSwitchOrphanParentClass", L"OrphanParent",
			0, 0, 0, 0, 0,
			nullptr, nullptr, hInst, &parentCtx);
		CHECK_TRUE(parentHwnd != nullptr);
		if (parentHwnd == nullptr)
		{
			return;
		}

		wchar_t exePath[MAX_PATH] = L"";
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);

		std::wstring cmdLine = L"\"";
		cmdLine += exePath;
		cmdLine += L"\" --test-child-worker ";
		cmdLine += std::to_wstring(reinterpret_cast<UINT_PTR>(parentHwnd));

		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = {};
		std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
		cmdBuffer.push_back(L'\0');

		const BOOL procCreated = CreateProcessW(
			nullptr, cmdBuffer.data(),
			nullptr, nullptr, FALSE, 0,
			nullptr, nullptr, &si, &pi);

		CHECK_TRUE(procCreated != FALSE);
		if (!procCreated)
		{
			DestroyWindow(parentHwnd);
			return;
		}

		CloseHandle(pi.hThread);
		CHandle childProc(pi.hProcess);

		ScopeExit childGuard([&] {
			if (childProc.isValid())
			{
				TerminateProcess(childProc, 99);
			}
		});

		// Wait for registration
		const ULONGLONG startTime = GetTickCount64();
		while (!parentCtx.childRegistered.load(std::memory_order_acquire) && GetTickCount64() - startTime < 5000)
		{
			MSG msg;
			if (PeekMessageW(&msg, parentHwnd, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			Sleep(10);
		}

		CHECK_TRUE(parentCtx.childRegistered.load());

		// Abruptly destroy parent window to simulate parent crash / sudden termination
		if (parentHwnd != nullptr)
		{
			DestroyWindow(parentHwnd);
			parentHwnd = nullptr;
		}

		// Child worker's idle loop checks ::IsWindow(parentHwnd). When gone, child destroys window and exits!
		const DWORD waitRes = WaitForSingleObject(childProc, 5000);
		CHECK_EQUAL(waitRes, WAIT_OBJECT_0);

		DWORD exitCode = 1;
		GetExitCodeProcess(childProc, &exitCode);
		CHECK_EQUAL(exitCode, 0);

		childGuard.dismiss();
	}
	std::wstring FindCompanionTestExe()
	{
#ifdef _WIN64
		const wchar_t* subPath = L"x64";
#else
		const wchar_t* subPath = L"x86";
#endif

		wchar_t exePath[MAX_PATH] = L"";
		GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		PathRemoveFileSpecW(exePath);

		wchar_t currentDir[MAX_PATH] = L"";
		wcscpy_s(currentDir, exePath);

		for (int level = 0; level < 6; ++level)
		{
			const std::vector<std::wstring> checks = {
				std::wstring(currentDir) + L"\\" + subPath + L"\\Release\\ShiftSwitchTests.exe",
				std::wstring(currentDir) + L"\\" + subPath + L"\\Debug\\ShiftSwitchTests.exe",
				std::wstring(currentDir) + L"\\tests\\" + subPath + L"\\Release\\ShiftSwitchTests.exe",
				std::wstring(currentDir) + L"\\tests\\" + subPath + L"\\Debug\\ShiftSwitchTests.exe",
			};

			for (const auto& path : checks)
			{
				const DWORD attr = GetFileAttributesW(path.c_str());
				if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
				{
					wchar_t fullPath[MAX_PATH] = L"";
					GetFullPathNameW(path.c_str(), MAX_PATH, fullPath, nullptr);
					return std::wstring(fullPath);
				}
			}

			if (!PathRemoveFileSpecW(currentDir))
				break;
		}
		return L"";
	}

	// Test 5: Real Cross-Bitness (x86 + x64) IPC and teardown
	void testCrossBitnessIpc()
	{
		const std::wstring companionExe = FindCompanionTestExe();
		if (companionExe.empty())
		{
			std::printf("  (Cross-bitness companion executable not found; skipping cross-bitness test)\n");
			return;
		}

		std::printf("  Testing cross-bitness IPC with: %ls\n", companionExe.c_str());
		setupMessageFilters();
		const HINSTANCE hInst = GetModuleHandleW(nullptr);

		WNDCLASSEXW pwcex = { sizeof(pwcex) };
		pwcex.lpfnWndProc = TestParentWndProc;
		pwcex.hInstance = hInst;
		pwcex.lpszClassName = L"ShiftSwitchCrossBitnessParentClass";
		RegisterClassExW(&pwcex);
		ScopeExit unregParentClass([&] { UnregisterClassW(L"ShiftSwitchCrossBitnessParentClass", hInst); });

		TestParentContext parentCtx;
		const HWND parentHwnd = CreateWindowExW(
			0, L"ShiftSwitchCrossBitnessParentClass", L"CrossBitnessParent",
			0, 0, 0, 0, 0,
			nullptr, nullptr, hInst, &parentCtx);
		CHECK_TRUE(parentHwnd != nullptr);
		ScopeExit destroyParent([&] { if (::IsWindow(parentHwnd)) DestroyWindow(parentHwnd); });

		std::wstring cmdLine = L"\"";
		cmdLine += companionExe;
		cmdLine += L"\" --test-child-worker ";
		cmdLine += std::to_wstring(reinterpret_cast<UINT_PTR>(parentHwnd));

		STARTUPINFOW si = { sizeof(si) };
		PROCESS_INFORMATION pi = {};
		std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
		cmdBuffer.push_back(L'\0');

		const BOOL procCreated = CreateProcessW(
			nullptr, cmdBuffer.data(),
			nullptr, nullptr, FALSE, 0,
			nullptr, nullptr, &si, &pi);

		CHECK_TRUE(procCreated != FALSE);
		if (!procCreated) return;

		CloseHandle(pi.hThread);
		CHandle childProc(pi.hProcess);

		ScopeExit childGuard([&] {
			if (childProc.isValid())
			{
				TerminateProcess(childProc, 99);
			}
		});

#ifdef _WIN64
		// Parent is 64-bit; verify child is indeed 32-bit WOW64 process
		BOOL isWow64 = FALSE;
		IsWow64Process(childProc, &isWow64);
		CHECK_TRUE(isWow64 != FALSE);
#endif

		// Wait for registration
		const ULONGLONG startTime = GetTickCount64();
		while (!parentCtx.childRegistered.load(std::memory_order_acquire) && GetTickCount64() - startTime < 5000)
		{
			MSG msg;
			if (PeekMessageW(&msg, parentHwnd, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			Sleep(10);
		}

		CHECK_TRUE(parentCtx.childRegistered.load());
		const HWND childHwnd = parentCtx.childHwnd;
		CHECK_TRUE(childHwnd != nullptr);
		CHECK_TRUE(::IsWindow(childHwnd));
		if (childHwnd == nullptr)
		{
			return;
		}

		// Cross-bitness Activate
		DWORD_PTR activateResult = 0;
		LRESULT lr = SendMessageTimeoutW(
			childHwnd, WMAPP_ACTIVATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &activateResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(activateResult, STATE_RESULT_OK);

		// Cross-bitness Query State (active)
		DWORD_PTR queryResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_QUERY_STATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &queryResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(queryResult, STATE_RESULT_OK);

		// Cross-bitness Deactivate
		DWORD_PTR deactivateResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_DEACTIVATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &deactivateResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(deactivateResult, STATE_RESULT_OK);

		// Cross-bitness Query State (inactive)
		queryResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_QUERY_STATE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &queryResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(queryResult, STATE_RESULT_ERROR);

		// Cross-bitness Teardown
		DWORD_PTR closeResult = 0;
		lr = SendMessageTimeoutW(
			childHwnd, WMAPP_CHILD_CLOSE, 0, 0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG, IPC_TIMEOUT_MS, &closeResult);
		CHECK_TRUE(lr != 0);
		CHECK_EQUAL(closeResult, STATE_RESULT_OK);

		// Wait for clean process termination
		const DWORD waitRes = WaitForSingleObject(childProc, 5000);
		CHECK_EQUAL(waitRes, WAIT_OBJECT_0);

		DWORD exitCode = 1;
		GetExitCodeProcess(childProc, &exitCode);
		CHECK_EQUAL(exitCode, 0);

		childGuard.dismiss();
	}
}

void runIpcIntegrationTests()
{
	std::printf("--- IPC, Cross-Process & Teardown Integration Tests ---\n");
	testInProcessWindowIpcProtocol();
	testStateCoordinatorWindowIpcIntegration();
	testCrossProcessIpcAndGracefulTeardown();
	testCrossProcessOrphanTeardown();
	testCrossBitnessIpc();
}
