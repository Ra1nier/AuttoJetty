//
// Created by Max on 3/27/2025.
//

#include "../include/jettyPlayer.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <linux/uinput.h>
#include <mutex>
#include <cerrno>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{
constexpr double MinBootContourArea = 35.0;
constexpr double MaxBootContourAreaRatio = 0.03;
constexpr double MaxBootWidthRatio = 0.18;
constexpr double MaxBootHeightRatio = 0.18;
constexpr double BootExpectedHeightRatio = 1.0 / 12.0;
constexpr double BootSearchMaxXRatio = 0.45;
constexpr double PipeHandoffBootRatio = 0.25;

int openUinputKeyboard()
{
	static bool warned = false;
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd < 0)
	{
		if (!warned)
		{
			std::cerr << "Failed to open /dev/uinput for key injection: " << std::strerror(errno)
				<< ". Add uinput permissions or run with access to /dev/uinput." << std::endl;
			warned = true;
		}
		return -1;
	}

	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 || ioctl(fd, UI_SET_KEYBIT, KEY_E) < 0)
	{
		std::cerr << "Failed to configure uinput key events: " << std::strerror(errno) << std::endl;
		close(fd);
		return -1;
	}

	uinput_setup setup{};
	std::snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "AuttoJetty Keyboard");
	setup.id.bustype = BUS_USB;
	setup.id.vendor = 0x1;
	setup.id.product = 0x1;
	setup.id.version = 1;

	if (ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0)
	{
		std::cerr << "Failed to create uinput keyboard: " << std::strerror(errno) << std::endl;
		close(fd);
		return -1;
	}

	std::cout << "[Input] uinput keyboard ready at /dev/uinput." << std::endl;
	usleep(100000);
	return fd;
}

int uinputKeyboard()
{
	static int fd = openUinputKeyboard();
	return fd;
}

void emitKeyEvent(int fd, int type, int code, int value)
{
	input_event event{};
	event.type = static_cast<unsigned short>(type);
	event.code = static_cast<unsigned short>(code);
	event.value = value;
	write(fd, &event, sizeof(event));
}
}

JettyPlayer::JettyPlayer(int height, bool useAIMode, bool train, bool save, bool restore)
	: frameHeight(height), useAI(useAIMode), trainAI(train), saveAI(save)
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
	if (trainAI && saveAI)
	{
		jettyBot->close();
	}
	delete jettyBot;
}

void JettyPlayer::sendFrame(Mat gameFrame, Mat stateFrame)
{
	using namespace std::chrono;
	auto now = steady_clock::now();
	double elapsedSeconds = duration_cast<duration<double>>(now - runStartTime).count();

	// Give the game a moment after startup/restart before trusting CV and lives state.
	if (!readyToTrain && elapsedSeconds < 1.5)
	{
		std::cout << "[System] Waiting for game to start (" << elapsedSeconds << "s)" << std::endl;
		return; // Skip frame
	}
	else if (!readyToTrain)
	{
		readyToTrain = true;
		std::cout << "[System] " << (useAI ? "AI" : "Controller") << " active!" << std::endl;
	}

	// Build binary masks for the objects the controller needs.
	Mat boot = extractBoot(gameFrame);
	Mat pillars = extractPillars(gameFrame);

	// Convert masks into rectangles so movement logic can use simple geometry.
	Rect bootPosition = getBootPosition(boot);
	vector<Rect> pillarGapPositions = getPillarGapPosition(pillars);

	// Lives are debounced because a single noisy state frame should not restart the game.
	vector<Rect> lives = getLivesLeft(stateFrame);
	int rawLives = std::min(3, (int)lives.size());
	int livesLeft = getStableLives(rawLives);
	static int debugFrameCounter = 0;
	if (++debugFrameCounter % 30 == 0)
	{
		std::cout << "[Debug] Boot detected: " << (!bootPosition.empty() ? "yes" : "no")
			<< " rect=(" << bootPosition.x << "," << bootPosition.y << " "
			<< bootPosition.width << "x" << bootPosition.height << ")"
			<< ", pillars: " << pillarGapPositions.size()
			<< ", lives: " << livesLeft << std::endl;
	}

	Rect gap;
	if (livesLeft <= 0)
	{
		handleGameOver();
	}

	// Only make a movement decision when both the boot and next obstacle are visible.
	if (!bootPosition.empty() && !pillarGapPositions.empty() && livesLeft > 0)
	{
		gap = calculatePillarGap(bootPosition, pillarGapPositions);
		decideNextMove(gap, bootPosition);
	}

	// In AI training mode, reward survival and penalize lost lives.
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

	// Score rewards are currently inert until getScoreFromFrame() is implemented.
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

	const double maxBootArea = static_cast<double>(bootFrame.cols * bootFrame.rows) * MaxBootContourAreaRatio;
	const int maxBootWidth = static_cast<int>(bootFrame.cols * MaxBootWidthRatio);
	const int maxBootHeight = static_cast<int>(bootFrame.rows * MaxBootHeightRatio);
	const int maxBootX = static_cast<int>(bootFrame.cols * BootSearchMaxXRatio);
	Rect bestBoot;
	double bestScore = 0.0;

	// Choose the strongest contour that still looks like a player boot, not a pipe base.
	for (const auto& contour : boot)
	{
		double area = cv::contourArea(contour);
		if (area < MinBootContourArea || area > maxBootArea)
		{
			continue;
		}

		Rect bounds = cv::boundingRect(contour);
		if (bounds.x > maxBootX || bounds.width > maxBootWidth || bounds.height > maxBootHeight)
		{
			continue;
		}

		double leftBias = 1.0 - (static_cast<double>(bounds.x) / std::max(bootFrame.cols, 1));
		double score = (area + static_cast<double>(bounds.height * bounds.width) * 0.25) * leftBias;
		if (score > bestScore)
		{
			bestScore = score;
			bestBoot = bounds;
		}
	}

	return bestBoot;
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

	for (const auto& pillar : pillars)
	{
		pillarPositions.push_back(cv::boundingRect(pillar));
	}

	return pillarPositions;
}

