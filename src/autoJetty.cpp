//
// Created by Max on 3/27/2025.
//

#include <iostream>

#include "opencv2/opencv.hpp"
#include "../include/frameCapture.h"
#include "../include/jettyPlayer.h"

using cv::Mat;

int main()
{
    // Setup program
    int x = 0, y = 0;
    int width = 2560, height = 1440;

    // Start JettyPlayer
    JettyPlayer* player = new JettyPlayer(width, height);

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

        // Send the frame to the player
        player->sendFrame(frame);

        // Exit app on ESC
        if (cv::waitKey(30) == 27) break;
    }


    // Clean up and end
    delete frames;
    delete player;
    return 0;
}

