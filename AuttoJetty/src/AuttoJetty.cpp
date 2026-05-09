//
// Created by Max on 3/27/2025.
//

#include <iostream>
#include <cctype>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

#include "../include/frameCapture.h"
#include "../include/jettyPlayer.h"

using std::cout;
using std::string;

// Startup mode determines which decision system drives jumps.
enum class RunMode
{
    Controller,
    AI
};

// Function declarations keep main readable while the helper bodies live below.
void getScreenDimensions();
void requestUserSettings();
RunMode requestRunMode();
bool requestYesNo(const string& message);
bool keyPressed();
char readKey();

// Capture geometry is initialized from the user-provided screen size.
int x = 0, y = 0;
int width = 0, height = 0;
int screenWidth = 0, screenHeight = 0;

// Player settings are filled in by requestUserSettings().
RunMode runMode = RunMode::Controller;
bool trainBot = false;
bool saveBot = false;
bool restoreBot = false;

int main()
{
    // Ask for screen size first because the default capture region is derived from it.
    getScreenDimensions();

    width = screenWidth / 3;
    height = (screenHeight * 4) / 11;
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;

	requestUserSettings();

    // JettyPlayer handles CV, decision making, and key presses; FrameCapture owns screen capture.
    auto player = std::make_unique<JettyPlayer>(
        height,
        runMode == RunMode::AI,
        trainBot,
        saveBot,
        restoreBot);
    auto frames = std::make_unique<FrameCapture>(x, y, width, height);
    frames->setUpCaptureFrame();

    while (true)
    {
        // Each loop captures the game area plus the state/lives area, then processes one frame.
        auto [gameFrame, stateFrame] = frames->captureFrame();
        if (gameFrame.empty() || stateFrame.empty())
        {
            break;
        }

        player->sendFrame(gameFrame, stateFrame);

        // Non-blocking keyboard handling lets Q exit and J send a manual test jump.
        if (keyPressed())
        {
            char key = readKey();
            if (key == 'q')
            {
                cout << "Exiting..." << std::endl;
                break;
            }
            if (key == 'j')
            {
                player->testJump();
            }
        }
    }

    cv::destroyAllWindows();
    return 0;
}

void getScreenDimensions()
{
    // Wayland screen capture does not expose one universal global size, so ask once at startup.
    cout << "Wayland does not provide a standard global screen size API for this use case." << std::endl;
    cout << "Enter your screen width [1920]: ";
    string input;
    std::getline(std::cin >> std::ws, input);
    screenWidth = input.empty() ? 1920 : std::stoi(input);

    cout << "Enter your screen height [1080]: ";
    std::getline(std::cin >> std::ws, input);
    screenHeight = input.empty() ? 1080 : std::stoi(input);

    cout << "Screen Resolution: " << screenWidth << "," << screenHeight << "\n";
}

void requestUserSettings()
{
	cout << "Welcome to AuttoJetty.\n\n";

    runMode = requestRunMode();
    trainBot = false;
    saveBot = false;
    restoreBot = false;

    if (runMode == RunMode::AI)
    {
        restoreBot = requestYesNo("Load the saved AI policy before starting? (y/n): ");
        trainBot = requestYesNo("Train the AI during this run? (y/n): ");
        if (trainBot)
        {
            saveBot = requestYesNo("Save the AI policy when training ends? (y/n): ");
        }
    }

    cout << "\nMode: " << (runMode == RunMode::AI ? "AI" : "Controller") << std::endl;
    cout << "Controls while running: q=quit, j=test jump.\n\n";
}

RunMode requestRunMode()
{
    // Force one explicit mode choice so controller runs are not interrupted by AI-only questions.
    char key = '\0';
    do
    {
        cout << "Select decision mode:" << std::endl;
        cout << "  c = Controller" << std::endl;
        cout << "  a = AI" << std::endl;
        cout << "Mode (c/a): ";
        std::cin >> key;
        key = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    } while (key != 'c' && key != 'a');

    return key == 'a' ? RunMode::AI : RunMode::Controller;
}

bool requestYesNo(const string& message)
{
    // Normalize Y/N input so callers only deal with booleans.
    char key = '\0';
    do
    {
        cout << message;
        std::cin >> key;
        key = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    } while (key != 'y' && key != 'n');

	return key == 'y';
}

bool keyPressed()
{
    // Temporarily put stdin in non-canonical mode so select() can poll without blocking.
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
    // Read one keypress without waiting for Enter and restore the terminal immediately after.
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
