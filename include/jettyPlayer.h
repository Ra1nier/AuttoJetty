//
// Created by Max on 3/27/2025.
//

#ifndef JETTYPLAYER_H
#define JETTYPLAYER_H

#include <windows.h>
#include <vector>

#include "opencv2/opencv.hpp"

using std::vector;
using cv::Mat;
using cv::Rect;

class JettyPlayer
{
private:
    // JettyPlayer specific
    double bootHeight = 0.0;
    double pillarTop = 0.0;
    double pillarBottom = 0.0;

    // External vars
    int frameWidth = 0;
    int frameHeight = 0;

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

    /**
     * Sends the E Key to windows in order to jump the jet boot.
     *
     * @param releaseDelay Delay in milliseconds to release the key press.
     */
    void sendJump(int releaseDelay);

public:
    JettyPlayer(int width, int height);
    ~JettyPlayer();

    /**
     * Sends the frame to the JettyBootPlayer for processing.
     *
     * @param frame Frame captured from the screen.
     */
    void sendFrame(Mat frame);
};

#endif //JETTYPLAYER_H
