//
// Created by Max on 3/27/2025.
//

#ifndef FRAMECAPTURE_H
#define FRAMECAPTURE_H

#include <windows.h>
#include <iostream>
#include <conio.h>
#include <tuple>

#include "opencv2/opencv.hpp"

using cv::Mat;
using std::cout;
using std::tuple;

class FrameCapture
{
private:
    int x, y = 0;
    int stateX, stateY = 0;

    void drawOverlay();
    void drawStateOverlay();

    Mat captureGameFrame();
    Mat captureGameState();

public:
    int width, height = 0;
    int stateWidth, stateHeight = 0;

    /**
     * Constructor for the FrameCapture Class.
     *
     * @param x X Coordinate of the top left start point.
     * @param y Y Coordinate of the top left start point.
     * @param width Width of the screen capture.
     * @param height Height of the screen capture.
     */
    FrameCapture(int x, int y, int width, int height);

    /**
     * Deconstructor for the FrameCapture Class.
     */
    ~FrameCapture();

    /**
     * Takes a capture of the currently displayed frame.
     *
	 * @return a tuple containing Mats. The first Mat represents the game, the second represents the game state.
     */
    tuple<Mat, Mat> captureFrame();

    void setUpCaptureFrame();
};

#endif //FRAMECAPTURE_H
