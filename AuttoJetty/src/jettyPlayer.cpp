//
// Created by Max on 3/27/2025.
//

#include "../include/jettyPlayer.h"

JettyPlayer::JettyPlayer(int width, int height, bool train, bool save, bool restore)
	: frameWidth(width), frameHeight(height), trainAI(train), saveAI(save)
{
	jettyBot = new JettyBot(trainAI, saveAI);

	if (restore)
	{
		jettyBot->restore();
	}
}

JettyPlayer::~JettyPlayer()
{
	delete jettyBot;
}

void JettyPlayer::sendFrame(Mat gameFrame, Mat stateFrame)
{
	// Get necessary game components.
	Mat boot = extractBoot(gameFrame);
	Mat pillars = extractPillars(gameFrame);

	// Jump Calculations from the pillars and boot position
	Rect bootPosition = getBootPosition(boot);
	vector<Rect> pillarGapPositions = getPillarGapPosition(pillars);

	if (bootPosition.empty() || pillarGapPositions.empty())
	{
		return;
	}

	Rect gap = calculatePillarGap(bootPosition, pillarGapPositions);

	int livesLeft = getLivesLeft(stateFrame);
	decideNextMove(gap, bootPosition, livesLeft);
	drawPreview(gameFrame, bootPosition, pillarGapPositions, gap);
}

Rect JettyPlayer::getBootPosition(Mat bootFrame)
{
	vector<vector<cv::Point>> boot;
	Mat hierarchy;
	cv::findContours(bootFrame, boot, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	if (boot.empty())
	{
		return cv::Rect();
	}

	return cv::boundingRect(boot[0]);
}

vector<Rect> JettyPlayer::getPillarGapPosition(Mat pillarFrame)
{
	vector<vector<cv::Point>> pillars;
	Mat hierarchy;
	cv::findContours(pillarFrame, pillars, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	vector<Rect> pillarPositions;

	if (pillars.empty())
	{
		return pillarPositions;
	}

	for (auto pillar : pillars)
	{
		pillarPositions.push_back(cv::boundingRect(pillar));
	}

	return pillarPositions;
}

Rect JettyPlayer::calculatePillarGap(Rect& boot, vector<Rect> pillars)
{
	if (pillars.size() < 2) return Rect();

	int bootCenterX = boot.x + (boot.width / 2);

	// Sort pillars by x (left to right)
	std::sort(pillars.begin(), pillars.end(), [](const Rect& a, const Rect& b) { return a.x < b.x; });

	vector<Rect> nextPillarPair;
	for (size_t i = 0; i < pillars.size() - 1; i++)
	{
		if (pillars[i].x + pillars[i].width >= bootCenterX)
		{
			nextPillarPair.push_back(pillars[i]);
			nextPillarPair.push_back(pillars[i + 1]);
			break;
		}
	}

	if (nextPillarPair.size() < 2) return Rect();

	// Sort the pair by y to distinguish top and bottom pillars
	std::sort(nextPillarPair.begin(), nextPillarPair.end(), [](const Rect& a, const Rect& b) { return a.y < b.y; });

	Rect topPillar = nextPillarPair[0];   // Top pillar
	Rect bottomPillar = nextPillarPair[1]; // Bottom pillar

	// Compute the gap rectangle
	int gapX = bottomPillar.x;
	int gapY = topPillar.y + topPillar.height;
	int gapWidth = bottomPillar.width;
	int gapHeight = bottomPillar.y - gapY;

	// Make rect and add gap padding
	return Rect(gapX, gapY, gapWidth, gapHeight);
}

int JettyPlayer::getLivesLeft(Mat stateFrame)
{
	cv::Mat hsv;
	cv::cvtColor(stateFrame.clone(), hsv, cv::COLOR_BGR2HSV);

	// Define HSV range for the green life indicators (100, 69, 69)
	cv::Scalar lowerGreenVal(50, 100, 200);
	cv::Scalar upperGreenVal(55, 140, 255);

	cv::Mat livesMask;
	cv::inRange(hsv, lowerGreenVal, upperGreenVal, livesMask);

	// Get the number of contours
	vector<vector<cv::Point>> lives;
	Mat hierarchy;
	cv::findContours(livesMask, lives, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	std::cout << "You have " << lives.size() << " lives left." << std::endl;

	vector<Rect> lifePos;
	for (auto life : lives)
	{
		lifePos.push_back(cv::boundingRect(life));
	}

	for (auto life : lifePos)
	{
		cv::rectangle(stateFrame, life, cv::Scalar(255, 0, 0), 2); // Green for pillar
	}
	cv::imshow("Detected Lifes", stateFrame);

	return lives.size();
}

void JettyPlayer::decideNextMove(Rect& gap, Rect& boot, int livesLeft)
{
	if (livesLeft == 0)
	{
		return;
	}

	static int prevBootY = boot.y;

	// Calculate fall speed
	int bootPositionY = boot.y + boot.height / 2;
	int fallSpeed = std::abs(bootPositionY - prevBootY);
	prevBootY = bootPositionY;

	// Define the gap
	int gapTop = gap.y;
	int gapBottom = gap.y + gap.height;

	int jumpTime = jettyBot->getJumpTime(bootPositionY, gapTop, gapBottom, fallSpeed);
	sendJump(jumpTime);

	// Update JettyBot with the game state if we are training.
	if (trainAI)
	{
		if (livesLeft < previousLives)
		{
			jettyBot->recordCrash();
		}
		else
		{
			jettyBot->recordAliveReward();
		}

		previousLives = livesLeft;
	}

}

void JettyPlayer::sendJump(int releaseDelay)
{
	INPUT input = { 0 };
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = 0x45; // E Key
	SendInput(1, &input, sizeof(INPUT));

	Sleep(releaseDelay / 3);  // Tiny pause before next tap

	input.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &input, sizeof(INPUT));
}

Mat JettyPlayer::extractBoot(Mat frame)
{
	// Convert to windows color
	cv::Mat hsv;
	cv::cvtColor(frame.clone(), hsv, cv::COLOR_BGR2GRAY);

	// Extract whitish values.
	cv::Mat bootMask;
	cv::threshold(hsv, bootMask, 235, 255, cv::THRESH_BINARY);

	return bootMask;
}

Mat JettyPlayer::extractPillars(Mat frame)
{
	cv::Mat hsv;
	cv::cvtColor(frame.clone(), hsv, cv::COLOR_BGR2HSV);

	// Define HSV range for the lower green pipe
	cv::Scalar lowerGreenVal(50, 100, 200);
	cv::Scalar upperGreenVal(55, 140, 255);

	cv::Mat pillarMask;
	cv::inRange(hsv, lowerGreenVal, upperGreenVal, pillarMask);

	return pillarMask;
}

void JettyPlayer::drawPreview(Mat frame, Rect boot, vector<Rect> pillarGaps, Rect gap)
{
	cv::rectangle(frame, boot, cv::Scalar(0, 0, 255), 2);  // Red for boot
	for (auto pillar : pillarGaps)
	{
		cv::rectangle(frame, pillar, cv::Scalar(0, 255, 0), 2); // Green for pillar
	}
	cv::rectangle(frame, gap, cv::Scalar(255, 0, 0), 2); // blue for gap
	cv::imshow("Detected Objects", frame);
	cv::waitKey(1);
}