Rect JettyPlayer::calculatePillarGap(Rect& boot, vector<Rect> pillars)
{
	if (pillars.size() < 2) return Rect();

	int bootHandoffX = boot.x + static_cast<int>(boot.width * PipeHandoffBootRatio);

	// Sort pillars left-to-right, then keep the current pair until it is past the front quarter of the boot.
	std::sort(pillars.begin(), pillars.end(), [](const Rect& a, const Rect& b) { return a.x < b.x; });

	vector<Rect> nextPillarPair;
	for (size_t i = 0; i < pillars.size() - 1; i++)
	{
		int pillarRightEdge = pillars[i].x + pillars[i].width;
		if (pillarRightEdge >= bootHandoffX)
		{
			nextPillarPair.push_back(pillars[i]);
			nextPillarPair.push_back(pillars[i + 1]);
			break;
		}
	}

	if (nextPillarPair.size() < 2) return Rect();

	// Sort the pair vertically so the gap is the space between top and bottom pillars.
	std::sort(nextPillarPair.begin(), nextPillarPair.end(), [](const Rect& a, const Rect& b) { return a.y < b.y; });

	Rect topPillar = nextPillarPair[0];
	Rect bottomPillar = nextPillarPair[1];

	// Return a rectangle for preview drawing and controller target calculations.
	int gapX = bottomPillar.x;
	int gapY = topPillar.y + topPillar.height;
	int gapWidth = bottomPillar.width;
	int gapHeight = bottomPillar.y - gapY;

	return Rect(gapX, gapY, gapWidth, gapHeight);
}

vector<Rect> JettyPlayer::getLivesLeft(Mat stateFrame)
{
	cv::Mat hsv;
	cv::cvtColor(stateFrame.clone(), hsv, cv::COLOR_BGR2HSV);

	// Lives use the same bright green family as the top pipe pieces in the state capture.
	cv::Scalar lowerGreenVal(50, 100, 200);
	cv::Scalar upperGreenVal(55, 140, 255);

	cv::Mat livesMask;
	cv::inRange(hsv, lowerGreenVal, upperGreenVal, livesMask);

	// Close tiny gaps so each life icon becomes one contour.
	cv::Mat cleaned;
	cv::morphologyEx(livesMask, cleaned, cv::MORPH_CLOSE, cv::Mat(), cv::Point(-1, -1), 2);

	vector<vector<cv::Point>> lives;
	Mat hierarchy;
	cv::findContours(cleaned, lives, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	// Keep only icon-sized contours.
	vector<Rect> lifeBoots;
	for (const auto& life : lives)
	{
		double area = cv::contourArea(life);
		if (area < 100.0 || area > 3000.0)
		{
			continue;
		}

		lifeBoots.push_back(cv::boundingRect(life));
	}

	return lifeBoots;
}

int JettyPlayer::getStableLives(int currentLives)
{
	static int stableLives = 3;
	static int stabilityCounter = 0;

	// Require several consecutive lower readings before accepting a lost life.
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

int JettyPlayer::getBootControlY(const Rect& boot) const
{
	int centerY = boot.y + (boot.height / 2);
	const int expectedBootHeight = std::max(24, static_cast<int>(frameHeight * BootExpectedHeightRatio));

	// If CV only sees the lower part of the boot, estimate where the full boot center should be.
	if (boot.height < expectedBootHeight)
	{
		centerY -= (expectedBootHeight - boot.height) / 2;
	}

	return std::clamp(centerY, 0, std::max(frameHeight - 1, 0));
}

int JettyPlayer::getScoreFromFrame(cv::Mat stateFrame)
{
	(void)stateFrame;
	return previousScore;
}

void JettyPlayer::decideNextMove(Rect& gap, Rect& boot)
{
	static auto lastJumpTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(500);

	// Fall speed is the frame-to-frame change of the corrected boot control point.
	int bootPositionY = getBootControlY(boot);
	int verticalSpeed = prevBootY > 0 ? bootPositionY - prevBootY : 0;
	prevBootY = bootPositionY;

	// The controller only needs the top and bottom Y bounds of the chosen gap.
	int gapTop = gap.y;
	int gapBottom = gap.y + gap.height;

	// Throttle jumps so holding or rapid re-triggering does not flood input.
	auto now = std::chrono::steady_clock::now();
	auto msSinceJump = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastJumpTime).count();
	bool shouldJump = useAI
		? jettyBot->shouldJump(bootPositionY, gapTop, gapBottom, verticalSpeed)
		: jettyBot->controllerShouldJump(bootPositionY, gapTop, gapBottom, std::abs(verticalSpeed));

	if (shouldJump && msSinceJump > 110)
	{
		sendJump(90);
		lastJumpTime = now;
	}
}

void JettyPlayer::handleGameOver()
{
	static auto lastRestartAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(5);
	auto now = std::chrono::steady_clock::now();
	auto msSinceRestart = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRestartAttempt).count();

	// Restart attempts are rate-limited so zero-lives noise does not hold E continuously.
	if (msSinceRestart < 2500)
	{
		return;
	}

	std::cout << "[System] No lives detected. Holding E to restart next round." << std::endl;
	sendJump(2000);
	lastRestartAttempt = now;
	readyToTrain = false;
	runStartTime = now;
	previousLives = 3;
}

