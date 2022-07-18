#pragma once
#include "CProcessBase.h"

class CProcessChild : public CProcessBase
{
protected:
	LRESULT on_WM_CREATE(WPARAM wp, LPARAM lp) override;
	bool onIdle(int i) override;
	HWND getParentHwnd() const;
public:
	LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;
};
