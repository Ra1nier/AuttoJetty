//
// Created by Max on 3/27/2025.
//

#ifndef JETTYPLAYER_H
#define JETTYPLAYER_H

#include <vector>
#include <thread>
#include <chrono>
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
    // Previous boot Y is used to estimate vertical speed between frames.
    int prevBootY = 0;

    // Lives and score are tracked so training can reward survival and progress.
    int previousLives = 3;
    int previousScore = 0;

    // Capture height is needed to estimate a corrected boot center when CV sees only part of it.
    int frameHeight = 0;

	JettyBot* jettyBot = nullptr;

    // Controller mode uses the deterministic controller; AI mode uses JettyBot::shouldJump().
    bool useAI = false;
	bool trainAI = false;
    bool saveAI = false;

    // Startup/restart delay prevents stale frames from being treated as active gameplay.
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

    /**
     * Finds the most likely boot contour from a binary boot mask.
     */
    Rect getBootPosition(Mat bootFrame);

    /**
     * Finds green pillar rectangles from a binary pillar mask.
     */
    vector<Rect> getPillarGapPosition(Mat pillarFrame);

    /**
     * Placeholder for score OCR/parsing; currently keeps the previous score.
     */
    int getScoreFromFrame(cv::Mat stateFrame);

    /**
     * Chooses the next visible pillar pair and returns the gap between them.
     */
    Rect calculatePillarGap(Rect& boot, vector<Rect> pillars);

    /**
     * Detects remaining life icons from the state capture.
     */
    vector<Rect> getLivesLeft(Mat stateFrame);

    /**
     * Debounces life detection so one bad frame does not trigger a restart.
     */
    int getStableLives(int currentLives);

    /**
     * Returns the Y position the controller should use for the boot.
     */
    int getBootControlY(const Rect& boot) const;

    /**
     * Uses the selected decision system to decide whether to jump this frame.
     */
    void decideNextMove(Rect& gap, Rect& boot);

    /**
     * Holds jump long enough to restart after the lives display reaches zero.
     */
    void handleGameOver();

    /**
     * Sends the E key through uinput in order to jump the jet boot.
     *
     * @param releaseDelay Delay in milliseconds to release the key press.
     */
    void sendJump(int releaseDelay);

    /**
     * Draws the detected boot, pillars, selected gap, lives, and control point.
     */
    void drawPreview(Mat gameFrame, Mat stateFrame, Rect boot, vector<Rect> pillarGaps, Rect gap, vector<Rect> lives);

public:
    JettyPlayer(int height, bool useAI, bool train, bool save, bool restore);
    ~JettyPlayer();

    /**
     * Processes one captured frame pair.
     */
    void sendFrame(Mat gameFrame, Mat stateFrame);

    /**
     * Sends a short jump for checking input injection while the app is running.
     */
    void testJump();
};

#endif //JETTYPLAYER_H
