#pragma once
#include "CLog.h"
#include "HookController.h"
#include "IWindow.h"
#include "IpcMessages.h"
#include <string>

class CProcessBase : public IWindow
{
public:
	int start();
	LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

	virtual bool setEnabled(bool enable);
	bool isEnabled() const noexcept { return m_hookController.IsActive(); }

	bool activateHook() { return setEnabled(true); }
	bool deactivateHook() { return setEnabled(false); }

protected:
	HWND m_HWND = nullptr;
	HookController m_hookController;

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
	int runMessageLoop();
	virtual bool onIdle(int i);
	virtual bool isIdleMessage(MSG* msg) const;
};
