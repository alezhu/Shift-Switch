#pragma once

#include "TestHarness.h"
#include "IconHelper.h"
#include <windows.h>
#include <shlwapi.h>
#include <vector>
#include <string>

inline std::wstring FindAppIconForTest()
{
	wchar_t exePath[MAX_PATH] = L"";
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	PathRemoveFileSpecW(exePath);

	wchar_t currentDir[MAX_PATH] = L"";
	wcscpy_s(currentDir, exePath);

	for (int level = 0; level < 6; ++level)
	{
		const std::vector<std::wstring> checks = {
			std::wstring(currentDir) + L"\\ShiftSwitchUI\\app.ico",
			std::wstring(currentDir) + L"\\app.ico"
		};

		for (const auto& path : checks)
		{
			const DWORD attr = GetFileAttributesW(path.c_str());
			if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
			{
				wchar_t fullPath[MAX_PATH] = L"";
				GetFullPathNameW(path.c_str(), MAX_PATH, fullPath, nullptr);
				return std::wstring(fullPath);
			}
		}

		if (!PathRemoveFileSpecW(currentDir))
			break;
	}
	return L"";
}

inline void runIconHelperTests()
{
	std::printf("--- Tray Icon Grayscale Unit Tests ---\n");

	const std::wstring icoPath = FindAppIconForTest();
	CHECK_TRUE(!icoPath.empty());
	if (icoPath.empty()) return;

	HICON hSrcIcon = static_cast<HICON>(LoadImageW(nullptr, icoPath.c_str(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE));
	CHECK_TRUE(hSrcIcon != nullptr);
	if (!hSrcIcon) return;

	HICON hGrayIcon = CreateGrayscaleIcon(hSrcIcon);
	CHECK_TRUE(hGrayIcon != nullptr);

	if (hGrayIcon)
	{
		ICONINFO ii = {};
		CHECK_TRUE(GetIconInfo(hGrayIcon, &ii));
		CHECK_TRUE(ii.hbmColor != nullptr);
		CHECK_TRUE(ii.hbmMask != nullptr);

		BITMAP bm = {};
		GetObject(ii.hbmColor, sizeof(bm), &bm);
		CHECK_EQUAL(bm.bmWidth, 16);
		CHECK_EQUAL(bm.bmHeight, 16);
		CHECK_EQUAL(bm.bmBitsPixel, 32);

		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = bm.bmWidth;
		bmi.bmiHeader.biHeight = bm.bmHeight;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		std::vector<RGBQUAD> pixels(static_cast<size_t>(bm.bmWidth) * bm.bmHeight);
		HDC hdc = GetDC(nullptr);
		GetDIBits(hdc, ii.hbmColor, 0, bm.bmHeight, pixels.data(), &bmi, DIB_RGB_COLORS);
		ReleaseDC(nullptr, hdc);

		BYTE maxR = 0, maxA = 0;
		for (const auto& p : pixels)
		{
			CHECK_EQUAL(p.rgbRed, p.rgbGreen);
			CHECK_EQUAL(p.rgbGreen, p.rgbBlue);
			if (p.rgbRed > maxR) maxR = p.rgbRed;
			if (p.rgbReserved > maxA) maxA = p.rgbReserved;
		}
		std::printf("  Grayscale icon verified: maxR=%u (dimmed <= 120), maxAlpha=%u (scaled <= 170)\n", maxR, maxA);
		CHECK_TRUE(maxR <= 120);
		CHECK_TRUE(maxA <= 170);

		DeleteObject(ii.hbmColor);
		DeleteObject(ii.hbmMask);
		DestroyIcon(hGrayIcon);
	}

	DestroyIcon(hSrcIcon);

	HICON hNullIcon = CreateGrayscaleIcon(nullptr);
	CHECK_TRUE(hNullIcon == nullptr);
}
