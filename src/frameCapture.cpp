#include "../include/frameCapture.h"

int overlayX, overlayY, overlayWidth, overlayHeight;

FrameCapture::FrameCapture(int x = 0, int y = 0, int width = 0, int height = 0)
    : x(x), y(y), width(width), height(height) {}

FrameCapture::~FrameCapture() {}

Mat FrameCapture::captureFrame()
{
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hbmScreen);

    // Capture screen at correct coordinates
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, x, y, SRCCOPY);

    // Convert to OpenCV Mat
    BITMAP bmp;
    GetObject(hbmScreen, sizeof(BITMAP), &bmp);
    cv::Mat mat(bmp.bmHeight, bmp.bmWidth, CV_8UC4); 
    GetBitmapBits(hbmScreen, bmp.bmHeight * bmp.bmWidth * 4, mat.data);

    // Clean up
    DeleteObject(hbmScreen);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return mat;
}

void FrameCapture::setUpCaptureFrame()
{
    overlayWidth = width;
    overlayHeight = height;
    overlayX = x;
    overlayY = y;

    std::cout << "Capture Area: " << x << ", " << y << ", " << width << ", " << height << "\n";
    std::cout << "Overlay Area: " << overlayX << ", " << overlayY << ", " << overlayWidth << ", " << overlayHeight << "\n";

    drawOverlay();
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int borderThickness = 5;

    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 128, 0));

            RECT rect;
            rect = {0, 0, overlayWidth, borderThickness}; FillRect(hdc, &rect, hBrush);  // Top
            rect = {0, overlayHeight - borderThickness, overlayWidth, overlayHeight}; FillRect(hdc, &rect, hBrush); // Bottom
            rect = {0, 0, borderThickness, overlayHeight}; FillRect(hdc, &rect, hBrush); // Left
            rect = {overlayWidth - borderThickness, 0, overlayWidth, overlayHeight}; FillRect(hdc, &rect, hBrush); // Right

            DeleteObject(hBrush);
            EndPaint(hwnd, &ps);
        }
        return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
        return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void FrameCapture::drawOverlay()
{
    SetProcessDPIAware();

    const char CLASS_NAME[] = "TransparentOverlay";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        CLASS_NAME,
        "Overlay Window",
        WS_POPUP,
        overlayX, overlayY, overlayWidth, overlayHeight,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hwnd)
    {
        MessageBox(NULL, "Failed to create overlay window!", "Error", MB_OK);
        return;
    }

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    ShowWindow(hwnd, SW_SHOW);

    cout << "Orange box displayed. Please align JettBoot inside the orange frame, then press 'G' to start AutoJetty." << std::endl;
    bool start = false;
    MSG msg = {};

    while (!start)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) return;
        }

        if (_kbhit())
        {
            char keyPressed = _getch();
            if (keyPressed == 'g')
            {
                cout << "Starting..." << std::endl;
                start = true;
            }
        }
    }
}