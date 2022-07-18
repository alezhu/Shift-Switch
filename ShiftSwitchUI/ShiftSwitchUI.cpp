// ShiftSwitchUI.cpp : Defines the entry point for the application.
//
#include "pch.h"
#include "framework.h"
#include "ShiftSwitchUI.h"

#include "CApp.h"


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	return CApp::getInstance().start(hInstance, lpCmdLine);
}
