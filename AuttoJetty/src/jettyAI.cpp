#include "../include/jettyAI.h"

#include <algorithm>
#include <iostream>

namespace
{
constexpr size_t ReplayCapacity = 4096;
constexpr int64_t BatchSize = 64;
constexpr size_t SaveInterval = 500;
constexpr double LearningRate = 3e-4;
}

JettyAIImpl::JettyAIImpl() : fc1(4, 64), fc2(64, 1)
{
	register_module("fc1", fc1);
	register_module("fc2", fc2);
	policyVersion = register_buffer("policy_version", torch::tensor(2, torch::kInt64));
}

Tensor JettyAIImpl::forward(Tensor x)
{
	x = torch::relu(fc1->forward(x));
	return torch::sigmoid(fc2->forward(x));
}

JettyBot::JettyBot(bool train, bool save)
	: model(JettyAI()), trainAI(train), saveAI(save)
{
	initializePolicy();
	if (trainAI)
	{
		optimizer = std::make_unique<torch::optim::Adam>(
			model->parameters(), torch::optim::AdamOptions(LearningRate));
		model->train();
	}
	else
	{
		model->eval();
	}
}

JettyBot::~JettyBot() = default;

void JettyBot::initializePolicy()
{
	// Represent the safe controller boundary with two ReLU units. This gives a
	// new model useful behavior immediately while leaving the remaining units
	// available for online learning.
	torch::NoGradGuard noGrad;
	model->fc1->weight.zero_();
	model->fc1->bias.zero_();
	model->fc2->weight.zero_();
	model->fc2->bias.zero_();

	// Inputs 0 and 1 are error/100 and vertical speed/20. The resulting logit is
	// (error + 2 * verticalSpeed - 10) / 10.
	model->fc1->weight.index_put_({0, 0}, 10.0f);
	model->fc1->weight.index_put_({0, 1}, 4.0f);
	model->fc1->bias.index_put_({0}, -1.0f);
	model->fc1->weight.index_put_({1, 0}, -10.0f);
	model->fc1->weight.index_put_({1, 1}, -4.0f);
	model->fc1->bias.index_put_({1}, 1.0f);
	model->fc2->weight.index_put_({0, 0}, 1.0f);
	model->fc2->weight.index_put_({0, 1}, -1.0f);
}

Tensor JettyBot::makeInput(int bootPosition, int gapTop, int gapBottom, float fallSpeed)
{
	const float gapCenter = static_cast<float>(gapTop + gapBottom) * 0.5f;
	const float halfGap = std::max(static_cast<float>(gapBottom - gapTop) * 0.5f, 1.0f);
	const float targetError = static_cast<float>(bootPosition) - (gapCenter + 8.0f);

	// Relative, bounded features remain meaningful at different capture sizes.
	return torch::tensor({
		std::clamp(targetError / 100.0f, -5.0f, 5.0f),
		std::clamp(fallSpeed / 20.0f, -5.0f, 5.0f),
		std::clamp(halfGap / 100.0f, 0.0f, 5.0f),
		std::clamp((static_cast<float>(bootPosition) - gapCenter) / halfGap, -5.0f, 5.0f)
	}, torch::TensorOptions().dtype(torch::kFloat32)).unsqueeze(0);
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
	if (trainAI)
	{
		const bool teacherJump = controllerShouldJump(
			bootPosition, gapTop, gapBottom, fallSpeed);
		return trainJump(input, teacherJump);
	}

	return queryJump(input);
}

bool JettyBot::queryJump(Tensor input)
{
	torch::NoGradGuard noGrad;
	const Tensor probability = model->forward(input).squeeze();
	return probability.item<float>() >= 0.5f;
}

bool JettyBot::trainJump(Tensor input, bool teacherJump)
{
	const Tensor target = torch::tensor(
		{{teacherJump ? 1.0f : 0.0f}}, torch::TensorOptions().dtype(torch::kFloat32));
	episodes.emplace_back(input.detach().clone(), target);
	if (episodes.size() > ReplayCapacity)
	{
		episodes.erase(episodes.begin());
	}

	const int64_t sampleCount = std::min<int64_t>(BatchSize, episodes.size());
	const Tensor indices = torch::randperm(static_cast<int64_t>(episodes.size()))
		.slice(0, 0, sampleCount);
	vector<Tensor> states;
	vector<Tensor> targets;
	states.reserve(static_cast<size_t>(sampleCount));
	targets.reserve(static_cast<size_t>(sampleCount));
	for (int64_t i = 0; i < sampleCount; ++i)
	{
		const size_t index = static_cast<size_t>(indices[i].item<int64_t>());
		states.push_back(episodes[index].state);
		targets.push_back(episodes[index].action);
	}

	const Tensor probabilities = model->forward(torch::cat(states, 0));
	const Tensor loss = torch::binary_cross_entropy(probabilities, torch::cat(targets, 0));
	optimizer->zero_grad();
	loss.backward();
	torch::nn::utils::clip_grad_norm_(model->parameters(), 1.0);
	optimizer->step();
	++trainingSteps;

	if (saveAI && trainingSteps % SaveInterval == 0)
	{
		close();
	}

	return queryJump(input);
}

void JettyBot::finalizeEpisode()
{
	std::cout << "[AI] Episode reward: " << totalReward
		<< ", replay examples: " << episodes.size() << std::endl;
	if (saveAI)
	{
		close();
	}
	totalReward = 0.0f;
}

void JettyBot::recordAliveReward()
{
	totalReward += 1.0f;
}

void JettyBot::recordCrash(int lives)
{
	totalReward -= 1000.0f;
	std::cout << "[AI] Crash recorded. Lives left: " << lives << std::endl;
	if (lives <= 0)
	{
		finalizeEpisode();
	}
}

void JettyBot::recordScoreReward(int scoreGained)
{
	totalReward += static_cast<float>(scoreGained) * 10.0f;
}

void JettyBot::close()
{
	try
	{
		torch::save(model, "jettybot_policy.pt");
		std::cout << "[AI] Saved jettybot_policy.pt" << std::endl;
	}
	catch (const c10::Error& error)
	{
		std::cerr << "[AI] Failed to save jettybot_policy.pt: " << error.what() << std::endl;
	}
}

void JettyBot::restore()
{
	try
	{
		torch::load(model, "jettybot_policy.pt");
		if (model->policyVersion.item<int64_t>() != 2)
		{
			std::cerr << "[AI] Policy format is outdated; using the default policy." << std::endl;
			initializePolicy();
			return;
		}
		model->eval();
		if (trainAI)
		{
			model->train();
		}
		std::cout << "[AI] Restored jettybot_policy.pt" << std::endl;
	}
	catch (const c10::Error& error)
	{
		std::cerr << "[AI] Failed to restore jettybot_policy.pt; using the default policy: "
			<< error.what() << std::endl;
		initializePolicy();
	}
}
