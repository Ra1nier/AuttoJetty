//
// Created by Max on 3/27/2025.
//

#include <iostream>
#include <cctype>
#include <cstdlib>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

#include <X11/Xlib.h>

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
bool keyPressed();
char readKey();

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
    height = (screenHeight * 4) / 11; // Scale randomly chosen but it works lol
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
        if (keyPressed())
        {
            if (readKey() == 'q')
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
    Display* display = XOpenDisplay(nullptr);
    if (!display)
    {
        std::cerr << "Failed to open X11 display. Make sure DISPLAY is set and you are running under X11/XWayland." << std::endl;
        std::exit(1);
    }

    int screen = DefaultScreen(display);
    screenWidth = DisplayWidth(display, screen);
    screenHeight = DisplayHeight(display, screen);
    XCloseDisplay(display);

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
    char key = '\0';
    do
    {
        cout << message << std::endl;
        std::cin >> key;
        key = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    } while (key != acceptKey && key != rejectKey);

    cout << std::endl << std::endl;
	return key == acceptKey;
}

bool keyPressed()
{
    termios oldTerm{};
    termios newTerm{};
    tcgetattr(STDIN_FILENO, &oldTerm);
    newTerm = oldTerm;
    newTerm.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);

    timeval timeout{};
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(STDIN_FILENO, &readSet);
    int result = select(STDIN_FILENO + 1, &readSet, nullptr, nullptr, &timeout);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
    return result > 0;
}

char readKey()
{
    char key = '\0';
    termios oldTerm{};
    termios newTerm{};
    tcgetattr(STDIN_FILENO, &oldTerm);
    newTerm = oldTerm;
    newTerm.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);
    read(STDIN_FILENO, &key, 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
    return key;
}
