#pragma once

#include <cstdint>
#include <vector>
#include <Windows.h>

struct ScreenPixelData {
    int width = 0;
    int height = 0;
    int channels = 4;
    std::vector<uint8_t> pixels;

    bool IsValid() const {
        return width > 0 && height > 0 && !pixels.empty();
    }
};

class ScreenCapture {
public:
    ScreenCapture() = default;
    ~ScreenCapture() = default;

    ScreenCapture(const ScreenCapture&) = delete;
    ScreenCapture& operator=(const ScreenCapture&) = delete;

    ScreenPixelData CaptureFullScreen();
    ScreenPixelData CaptureRegion(int x, int y, int w, int h);
    static int GetScreenWidth();
    static int GetScreenHeight();

private:
    ScreenPixelData CaptureRect(int x, int y, int w, int h);
};