#include "ScreenCapture.h"

ScreenPixelData ScreenCapture::CaptureFullScreen() {
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return CaptureRect(x, y, w, h);
}

ScreenPixelData ScreenCapture::CaptureRegion(int x, int y, int w, int h) {
    return CaptureRect(x, y, w, h);
}

int ScreenCapture::GetScreenWidth() {
    return GetSystemMetrics(SM_CXVIRTUALSCREEN);
}

int ScreenCapture::GetScreenHeight() {
    return GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

ScreenPixelData ScreenCapture::CaptureRect(int x, int y, int w, int h) {
    ScreenPixelData result;
    if (w <= 0 || h <= 0) return result;

    HDC hDesktopDC = GetDC(nullptr);
    if (!hDesktopDC) return result;

    HDC hCaptureDC = CreateCompatibleDC(hDesktopDC);
    if (!hCaptureDC) {
        ReleaseDC(nullptr, hDesktopDC);
        return result;
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hDesktopDC, w, h);
    if (!hBitmap) {
        DeleteDC(hCaptureDC);
        ReleaseDC(nullptr, hDesktopDC);
        return result;
    }

    HGDIOBJ hOldObj = SelectObject(hCaptureDC, hBitmap);
    BitBlt(hCaptureDC, 0, 0, w, h, hDesktopDC, x, y, SRCCOPY);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    result.width = w;
    result.height = h;
    result.channels = 4;
    result.pixels.resize(w * h * 4);

    GetDIBits(hCaptureDC, hBitmap, 0, h, result.pixels.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(hCaptureDC, hOldObj);
    DeleteObject(hBitmap);
    DeleteDC(hCaptureDC);
    ReleaseDC(nullptr, hDesktopDC);

    return result;
}