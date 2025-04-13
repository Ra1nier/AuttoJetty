//
// Created by Max on 3/27/2025.
//

#ifndef JETTYPLAYER_H
#define JETTYPLAYER_H

#include <windows.h>
#include <vector>
#include <thread>
#include <opencv2/highgui.hpp>

#include "opencv2/opencv.hpp"
#include "jettyAI.h"

using std::vector;
using cv::Mat;
using cv::Rect;

void makeWindowAlwaysOnTop(const std::string& windowName);

class JettyPlayer
{
private:
    // JettyPlayer specific
    double bootHeight = 0.0;
    double pillarTop = 0.0;
    double pillarBottom = 0.0;
    int prevBootY = 0;
    int previousLives = 3;

    // External vars
    int frameWidth = 0;
    int frameHeight = 0;

	JettyBot* jettyBot = nullptr;
	bool gameOver = false;
	bool trainAI = false;
    bool saveAI = false;
    std::chrono::steady_clock::time_point runStartTime;
    bool readyToTrain = false;

    /**
     * Extracts the boot object from the frame.
     *
     * @param frame Frame captured from the screen.
     * @return A Mat representing the mask of the boot.
     */
    Mat extractBoot(Mat frame);

    /**
     * Extracts the pillar objects from the frame.
     *
     * @param frame Frame captured from the screen.
     * @return A Mat representing the mask of the pillars.
     */
    Mat extractPillars(Mat frame);

    Rect getBootPosition(Mat bootFrame);

    vector<Rect> getPillarGapPosition(Mat pillarFrame);

    Rect calculatePillarGap(Rect& boot, vector<Rect> pillars);

    vector<Rect> getLivesLeft(Mat stateFrame);

    void decideNextMove(Rect& gap, Rect& boot, int livesLeft);

    /**
     * Sends the E Key to windows in order to jump the jet boot.
     *
     * @param releaseDelay Delay in milliseconds to release the key press.
     */
    void sendJump(int releaseDelay);

    void drawPreview(Mat gameFrame, Mat stateFrame, Rect boot, vector<Rect> pillarGaps, Rect gap, vector<Rect> lives);

public:
    JettyPlayer(int width, int height, bool train, bool save, bool restore);
    ~JettyPlayer();

    /**
     * Sends the frame to the JettyBootPlayer for processing.
     *
     * @param frame Frame captured from the screen.
     */
    void sendFrame(Mat gameFrame, Mat stateFrame);
};

#endif //JETTYPLAYER_H
