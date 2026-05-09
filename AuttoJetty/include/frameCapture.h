//
// Created by Max on 3/27/2025.
//

#ifndef FRAMECAPTURE_H
#define FRAMECAPTURE_H

#include <iostream>
#include <memory>
#include <string>
#include <tuple>

#include "opencv2/opencv.hpp"

using cv::Mat;
using std::cout;
using std::tuple;

struct PipeWireCapture;

// Owns Wayland/PipeWire screen capture and splits each frame into game and state regions.
class FrameCapture
{
private:
    // Game capture rectangle.
    int x, y = 0;

    // State capture rectangle, used for lives/score UI.
    int stateX, stateY = 0;

    // Platform capture implementation is hidden from the header.
    std::unique_ptr<PipeWireCapture> capture;

    // Lets the user confirm or edit the game/state capture rectangles.
    void configureCaptureRegions();

    // Copies the latest full desktop frame out of PipeWire.
    bool captureScreen(Mat& screen);

    // Shows the editable setup overlay with both capture rectangles.
    void showCaptureRegionPreview(const Mat& screen);

    // Clips a requested rectangle to the screen before cloning the crop.
    Mat cropRegion(const Mat& screen, int captureX, int captureY, int captureWidth, int captureHeight);

public:
    // Game capture size.
    int width, height = 0;

    // State capture size.
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

    ~FrameCapture();

    /**
     * Takes a capture of the currently displayed frame.
     *
	 * @return a tuple containing Mats. The first Mat represents the game, the second represents the game state.
     */
    tuple<Mat, Mat> captureFrame();

    /**
     * Initializes default state-region coordinates and opens the setup preview.
     */
    void setUpCaptureFrame();
};

#endif //FRAMECAPTURE_H
