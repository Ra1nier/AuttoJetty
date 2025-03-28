//
// Created by Max on 3/27/2025.
//

#include "../include/frameCapture.h"

FrameCapture::FrameCapture(int x, int y, int width, int height): x(x), y(y), width(width), height(height) {}

FrameCapture::~FrameCapture() {}

Mat FrameCapture::captureFrame()
{
    // Defines the screen device context
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    // Creates a BitMap and assigns that as the drawing surface for the ScreenMem.
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hbmScreen);

    // Captures the specified screen area.
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, x, y, SRCCOPY);

    // Convert the Captured BitMap into a Mat
    BITMAP bmp;
    GetObject(hbmScreen, sizeof(BITMAP), &bmp);
    cv::Mat mat(bmp.bmHeight, bmp.bmWidth, CV_8UC4); //BGRA Format cause windows is weird
    GetBitmapBits(hbmScreen, bmp.bmHeight * bmp.bmWidth * 4, mat.data);

    // Clean up
    DeleteObject(hbmScreen);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return mat;
}



