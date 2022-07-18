#pragma once
#include "CLog.h"
#include "IWindow.h"

constexpr UINT WMAPP_NOTIFYCALLBACK = WM_APP + 1;
constexpr UINT WMAPP_SET_CHILD_PROCESS_HWND = WM_APP + 2;
constexpr UINT WMAPP_ACTIVATE = WM_APP + 3;
constexpr UINT WMAPP_DEACTIVATE = WM_APP + 4;
constexpr UINT WMAPP_CHILD_CLOSE = WM_APP + 5;

typedef BOOL(APIENTRY* SETHOOK)(BOOL bSet);

class CProcessBase : public IWindow
{
public:
	int start();
	LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;
protected:
	HWND m_HWND = nullptr;
	HMODULE m_hLib = nullptr;
	//HOOKPROC m_pfnHookKeyboardLL = nullptr;
	SETHOOK m_pfnSETHOOK = nullptr;
	//HHOOK m_hHook = nullptr;
	bool m_bActivated = false;
#ifdef _DEBUG
	CLog& getLog() const;
	void log(LPCWSTR message);
#endif
	virtual LRESULT on_WM_CREATE(WPARAM wp, LPARAM lp);
	virtual LRESULT on_WM_COMMAND(WPARAM wp, LPARAM lp);
	virtual ATOM registerMainWindowClass();
	virtual HWND getParentHWnd();
	virtual bool createMainWindow();
	virtual void afterMainWindowCreate();
	bool activateHook();
	virtual void afterActivateHook() {}
	bool deactivateHook();
	virtual void afterDeactivateHook() {}
	int runMessageLoop();
	virtual bool onIdle(int i);
	virtual bool isIdleMessage(MSG* msg) const;
};
