#include "pch.h"
#include "CProcessChild.h"

#include "CApp.h"


LRESULT CProcessChild::on_WM_CREATE(WPARAM wp, LPARAM lp)
{
	const auto parentHWND = getParentHwnd();
	if (::IsWindow(parentHWND))
	{
		PostMessageW(parentHWND, WMAPP_SET_CHILD_PROCESS_HWND, WPARAM(m_HWND), 0);
	}
	return CProcessBase::on_WM_CREATE(wp, lp);
}

bool CProcessChild::onIdle(int i)
{
	if (!::IsWindow(getParentHwnd()))
	{
		DestroyWindow(m_HWND);
	}
	return CProcessBase::onIdle(i);
}

HWND CProcessChild::getParentHwnd() const
{
	return 	CApp::getInstance().getParentHWND();
}

LRESULT CProcessChild::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WMAPP_CHILD_CLOSE)
	{
		return DestroyWindow(hWnd);
	}
	return CProcessBase::WndProc(hWnd, message, wParam, lParam);
}