void JettyPlayer::sendJump(int releaseDelay)
{
	std::thread([releaseDelay]() {
		// Serialize key events because jumps are dispatched from detached worker threads.
		static std::mutex inputMutex;
		std::lock_guard<std::mutex> lock(inputMutex);

		int fd = uinputKeyboard();
		if (fd < 0)
		{
			return;
		}

		emitKeyEvent(fd, EV_KEY, KEY_E, 1);
		emitKeyEvent(fd, EV_SYN, SYN_REPORT, 0);
		usleep(static_cast<useconds_t>(std::max(releaseDelay / 3, 0) * 1000));
		emitKeyEvent(fd, EV_KEY, KEY_E, 0);
		emitKeyEvent(fd, EV_SYN, SYN_REPORT, 0);
	}).detach();
}

void JettyPlayer::testJump()
{
	std::cout << "[Input] Sending manual test jump." << std::endl;
	sendJump(150);
}


Mat JettyPlayer::extractBoot(Mat frame)
{
	// The boot is mostly light, but the mask must explicitly remove green pipe pixels.
	cv::Mat gray;
	cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

	cv::Mat hsv;
	cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

	// Extract the light boot body, then join nearby highlights so the box is not just the bright sole.
	cv::Mat bootMask;
	cv::threshold(gray, bootMask, 170, 255, cv::THRESH_BINARY);

	// Darker pipe bases can pass the grayscale threshold, so remove green before contour selection.
	cv::Mat greenMask;
	cv::inRange(hsv, cv::Scalar(35, 40, 35), cv::Scalar(90, 255, 255), greenMask);
	cv::bitwise_and(bootMask, ~greenMask, bootMask);

	cv::morphologyEx(bootMask, bootMask, cv::MORPH_CLOSE, cv::Mat(), cv::Point(-1, -1), 2);
	cv::dilate(bootMask, bootMask, cv::Mat(), cv::Point(-1, -1), 1);

	return bootMask;
}

Mat JettyPlayer::extractPillars(Mat frame)
{
	cv::Mat hsv;
	cv::cvtColor(frame.clone(), hsv, cv::COLOR_BGR2HSV);

	// Bright green captures the pipe faces used to calculate the gap.
	cv::Scalar lowerGreenVal(50, 100, 200);
	cv::Scalar upperGreenVal(55, 140, 255);

	cv::Mat pillarMask;
	cv::inRange(hsv, lowerGreenVal, upperGreenVal, pillarMask);

	return pillarMask;
}

void JettyPlayer::drawPreview(Mat gameFrame, Mat stateFrame, Rect boot, vector<Rect> pillarGaps, Rect gap, vector<Rect> lives)
{
	// Red rectangle is the detected boot; yellow dot is the corrected control point.
	cv::rectangle(gameFrame, boot, cv::Scalar(0, 0, 255), 2);
	if (!boot.empty())
	{
		cv::circle(gameFrame, cv::Point(boot.x + (boot.width / 2), getBootControlY(boot)), 5, cv::Scalar(0, 255, 255)); // Yellow for boot control point
	}

	// Green rectangles are pipe contours; blue rectangle is the selected gap.
	for (const auto& pillar : pillarGaps)
	{
		cv::rectangle(gameFrame, pillar, cv::Scalar(0, 255, 0), 2);
	}
	cv::rectangle(gameFrame, gap, cv::Scalar(255, 0, 0), 2);

	// State preview annotates each detected life icon.
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

	// Stack game and state previews so all CV output is visible in one window.
	cv::Mat combined;
	cv::vconcat(std::vector<cv::Mat>{gameFrame, resizedState}, combined);

	cv::imshow("Detected Objects", combined);
	cv::moveWindow("Detected Objects", 0, 0);
	makeWindowAlwaysOnTop("Detected Objects");

	cv::waitKey(1);
}

void makeWindowAlwaysOnTop(const std::string& windowName)
{
	(void)windowName;
}
