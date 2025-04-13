//
// Created by Max on 3/27/2025.
//

#include "../include/jettyPlayer.h"

JettyPlayer::JettyPlayer(int width, int height, bool train, bool save, bool restore)
	: frameWidth(width), frameHeight(height), trainAI(train), saveAI(save)
{
	jettyBot = new JettyBot(trainAI, saveAI);
	runStartTime = std::chrono::steady_clock::now();
	readyToTrain = false;

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
	using namespace std::chrono;
	auto now = steady_clock::now();
	double elapsedSeconds = duration_cast<duration<double>>(now - runStartTime).count();

	if (!readyToTrain && elapsedSeconds < 1.5)
	{
		std::cout << "[System] Waiting for game to start (" << elapsedSeconds << "s)" << std::endl;
		return; // Skip frame
	}
	else if (!readyToTrain)
	{
		readyToTrain = true;
		std::cout << "[System] AI training active!" << std::endl;
	}

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

	vector<Rect> lives = getLivesLeft(stateFrame);
	int livesLeft = std::min(3, (int)lives.size());
	decideNextMove(gap, bootPosition, livesLeft);
	drawPreview(gameFrame, stateFrame, bootPosition, pillarGapPositions, gap, lives);
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

vector<Rect> JettyPlayer::getLivesLeft(Mat stateFrame)
{
	cv::Mat hsv;
	cv::cvtColor(stateFrame.clone(), hsv, cv::COLOR_BGR2HSV);

	// Define HSV range for the green life indicators (100, 69, 69)
	cv::Scalar lowerGreenVal(50, 100, 200);
	cv::Scalar upperGreenVal(55, 140, 255);

	cv::Mat livesMask;
	cv::inRange(hsv, lowerGreenVal, upperGreenVal, livesMask);

	// Merge contours to get the number of lives left
	cv::Mat cleaned;
	cv::morphologyEx(livesMask, cleaned, cv::MORPH_CLOSE, cv::Mat(), cv::Point(-1, -1), 2);

	// Get the number of contours
	vector<vector<cv::Point>> lives;
	Mat hierarchy;
	cv::findContours(cleaned, lives, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	// Filter contours by area to remove small noise.
	vector<Rect> lifeBoots;
	for (auto life : lives)
	{
		double area = cv::contourArea(life);
		if (area < 200.0 || area > 1000.0)
		{
			continue;
		}

		lifeBoots.push_back(cv::boundingRect(life));
	}

	return lifeBoots;
}

void JettyPlayer::decideNextMove(Rect& gap, Rect& boot, int livesLeft)
{
	static int prevBootY = boot.y;

	// Calculate fall speed
	int bootPositionY = boot.y + (boot.height / 2);
	int fallSpeed = std::abs(bootPositionY - prevBootY);
	prevBootY = bootPositionY;

	// Define the gap
	int gapTop = gap.y;
	int gapBottom = gap.y + gap.height;

	if (livesLeft > 0)
	{
		int jumpTime = jettyBot->getJumpTime(bootPositionY, gapTop, gapBottom, fallSpeed);
		sendJump(jumpTime);
	}

	// Update JettyBot with the game state if we are training.
	if (trainAI)
	{
		if (livesLeft < previousLives)
		{
			jettyBot->recordCrash(livesLeft);
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
	std::thread([releaseDelay]() {
		INPUT input = { 0 };
		input.type = INPUT_KEYBOARD;
		input.ki.wVk = 0x45; // E Key
		SendInput(1, &input, sizeof(INPUT));
		Sleep(releaseDelay / 3);
		input.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &input, sizeof(INPUT));
		}).detach();
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

void JettyPlayer::drawPreview(Mat gameFrame, Mat stateFrame, Rect boot, vector<Rect> pillarGaps, Rect gap, vector<Rect> lives)
{
	// Boot
	cv::rectangle(gameFrame, boot, cv::Scalar(0, 0, 255), 2);  // Red for boot
	cv::circle(gameFrame, cv::Point(boot.x + (boot.width / 2), boot.y + (boot.height / 2)), 5, cv::Scalar(0, 255, 255)); // Yellow for boot center

	// Pillars
	for (auto pillar : pillarGaps)
	{
		cv::rectangle(gameFrame, pillar, cv::Scalar(0, 255, 0), 2); // Green for pillar
	}
	cv::rectangle(gameFrame, gap, cv::Scalar(255, 0, 0), 2); // blue for gap

	// Lives
	if (!stateFrame.empty())
	{
		for (auto life : lives)
		{
			cv::rectangle(stateFrame, life, cv::Scalar(255, 0, 0), 2); // Green for pillar
		}

		cv::Mat resizedState;
		cv::resize(stateFrame, resizedState, cv::Size(gameFrame.cols, stateFrame.rows * gameFrame.cols / stateFrame.cols));

		// Combine horizontally
		cv::Mat combined;
		cv::vconcat(gameFrame, resizedState, combined);

		cv::imshow("Detected Objects", combined);
		cv::moveWindow("Detected Objects", 0, 0);
		makeWindowAlwaysOnTop("Detected Objects");
	}
	cv::waitKey(1);
}

void makeWindowAlwaysOnTop(const std::string& windowName)
{
	HWND hwnd = FindWindowA(NULL, windowName.c_str());
	if (hwnd != nullptr)
	{
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}
}
