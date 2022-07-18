#pragma once
#include "CMailslot.h"
#include "CProcessBase.h"

class CProcessMain : public CProcessBase
{
protected:
	HWND m_hwndChildProcess = nullptr;
#ifdef _DEBUG
	HWND m_hLogHWND = nullptr;
	CMailslot m_mailSlot{ true };
	void readMailslot();
	bool onIdle(int i) override;
	void on_WM_SIZE(WPARAM uint, LPARAM long_);
#endif
	bool deleteTrayIcon();
	bool createTrayIcon();
	void createChildProcess();
#ifndef 	_WIN64
	bool isWow64();
#endif
	LRESULT on_WMAPP_NOTIFYCALLBACK(WPARAM wp, LPARAM lp);
	void afterMainWindowCreate() override;
	void afterActivateHook() override;
	void afterDeactivateHook() override;
	void showContextMenu(const POINT& pt) const;
public:
	LRESULT WndProc(HWND, UINT, WPARAM, LPARAM) override;
};

