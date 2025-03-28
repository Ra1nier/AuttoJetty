//
// Created by Max on 3/27/2025.
//

#ifndef JETTYPLAYER_H
#define JETTYPLAYER_H

#include <iostream>
#include <vector>

#include "opencv2/opencv.hpp"

using std::vector;
using cv::Mat;

class JettyPlayer
{
    private:
        double bootHeight = 0.0;
        double pillarTop = 0.0;
        double pillarBottom = 0.0;

    public:
        JettyPlayer();
        ~JettyPlayer();

        void sendFrame(Mat frame);

};

#endif //JETTYPLAYER_H
