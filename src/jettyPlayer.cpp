//
// Created by Max on 3/27/2025.
//

#include "../include/jettyPlayer.h"

JettyPlayer::JettyPlayer(int width, int height): frameWidth(width), frameHeight(height) {}

JettyPlayer::~JettyPlayer() {}

void JettyPlayer::sendFrame(Mat frame)
{
    // Get necessary game components.
    Mat boot = extractBoot(frame);
    Mat pillars = extractPillars(frame);

    // TODO: Remove (Just for debug), Show extracted boot frame
    // cv::imshow("Boot Frame", boot);
    // cv::waitKey(0);

    // TODO: Remove (Just for debug), Show Extracted pillar frame
    // cv::imshow("Pillar Frame", pillars);
    // cv::waitKey(0);

    // Jump Calculations from the pillars and boot position
    Rect bootPosition = getBootPosition(boot);
    vector<Rect> pillarGapPositions = getPillarGapPosition(pillars);

    if (bootPosition.empty() || pillarGapPositions.empty())
    {
        return;
    }

    Rect gap = calculatePillarGap(bootPosition, pillarGapPositions);

    // TODO: Remove (Just For Debug), Show extracted positions
    Mat positionFrame = frame.clone();
    cv::rectangle(positionFrame, bootPosition, cv::Scalar(0, 0, 255), 2);  // Red for boot
    for (auto pillar: pillarGapPositions)
    {
        cv::rectangle(positionFrame, pillar, cv::Scalar(0, 255, 0), 2); // Green for pillar
    }
    cv::rectangle(positionFrame, gap, cv::Scalar(255, 0, 0), 2); // blue for gap
    cv::imshow("Detected Objects", positionFrame);
    if (cv::waitKey(1) == 27) return;
}

Rect JettyPlayer::getBootPosition(Mat bootFrame)
{
    vector<vector<cv::Point>> boot;
    Mat hierarchy;
    cv::findContours(bootFrame, boot, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (boot.empty())
    {
        std::cerr << "Error: No boot detected!" << std::endl;
        return cv::Rect();
    }

    return cv::boundingRect(boot[0]);
}

vector<Rect> JettyPlayer::getPillarGapPosition(Mat pillarFrame)
{
    vector<vector<cv::Point>> pillars;
    Mat hierarchy;
    cv::findContours(pillarFrame, pillars, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    vector<Rect> pillarPositions;

    if (pillars.empty())
    {
        std::cerr << "Error: No pillars detected!" << std::endl;
        return pillarPositions;
    }

    for (auto pillar : pillars)
    {
        pillarPositions.push_back(cv::boundingRect(pillar));
    }

    return pillarPositions;
}

Rect JettyPlayer::calculatePillarGap(Rect &boot, vector<Rect> pillars)
{
    if (pillars.size() < 2) return Rect();

    int bootCenterX = boot.x + boot.width / 2;

    // Sort pillars by x (left to right)
    std::sort(pillars.begin(), pillars.end(), [](const Rect &a, const Rect &b) { return a.x < b.x; });

    vector<Rect> nextPillarPair;
    for (size_t i = 0; i < pillars.size() - 1; i++)
    {
        if (pillars[i].x >= bootCenterX)
        {
            nextPillarPair.push_back(pillars[i]);
            nextPillarPair.push_back(pillars[i + 1]);
            break;
        }
    }

    if (nextPillarPair.size() < 2) return Rect();

    // Sort the pair by y to distinguish top and bottom pillars
    std::sort(nextPillarPair.begin(), nextPillarPair.end(), [](const Rect &a, const Rect &b) { return a.y < b.y; });

    Rect topPillar = nextPillarPair[0];   // Top pillar
    Rect bottomPillar = nextPillarPair[1]; // Bottom pillar

    // Compute the gap rectangle
    int gapX = bottomPillar.x;
    int gapY = topPillar.y + topPillar.height;
    int gapWidth = bottomPillar.width;
    int gapHeight = bottomPillar.y - gapY;

    return Rect(gapX, gapY, gapWidth, gapHeight);
}


void JettyPlayer::sendJump(int releaseDelay)
{
    // Send E Key Press
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0x45; // E Key to jump.
    SendInput(1, &input, sizeof(INPUT));

    // Release key after delay
    Sleep(releaseDelay);
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

Mat JettyPlayer::extractBoot(Mat frame)
{
    // Convert to windows color
    cv::Mat hsv;
    cv::cvtColor(frame.clone(), hsv, cv::COLOR_BGR2GRAY);

    // Extract whitish values.
    cv::Mat bootMask;
    cv::threshold(hsv, bootMask, 235, 255, cv::THRESH_BINARY);

    return bootMask;
}

Mat JettyPlayer::extractPillars(Mat frame)
{
    cv::Mat hsv;
    cv::cvtColor(frame.clone(), hsv, cv::COLOR_BGR2HSV);

    // Define HSV range for the lower green pipe
    cv::Scalar lowerGreenVal(50, 100, 200);
    cv::Scalar upperGreenVal(55, 140, 255);

    cv::Mat pillarMask;
    cv::inRange(hsv, lowerGreenVal, upperGreenVal, pillarMask);

    return pillarMask;
}