//
// Created by Max on 3/27/2025.
//

#include "../include/jettyPlayer.h"

JettyPlayer::JettyPlayer(int width, int height, bool train, bool save, bool restore)
	: frameWidth(width), frameHeight(height), trainAI(train), saveAI(save), previousLives(3)
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

	vector<Rect> lives = getLivesLeft(stateFrame);
	int rawLives = std::min(3, (int)lives.size());
	int livesLeft = getStableLives(rawLives);

	Rect gap;
	if (!bootPosition.empty() && !pillarGapPositions.empty() && livesLeft > 0)
	{
		gap = calculatePillarGap(bootPosition, pillarGapPositions);
		decideNextMove(gap, bootPosition);
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

	int currentScore = getScoreFromFrame(stateFrame);
	int scoreDelta = currentScore - previousScore;

	if (scoreDelta > 0)
	{
		jettyBot->recordScoreReward(scoreDelta);
	}
	previousScore = currentScore;


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
		if (area < 100.0 || area > 3000.0)
		{
			continue;
		}

		lifeBoots.push_back(cv::boundingRect(life));
	}

	std::cout << "[Debug] Lives detected: " << lifeBoots.size() << std::endl;

	return lifeBoots;
}

int JettyPlayer::getStableLives(int currentLives)
{
	static int stableLives = 3;
	static int stabilityCounter = 0;

	if (currentLives < stableLives)
	{
		stabilityCounter++;
		if (stabilityCounter >= 5)
		{
			stableLives = currentLives;
			stabilityCounter = 0;
		}
	}
	else if (currentLives > stableLives)
	{
		stableLives = currentLives;
		stabilityCounter = 0;
	}
	else
	{
		stabilityCounter = 0;
	}
	return stableLives;
}

int JettyPlayer::getScoreFromFrame(cv::Mat stateFrame)
{
	// Assume cropped region contains the score text
	cv::Rect scoreRegion(...); // ← define this rectangle manually
	cv::Mat scoreCrop = stateFrame(scoreRegion);

	// Preprocess: grayscale + threshold
	cv::Mat gray;
	cv::cvtColor(scoreCrop, gray, cv::COLOR_BGR2GRAY);
	cv::threshold(gray, gray, 200, 255, cv::THRESH_BINARY);

	// Use OCR (Tesseract or other) — for now fake it:
	// return fakeScore;

	// You can use tesseract or template-matching to read the digits
}

void JettyPlayer::decideNextMove(Rect& gap, Rect& boot)
{
	static int prevBootY = boot.y;

	// Calculate fall speed
	int bootPositionY = boot.y + (boot.height / 2);
	int fallSpeed = std::abs(bootPositionY - prevBootY);
	prevBootY = bootPositionY;

	// Define the gap
	int gapTop = gap.y;
	int gapBottom = gap.y + gap.height;

	int jumpTime = jettyBot->getJumpTime(bootPositionY, gapTop, gapBottom, fallSpeed);
	sendJump(jumpTime);
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
	cv::rectangle(gameFrame, gap, cv::Scalar(255, 0, 0), 2); // Blue for gap

	// Lives
	for (size_t i = 0; i < lives.size(); ++i)
	{
		cv::rectangle(stateFrame, lives[i], cv::Scalar(255, 0, 0), 2);
		cv::putText(stateFrame, "L" + std::to_string(i + 1),
			cv::Point(lives[i].x, lives[i].y - 5),
			cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
	}

	cv::Mat resizedState;

	if (!stateFrame.empty())
	{
		cv::resize(stateFrame, resizedState, cv::Size(gameFrame.cols, stateFrame.rows * gameFrame.cols / stateFrame.cols));
	}
	else
	{
		resizedState = cv::Mat::zeros(100, gameFrame.cols, gameFrame.type());
	}

	// Combine vertically: game + state + gameOver
	cv::Mat combined;
	cv::vconcat(std::vector<cv::Mat>{gameFrame, resizedState}, combined);

	cv::imshow("Detected Objects", combined);
	cv::moveWindow("Detected Objects", 0, 0);
	makeWindowAlwaysOnTop("Detected Objects");

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
