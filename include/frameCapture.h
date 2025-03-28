//
// Created by Max on 3/27/2025.
//

#ifndef FRAMECAPTURE_H
#define FRAMECAPTURE_H

#include <windows.h>
#include "opencv2/opencv.hpp"

using cv::Mat;

class FrameCapture
{
private:
    int x;
    int y;
    int width;
    int height;

public:
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
     * @return cv::Mat representing the captured frame.
     */
    Mat captureFrame();

};

#endif //FRAMECAPTURE_H
