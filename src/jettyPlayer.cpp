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
    cv::imshow("Boot Frame", boot);
    cv::waitKey(0);

    // TODO: Remove (Just for debug), Show Extracted pillar frame
    cv::imshow("Pillar Frame", pillars);
    cv::waitKey(0);

    // TODO: Jump Calculations from the pillars and boot position
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
    cv::cvtColor(frame.clone(), hsv, cv::COLOR_BGR2HSV);

    // Extract whitish values.
    cv::Scalar lowerWhiteVal(120, 120, 120);
    cv::Scalar upperWhiteVal(255, 255, 255);
    cv::Mat bootMask;
    cv::inRange(hsv, lowerWhiteVal, upperWhiteVal, bootMask);

    return bootMask;
}

Mat JettyPlayer::extractPillars(Mat frame)
{
    // Convert to windows color
    cv::Mat hsv;
    cv::cvtColor(frame.clone(), hsv, cv::COLOR_BGR2HSV);

    // Extract green values.
    cv::Scalar lowerGreenVal(50, 100, 50);
    cv::Scalar upperGreenVal(120, 255, 120);
    cv::Mat pillarMask;
    cv::inRange(hsv, lowerGreenVal, upperGreenVal, pillarMask);

    return pillarMask;
}