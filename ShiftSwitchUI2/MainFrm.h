// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include <sstream>

class CMainFrame :
	public CFrameWindowImpl<CMainFrame>,
	public CUpdateUI<CMainFrame>,
	public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CView m_view;
	//CCommandBarCtrl m_CmdBar;
	CFont m_font;
	HANDLE m_hChannel;

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		UIUpdateToolBar();

		ReadChannel();

		return FALSE;
	}

	BEGIN_UPDATE_UI_MAP(CMainFrame)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileHook)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileUnhook)
		COMMAND_ID_HANDLER(ID_FILE_CLEAR, OnFileClear)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		//// create command bar window
		//HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		//// attach menu
		////m_CmdBar.AttachMenu(GetMenu());
		//// load command bar images
		//m_CmdBar.LoadImages(IDR_MAINFRAME);
		//// remove old menu
		////SetMenu(NULL);

		HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

		CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		//AddSimpleReBarBand(hWndCmdBar);
		AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

		CreateSimpleStatusBar();

		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL, WS_EX_CLIENTEDGE);
		m_font = AtlCreateControlFont();
		m_view.SetFont(m_font);

		UIAddToolBar(hWndToolBar);
		UISetCheck(ID_VIEW_TOOLBAR, 1);
		UISetCheck(ID_VIEW_STATUS_BAR, 1);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		EnumLayouts();

		PrintKeyNames();

		CreateChannel();

		return 0;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		Unhook(true);
		// unregister message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->RemoveMessageFilter(this);
		pLoop->RemoveIdleHandler(this);

		bHandled = FALSE;

		CloseHandle(m_hChannel);
		return 1;
	}

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}
	HMODULE hLib = 0;
	FARPROC pfnHookKeyboardLL = nullptr;
	HHOOK hHook = 0;

	LRESULT OnFileHook(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if (hHook != 0) {
			return 0;
		}

		hLib = LoadLibrary(_T("ShiftSwitch.dll"));
		pfnHookKeyboardLL = GetProcAddress(hLib, "KeyboardProc");

		hHook = SetWindowsHookEx(
			WH_KEYBOARD,
			(HOOKPROC)pfnHookKeyboardLL,
			hLib,
#ifdef _DEBUG
			GetCurrentThreadId()
#else
			0
#endif
		);
		m_view.AddString(L"Hooked");
		return 0;
	}

	void Unhook(bool bClose) {
		if (hHook == 0) {
			return;
		}
		UnhookWindowsHookEx(hHook);
		hHook = 0;
		FreeLibrary(hLib);
		pfnHookKeyboardLL = nullptr;

		if (!bClose && m_view != nullptr) {
			m_view.AddString(L"Unhooked");
		}
	}
	LRESULT OnFileUnhook(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		Unhook(false);
		return 0;
	}


	LRESULT OnFileClear(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		m_view.ResetContent();
		return 0;
	}

	//LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//{
	//	static BOOL bVisible = TRUE;	// initially visible
	//	bVisible = !bVisible;
	//	CReBarCtrl rebar = m_hWndToolBar;
	//	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// toolbar is 2nd added band
	//	rebar.ShowBand(nBandIndex, bVisible);
	//	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	//	UpdateLayout();
	//	return 0;
	//}

	//LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//{
	//	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	//	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	//	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	//	UpdateLayout();
	//	return 0;
	//}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}

	void EnumLayouts() {
		auto iCount = GetKeyboardLayoutList(0, nullptr);
		if (iCount == 0) {
			return;
		}

		auto hKLCurrent = GetKeyboardLayout(0);

		HKL* pHKL = new HKL[iCount];
		GetKeyboardLayoutList(iCount, pHKL);
		m_view.AddString(L"Layouts:");
		CHAR buffer[KL_NAMELENGTH];

		for (size_t i = 0; i < iCount; i++)
		{
			ActivateKeyboardLayout(pHKL[i], 0);

			if (GetKeyboardLayoutNameA(buffer)) {
				std::wstringstream stream;
				stream << pHKL[i] << ":" << buffer << std::endl;
				auto str = stream.str();
				m_view.AddString(str.c_str());
			}
		}
		delete[] pHKL;

		ActivateKeyboardLayout(hKLCurrent, 0);

	}

	void CreateChannel() {
		m_hChannel = CreateMailslot(
			LR"(\\.\mailslot\ShiftSwitch)",
			0,
			MAILSLOT_WAIT_FOREVER,
			nullptr
		);
	}

	void ReadChannel() {
		while (true)
		{
			DWORD dwMaxMsgSize;
			DWORD dwNextSize;
			DWORD dwMessageCount;
			DWORD dwReadTimeout;
			auto result = GetMailslotInfo(
				m_hChannel,
				&dwMaxMsgSize,
				&dwNextSize,
				&dwMessageCount,
				&dwReadTimeout
			);
			if (!result || dwNextSize == 0 || dwNextSize == MAILSLOT_NO_MESSAGE || dwMessageCount == 0)
			{
				break;
			}
			auto buffer = std::make_unique<BYTE[]>(dwNextSize);
			buffer[0] = 0;
			DWORD dwReaded = 0;
			result = ReadFile(
				m_hChannel,
				buffer.get(),
				dwNextSize,
				&dwReaded,
				nullptr
			);
			if (!result) {
				break;
			}
			m_view.AddString((LPCTSTR)buffer.get());
		}

	}


	void PrintKeyName(const LONG vkCode, const WCHAR* vkName) {
#define BUF_SIZE 128
		TCHAR keyname[BUF_SIZE]{};

		std::wstringstream stream;
		auto scanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
		auto value = (scanCode << 16);
		GetKeyNameText(value, keyname, BUF_SIZE / sizeof(keyname[0]));
		stream << vkName << " : " << keyname;
		auto str = stream.str();
		m_view.AddString(str.c_str());
	}

	void PrintKeyNames() {
		PrintKeyName(VK_LSHIFT, L"VK_LSHIFT");
		PrintKeyName(VK_RSHIFT, L"VK_RSHIFT");
		PrintKeyName(VK_SHIFT, L"VK_SHIFT");
	}
};
