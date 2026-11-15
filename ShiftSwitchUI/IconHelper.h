#pragma once

#include <windows.h>
#include <vector>
#include <cstring>

/**
 * Creates a grayscale/disabled version of an HICON programmatically.
 * Caller owns the returned HICON and must call DestroyIcon() when done.
 * If hSrcIcon is null or conversion fails, returns nullptr.
 */
inline HICON CreateGrayscaleIcon(HICON hSrcIcon)
{
	if (!hSrcIcon)
		return nullptr;

	ICONINFO srcInfo = {};
	if (!GetIconInfo(hSrcIcon, &srcInfo))
		return nullptr;

	// Ensure source bitmaps from GetIconInfo are always cleaned up
	struct BmpCleaner
	{
		HBITMAP hColor;
		HBITMAP hMask;
		~BmpCleaner()
		{
			if (hColor) DeleteObject(hColor);
			if (hMask) DeleteObject(hMask);
		}
	} cleaner{ srcInfo.hbmColor, srcInfo.hbmMask };

	if (!srcInfo.hbmColor)
	{
		// Monochrome icon (AND/XOR masks only, no color bitmap)
		return CopyIcon(hSrcIcon);
	}

	BITMAP bm = {};
	if (GetObject(srcInfo.hbmColor, sizeof(bm), &bm) == 0)
		return nullptr;

	const int width = bm.bmWidth;
	const int height = bm.bmHeight;

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = height; // positive -> bottom-up
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	std::vector<RGBQUAD> pixels(static_cast<size_t>(width) * height);
	HDC hdc = GetDC(nullptr);
	if (!hdc)
		return nullptr;

	const int lines = GetDIBits(hdc, srcInfo.hbmColor, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);
	if (lines != height)
	{
		ReleaseDC(nullptr, hdc);
		return nullptr;
	}

	bool hasAlpha = false;
	for (const auto& p : pixels)
	{
		if (p.rgbReserved > 0)
		{
			hasAlpha = true;
			break;
		}
	}

	for (auto& p : pixels)
	{
		// Luminance conversion (ITU-R BT.601)
		const BYTE gray = static_cast<BYTE>((p.rgbRed * 299 + p.rgbGreen * 587 + p.rgbBlue * 114) / 1000);
		// Dim to inactive gray level
		const BYTE dimGray = static_cast<BYTE>((gray * 120) / 255);
		p.rgbRed = dimGray;
		p.rgbGreen = dimGray;
		p.rgbBlue = dimGray;
		if (hasAlpha)
		{
			// Scale opacity for inactive/disabled state
			p.rgbReserved = static_cast<BYTE>((p.rgbReserved * 170) / 255);
		}
	}

	void* pBits = nullptr;
	HBITMAP hGrayColorBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
	ReleaseDC(nullptr, hdc);

	if (!hGrayColorBmp || !pBits)
	{
		if (hGrayColorBmp) DeleteObject(hGrayColorBmp);
		return nullptr;
	}

	std::memcpy(pBits, pixels.data(), pixels.size() * sizeof(RGBQUAD));

	ICONINFO destInfo = {};
	destInfo.fIcon = TRUE;
	destInfo.xHotspot = srcInfo.xHotspot;
	destInfo.yHotspot = srcInfo.yHotspot;
	destInfo.hbmMask = srcInfo.hbmMask;
	destInfo.hbmColor = hGrayColorBmp;

	HICON hGrayIcon = CreateIconIndirect(&destInfo);
	DeleteObject(hGrayColorBmp);

	return hGrayIcon;
}
