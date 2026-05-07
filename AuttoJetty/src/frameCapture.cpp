#include "../include/frameCapture.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

#include <X11/Xatom.h>
#include <X11/Xutil.h>

int overlayX, overlayY, overlayWidth, overlayHeight;
int stateOverlayX, stateOverlayY, stateOverlayWidth, stateOverlayHeight;

namespace
{
constexpr int BorderThickness = 5;

void setAlwaysOnTop(Display* display, Window window)
{
    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
    Atom above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);

    XClientMessageEvent event{};
    event.type = ClientMessage;
    event.window = window;
    event.message_type = wmState;
    event.format = 32;
    event.data.l[0] = 1;
    event.data.l[1] = static_cast<long>(above);

    XSendEvent(
        display,
        DefaultRootWindow(display),
        False,
        SubstructureRedirectMask | SubstructureNotifyMask,
        reinterpret_cast<XEvent*>(&event));
}

Window createOverlayBar(Display* display, int x, int y, int width, int height)
{
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = 0xff8000;

    Window window = XCreateWindow(
        display,
        root,
        x,
        y,
        std::max(width, 1),
        std::max(height, 1),
        0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWOverrideRedirect | CWBackPixel,
        &attrs);

    XMapRaised(display, window);
    setAlwaysOnTop(display, window);
    return window;
}

std::vector<Window> createOverlayWindow(Display* display, int x, int y, int width, int height)
{
    return {
        createOverlayBar(display, x, y, width, BorderThickness),
        createOverlayBar(display, x, y + height - BorderThickness, width, BorderThickness),
        createOverlayBar(display, x, y, BorderThickness, height),
        createOverlayBar(display, x + width - BorderThickness, y, BorderThickness, height)
    };
}
}

FrameCapture::FrameCapture(int x, int y, int width, int height)
    : x(x), y(y), width(width), height(height)
{
    display = XOpenDisplay(nullptr);
    if (!display)
    {
        std::cerr << "Failed to open X11 display. Make sure DISPLAY is set and you are running under X11/XWayland." << std::endl;
        std::exit(1);
    }

    rootWindow = DefaultRootWindow(display);
}

FrameCapture::~FrameCapture()
{
    if (display)
    {
        XCloseDisplay(display);
    }
}

tuple<Mat, Mat> FrameCapture::captureFrame()
{
    return {captureGameFrame(), captureGameState()};
}

Mat FrameCapture::captureGameFrame()
{
    return captureRegion(x, y, width, height);
}

Mat FrameCapture::captureGameState()
{
    return captureRegion(stateX, stateY, stateWidth, stateHeight);
}

Mat FrameCapture::captureRegion(int captureX, int captureY, int captureWidth, int captureHeight)
{
    if (!display || captureWidth <= 0 || captureHeight <= 0)
    {
        return {};
    }

    XImage* image = XGetImage(
        display,
        rootWindow,
        captureX,
        captureY,
        static_cast<unsigned int>(captureWidth),
        static_cast<unsigned int>(captureHeight),
        AllPlanes,
        ZPixmap);

    if (!image)
    {
        return {};
    }

    cv::Mat bgra(image->height, image->width, CV_8UC4, image->data);
    cv::Mat frame = bgra.clone();
    XDestroyImage(image);
    return frame;
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

void FrameCapture::drawOverlay()
{
    if (!display)
    {
        return;
    }

    std::vector<Window> gameOverlay = createOverlayWindow(display, overlayX, overlayY, overlayWidth, overlayHeight);
    std::vector<Window> stateOverlay = createOverlayWindow(display, stateOverlayX, stateOverlayY, stateOverlayWidth, stateOverlayHeight);
    XFlush(display);

    cout << "Orange boxes displayed. Please align JettBoot inside the orange frame, then press 'G' and Enter to start AutoJetty." << std::endl;

    char key = '\0';
    while (key != 'g')
    {
        std::cin >> key;
        key = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    }

    for (Window window : gameOverlay)
    {
        XDestroyWindow(display, window);
    }
    for (Window window : stateOverlay)
    {
        XDestroyWindow(display, window);
    }
    XFlush(display);

    cout << "Starting..." << std::endl;
}
