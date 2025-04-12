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
using std::string;

// Func Declarations
void getScreenDimensions();
void requestUserSettings();
bool displayRequest(string message, char acceptKey, char rejectKey);

// System vals
int x = 0, y = 0;
int width = 0, height = 0;
int screenWidth = 0, screenHeight = 0;
JettyPlayer* player = nullptr;
FrameCapture* frames = nullptr;

// User Settings
bool trainBot = false;
bool saveBot = false;
bool restoreBot = false;

int main()
{
    // Screen Setup
    getScreenDimensions();

    width = screenWidth / 3;
    height = (screenHeight * 3) / 7; // Scale randomly chosen but it works lol
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;

	requestUserSettings();

    // Start JettyPlayer
    player = new JettyPlayer(width, height, trainBot, saveBot, restoreBot);
    frames = new FrameCapture(x, y, width, height);
    frames->setUpCaptureFrame();

    while (true)
    {
        // Capture frame
        auto [gameFrame, stateFrame] = frames->captureFrame();
        if (gameFrame.empty() || stateFrame.empty())
        {
            break;
        }

        // Send the frame to the player
        player->sendFrame(gameFrame, stateFrame);

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
}

void requestUserSettings()
{
	cout << "Welcome to AutoJetty!\nBefore we start lets get JettyBot configured for this run.\n\n";

    trainBot = displayRequest("Do you want to train the AI? (y/n): ", 'y', 'n');
    if (trainBot)
    {
        saveBot = displayRequest("Do you want this AI training run saved? (y/n): ", 'y', 'n');
    }
    restoreBot = displayRequest("Do you want to restore the most recent version of the AI? (y/n): ", 'y', 'n');

    cout << "All done! JettyBot is now configured and AuttoJetty will now start.\n\n";
}

bool displayRequest(string message, char acceptKey, char rejectKey)
{
    do
    {
        cout << message << std::endl;
    } while (_kbhit() && !(_getch() == acceptKey || _getch() == rejectKey));

    cout << std::endl << std::endl;
	return _getch() == acceptKey;
}