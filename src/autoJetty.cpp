//
// Created by Max on 3/27/2025.
//

#include <iostream>

#include "opencv2/opencv.hpp"
#include "../include/frameCapture.h"

using cv::Mat;

int main()
{
    // Setup program
    int x = 0, y = 0;
    int width = 2560, height = 1440;

    // Start Frame Capture
    FrameCapture* frames = new FrameCapture(x, y, width, height);
    while (true)
    {
        // Capture frame
        Mat frame = frames->captureFrame();
        if (frame.empty())
        {
            break;
        }

        Mat greenPillars =
    }


    delete frames;
    return 0;
}

