#pragma once
#include "CMailslot.h"
#include "CProcessBase.h"
#include "AppSettings.h"
#include "IconHelper.h"
#include "StateCoordinator.h"

class CProcessMain : public CProcessBase, public IChildProcessHook
{
public:
	CProcessMain();
	~CProcessMain() override;
	LRESULT WndProc(HWND, UINT, WPARAM, LPARAM) override;

	// IChildProcessHook implementation
	bool EnableChild() override;
	bool DisableChild() override;
	bool QueryChildState(bool& isActive) override;
	bool HasChild() const override;

	bool setEnabled(bool enable) override;

protected:
	HWND m_hwndChildProcess = nullptr;
	PROCESS_INFORMATION m_childProcessInfo = {};
	StateCoordinator m_coordinator;

	HotkeyConfig m_hotkeyConfig;
	bool m_hotkeyRegistered = false;
	TrayDblClickAction m_trayDblClickAction = TrayDblClickAction::ToggleActive;

	void loadConfig();
	bool registerGlobalHotkey();
	void unregisterGlobalHotkey();
	void executeTrayDblClickAction();
	LRESULT on_WM_COMMAND(WPARAM wp, LPARAM lp) override;

#ifdef _DEBUG
	HWND m_hLogHWND = nullptr;
	CMailslot m_mailSlot{ true };
	void readMailslot();
	bool onIdle(int i) override;
	void on_WM_SIZE(WPARAM uint, LPARAM long_);
#endif
	HICON m_hIconActive = nullptr;
	HICON m_hIconInactive = nullptr;

	void initTrayIcons();
	void destroyTrayIcons();
	HICON getTrayIcon(bool active) const noexcept;
	bool updateTrayIcon();
	bool deleteTrayIcon();
	bool createTrayIcon();
	bool createChildProcess();
#ifndef 	_WIN64
	bool isWow64();
#endif
	LRESULT on_WMAPP_NOTIFYCALLBACK(WPARAM wp, LPARAM lp);
	void afterMainWindowCreate() override;
	void showContextMenu(const POINT& pt) const;
	void on_WM_HOTKEY(WPARAM wParam, LPARAM lParam);
	void onChildProcessHwndReceived(HWND hwnd);
};
