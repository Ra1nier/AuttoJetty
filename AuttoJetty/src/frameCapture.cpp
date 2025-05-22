#include "../include/frameCapture.h"

int overlayX, overlayY, overlayWidth, overlayHeight;
int stateOverlayX, stateOverlayY, stateOverlayWidth, stateOverlayHeight;

FrameCapture::FrameCapture(int x = 0, int y = 0, int width = 0, int height = 0)
    : x(x), y(y), width(width), height(height) {}

FrameCapture::~FrameCapture() {}

tuple<Mat, Mat> FrameCapture::captureFrame()
{
    return {captureGameFrame(), captureGameState()};
}

Mat FrameCapture::captureGameFrame()
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

Mat FrameCapture::captureGameState()
{
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, stateWidth, stateHeight);
    SelectObject(hdcMem, hbmScreen);

    // Capture screen at correct coordinates
    BitBlt(hdcMem, 0, 0, stateWidth, stateHeight, hdcScreen, stateX, stateY, SRCCOPY);

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

// TODO: ALL of this code is obviously only relevant to my display and needs to be updated to using some sort of scaling based on the display. 
void FrameCapture::setUpCaptureFrame()
{
    // Main capture region.
    overlayWidth = width;
    overlayHeight = height;
    overlayX = x;
    overlayY = y;

    // State capture region.
	stateWidth = width / 4;
	stateHeight = height / 9;
	stateOverlayWidth = stateWidth;
	stateOverlayHeight = stateHeight;
	stateX = x + 100;
	stateY = y - 50;
	stateOverlayX = stateX;
	stateOverlayY = stateY;

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
            rect = { overlayWidth - borderThickness, 0, overlayWidth, overlayHeight}; FillRect(hdc, &rect, hBrush); // Right

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

    LPCWSTR CLASS_NAME = L"TransparentOverlay";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        CLASS_NAME,
        L"Overlay Window",
        WS_POPUP,
        overlayX, overlayY, overlayWidth, overlayHeight,
        NULL, NULL, GetModuleHandle(NULL), NULL);


    if (!hwnd)
    {
        MessageBox(NULL, L"Failed to create overlay window!", L"Error", MB_OK);
        return;
    }

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    ShowWindow(hwnd, SW_SHOW);

    HWND stateHwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        CLASS_NAME,
        L"State Overlay Window",
        WS_POPUP,
        stateOverlayX, stateOverlayY, stateOverlayWidth, stateOverlayHeight,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!stateHwnd)
    {
        MessageBox(NULL, L"Failed to create state overlay window!", L"Error", MB_OK);
        return;
    }

    SetLayeredWindowAttributes(stateHwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    ShowWindow(stateHwnd, SW_SHOW);

    HWND overHwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        CLASS_NAME,
        L"Game Over Overlay Window",
        WS_POPUP,
        overX, overY, overWidth, overHeight,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!overHwnd)
    {
        MessageBox(NULL, L"Failed to create game over overlay window!", L"Error", MB_OK);
        return;
    }

    SetLayeredWindowAttributes(overHwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    ShowWindow(overHwnd, SW_SHOW);

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