#include "pch.h"
#include "CProcessBase.h"

#include "CApp.h"
#include "resource.h"

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
	activateHook();
}

bool CProcessBase::activateHook()
{
#ifdef _DEBUG
	log(L"Activate");
#endif
	if (m_bActivated) {
#ifdef _DEBUG
		log(L"Already activated");
#endif
		return false;
	}

#ifdef 	_WIN64
	m_hLib = LoadLibrary(_T("ShiftSwitch64.dll"));
#else
	m_hLib = LoadLibrary(_T("ShiftSwitch.dll"));
#endif
	if (m_hLib == nullptr)
	{
#ifdef _DEBUG
		log(L"Hook Dll not loaded");
#endif
		return false;
	}

	//m_pfnHookKeyboardLL = (HOOKPROC)GetProcAddress(m_hLib, "KeyboardProc");
	//if (m_pfnHookKeyboardLL == nullptr) {
	//if (m_pfnHookKeyboardLL == nullptr) {
	m_pfnSETHOOK = (SETHOOK)GetProcAddress(m_hLib, "SetHook");
	if (m_pfnSETHOOK == nullptr) {
#ifdef _DEBUG
		log(L"HookProc not found");
#endif
		return false;
	}
	m_bActivated = m_pfnSETHOOK(true);

	if (!m_bActivated) {
#ifdef _DEBUG
		log(L"Hook not set");
#endif
		return false;
	}

	afterActivateHook();

	return true;
}

bool CProcessBase::deactivateHook()
{
#ifdef _DEBUG
	log(L"Deactivate");
#endif
	if (!m_bActivated) {
#ifdef _DEBUG
		log(L"Hook not active ");
#endif
		return false;
	}
	if (m_pfnSETHOOK != nullptr)
	{
		if (m_pfnSETHOOK(false))
		{
			m_bActivated = false;
			m_pfnSETHOOK = nullptr;
			FreeLibrary(m_hLib);
			afterDeactivateHook();
			return true;
		}
	}
	return false;
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
		activateHook();
		return 0;
	case  WMAPP_DEACTIVATE:
		deactivateHook();
		return 0;
	case WM_DESTROY:
		deactivateHook();
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

// Message handler for about box.
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
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
		activateHook();
		break;
	case IDM_DEACTIVATE:
		deactivateHook();
		break;
	case IDM_ABOUT:
		DialogBox(CApp::getInstance().handle(), MAKEINTRESOURCE(IDD_ABOUTBOX), m_HWND, AboutDlgProc);
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
#ifdef _DEBUG
	return nullptr;
#else	
	return HWND_MESSAGE;
#endif
}
