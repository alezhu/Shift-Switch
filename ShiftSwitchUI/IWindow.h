#pragma once
#include <Windows.h>
struct IWindow
{
	virtual ~IWindow() = default;
	virtual LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
};