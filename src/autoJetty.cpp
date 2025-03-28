//
// Created by Max on 3/27/2025.
//

#include <iostream>
#include <conio.h>
#include <windows.h>

#include "opencv2/opencv.hpp"
#include "../include/frameCapture.h"
#include "../include/jettyPlayer.h"

using cv::Mat;
using std::cout;

// Func Declarations
void getScreenDimensions();

// System vals
int x = 0, y = 0;
int width = 0, height = 0;
int screenWidth = 0, screenHeight = 0;
JettyPlayer* player = nullptr;
FrameCapture* frames = nullptr;

int main()
{
    // Screen Setup
    getScreenDimensions();

    // Start JettyPlayer
    player = new JettyPlayer(width, height);
    frames = new FrameCapture(x, y, width, height);
    frames->setUpCaptureFrame(screenWidth, screenHeight, x, y, width, height);

    while (true)
    {
        // Capture frame
        Mat frame = frames->captureFrame();
        if (frame.empty())
        {
            break;
        }

        // Send the frame to the player
        player->sendFrame(frame);

        // Exit app on Q
        if (_kbhit())
        {
            if (_getch() == 'q')
            {
                cout << "Exiting..." << std::endl;
                break;
            }
        }
    }

    // Clean up and end
    delete frames;
    delete player;
    return 0;
}

void getScreenDimensions()
{
    // Get Display resolution without the screen scaling affecting it
    DEVMODE devmode;
    EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
    screenWidth = devmode.dmPelsWidth;
    screenHeight = devmode.dmPelsHeight;

    cout << "Screen Resolution: " << screenWidth << "," << screenHeight << "\n";

    // Get capture area top left coords
    x = screenWidth / 2;
    y = screenHeight / 2;

    // Get capture area
    width = screenWidth / 4;
    height = screenHeight / 2;

    cout << "Capture Area: "<< x << "," << y << ", " << width << "," << height << "\n";
}