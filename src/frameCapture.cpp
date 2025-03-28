//
// Created by Max on 3/27/2025.
//

#include "../include/frameCapture.h"

int overlayWidth;
int overlayHeight;

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

void FrameCapture::setUpCaptureFrame(int screenWidth, int screenHeight, int x, int y, int captureWidth, int captureHeight)
{
    // Get capture area top left coords
    x = screenWidth / 2;
    y = screenHeight / 2;

    // Get capture area
    width = screenWidth / 4;
    height = screenHeight / 2;

    overlayWidth = captureWidth;
    overlayHeight = captureHeight;

    cout << "Capture Area: "<< x << "," << y << ", " << width << "," << height << "\n";

    drawOverlay();
}

// Window procedure: Handles drawing and closing
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int borderThickness = 5;  // Thickness of the orange overlay

    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Create an orange brush
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 128, 0));

            // Draw only the border (leave the center transparent)
            RECT rect;

            // Top border
            rect = {0, 0, overlayWidth, borderThickness};
            FillRect(hdc, &rect, hBrush);

            // Bottom border
            rect = {0, overlayHeight - borderThickness, overlayWidth, overlayHeight};
            FillRect(hdc, &rect, hBrush);

            // Left border
            rect = {0, 0, borderThickness, overlayHeight};
            FillRect(hdc, &rect, hBrush);

            // Right border
            rect = {overlayWidth - borderThickness, 0, overlayWidth, overlayHeight};
            FillRect(hdc, &rect, hBrush);

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
    const char CLASS_NAME[] = "TransparentOverlay";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Calculate centered position
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;

    // Create layered, topmost, transparent window
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,  // Transparent overlay
        CLASS_NAME,
        "Overlay Window",
        WS_POPUP,  // No borders/title
        x, y, width, height,  // Centered position
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hwnd)
    {
        MessageBox(NULL, "Failed to create overlay window!", "Error", MB_OK);
        return;
    }

    // Make the background fully transparent
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    // Show the overlay
    ShowWindow(hwnd, SW_SHOW);

    // Display Instructions
    cout << "There should be an orange box on your screen. Align the Jetty Boot game within the orange box, then press 'G' to start or 'Q' to quit." << std::endl;
    bool start = false;
    MSG msg = {};

    // Main loop: process Windows messages & check for user input
    while (!start)
    {
        // Process Windows messages (keeps overlay running)
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            // Exit if window is closed
            if (msg.message == WM_QUIT)
                return;
        }

        // Check for key press
        if (_kbhit())
        {
            char keyPressed = _getch();
            switch (keyPressed)
            {
                case 'g':
                    cout << "Starting... Press 'Q' to stop program." << std::endl;
                start = true;
                break;
                case 'q':
                    cout << "Exiting..." << std::endl;
                return;
                default:
                    break;
            }
        }
    }
}