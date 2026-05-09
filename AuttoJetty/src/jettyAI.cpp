#include "../include/jettyAI.h"

#include <iostream>

// JettyAIImpl is a neural network model that takes 4 normalized inputs and outputs a jump probability.
JettyAIImpl::JettyAIImpl() : fc1(4, 64), fc2(64, 1)
{
	register_module("fc1", fc1);
	register_module("fc2", fc2);
}

// The forward function defines the forward pass of the model.
Tensor JettyAIImpl::forward(Tensor x)
{
	x = torch::relu(fc1->forward(x)); // Turn 4 inputs into 64 tensors
	return torch::sigmoid(fc2->forward(x)); // Merge 64 tensors into a 0..1 jump probability
}

// JettyBot is a class that contains the AI model and is used to interact with the JettyAI. Essentially, it is a wrapper for the JettyAI.
JettyBot::JettyBot(bool train, bool save)
	: trainAI(train), saveAI(save)
{
	model = JettyAI();
	if (train)
	{
		optimizer = new torch::optim::Adam(model->parameters(), 1e-3);
	}
}

JettyBot::~JettyBot()
{
	if (optimizer)
	{
		delete optimizer;
	}
}

Tensor JettyBot::makeInput(int bootPosition, int gapTop, int gapBottom, float fallSpeed)
{
	int gapCenter = gapTop + ((gapBottom - gapTop) / 2);
	float targetError = static_cast<float>(bootPosition - gapCenter) / 500.0f;
	float normalizedBoot = static_cast<float>(bootPosition) / 1000.0f;
	float normalizedGapTop = static_cast<float>(gapTop) / 1000.0f;
	float normalizedFallSpeed = fallSpeed / 50.0f;

	return torch::tensor({ normalizedBoot, normalizedGapTop, targetError, normalizedFallSpeed }).unsqueeze(0);
}

bool JettyBot::controllerShouldJump(int bootPosition, int gapTop, int gapBottom, float fallSpeed)
{
	int gapCenter = gapTop + ((gapBottom - gapTop) / 2);
	int targetY = gapCenter + 8;
	int error = bootPosition - targetY;

	// Positive error means the boot is below the target. Account for downward velocity.
	return error + static_cast<int>(fallSpeed * 2.0f) > 10;
}

bool JettyBot::shouldJump(int bootPosition, int gapTop, int gapBottom, float fallSpeed)
{
	Tensor input = makeInput(bootPosition, gapTop, gapBottom, fallSpeed);
	bool teacherJump = controllerShouldJump(bootPosition, gapTop, gapBottom, fallSpeed);

	if (trainAI)
	{
		return trainJump(input, teacherJump);
	}

	return queryJump(input);
}

bool JettyBot::queryJump(Tensor input)
{
	torch::NoGradGuard noGrad;
	Tensor probability = model->forward(input).squeeze();
	return probability.item<float>() > 0.5f;
}

bool JettyBot::trainJump(Tensor input, bool teacherJump)
{
	// Store the supervised target so later survival/crash rewards can score the episode.
	Tensor target = torch::tensor({ teacherJump ? 1.0f : 0.0f }).unsqueeze(0);
	episodes.emplace_back(input.clone(), target.clone());

	Tensor probability = model->forward(input);
	Tensor loss = torch::binary_cross_entropy(probability, target);

	optimizer->zero_grad();
	loss.backward();
	optimizer->step();

	if (torch::rand({ 1 }).item<float>() < 0.05f)
	{
		return !teacherJump;
	}

	return queryJump(input);
}

void JettyBot::finalizeEpisode()
{
	if (episodes.size() < 2 || !optimizer)
	{
		reset();
		return;
	}

	std::cout << "Episode Reward: " << totalReward << std::endl;

	// Compute discounted rewards
	vector<float> discounted;
	float G = 0.0f;
	float gamma = 0.99f;
	for (int t = episodes.size() - 1; t >= 0; --t)
	{
		G = episodes[t].reward + gamma * G;
		discounted.insert(discounted.begin(), G);
	}

	// Normalize rewards
	Tensor rewards = torch::tensor(discounted);
	rewards = (rewards - rewards.mean()) / (rewards.std() + 1e-5);

	// Perform gradient descent
	optimizer->zero_grad();
	for (size_t i = 0; i < episodes.size(); ++i)
	{
		Tensor output = model->forward(episodes[i].state).squeeze();
		Tensor loss = torch::mse_loss(output, episodes[i].action.squeeze()) * rewards[i];
		loss.backward();
	}
	optimizer->step();

	if (saveAI)
	{
		close();
	}
}

void JettyBot::recordAliveReward()
{
	if (!episodes.empty())
	{
		episodes.back().reward += 1.0f;
		totalReward += 1.0f;
	}
}

void JettyBot::recordCrash(int lives)
{
	// Apply -1000 reward to the *last N steps*, not just the last frame.
	const int blameWindow = 3;
	int startIdx = std::max(0, (int)episodes.size() - blameWindow);
	for (int i = startIdx; i < (int)episodes.size(); ++i)
	{
		episodes[i].reward -= 1000.0f / blameWindow;
		totalReward -= 1000.0f / blameWindow;
	}

	std::cout << "[AI] Crash recorded. Lives left: " << lives << std::endl;

	if (lives <= 0)
	{
		std::cout << "[AI] Episode over. Reward: " << totalReward << std::endl;
		finalizeEpisode();
		reset();
	}
}

void JettyBot::recordScoreReward(int scoreGained)
{
	float reward = scoreGained * 10.0f;
	if (!episodes.empty())
	{
		episodes.back().reward += reward;
		totalReward += reward;
	}
}

void JettyBot::reset()
{
	totalReward = 0.0f;
	episodes.clear();
}

void JettyBot::close()
{
	torch::save(model, "jettybot_policy.pt");
}

void JettyBot::restore()
{
	try
	{
		torch::load(model, "jettybot_policy.pt");
		std::cout << "[AI] Restored jettybot_policy.pt" << std::endl;
	}
	catch (const c10::Error& error)
	{
		std::cerr << "[AI] Failed to restore jettybot_policy.pt: " << error.what() << std::endl;
	}
}
