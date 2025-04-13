#include "../include/jettyAI.h"

// ----------* AI Model *----------

// JettyAIImpl is a neural network model that takes 4 inputs and outputs a single value that represents a jump in milliseconds.
JettyAIImpl::JettyAIImpl() : fc1(4, 64), fc2(64, 1)
{
	register_module("fc1", fc1);
	register_module("fc2", fc2);
}

// The forward function defines the forward pass of the model.
Tensor JettyAIImpl::forward(Tensor x)
{
	x = torch::relu(fc1->forward(x)); // Turn 4 inputs into 64 tensors
	x = torch::sigmoid(fc2->forward(x)); // Merge 64 tensors into 1 tensor, the output tensor
	return x * 200.0f; // Convert output to 0–200ms jump
}

// ----------* JettyBot *----------

// JettyBot is a class that contains the AI model and is used to interact with the JettyAI. Essentially, it is a wrapper for the JettyAI.
JettyBot::JettyBot(bool train, bool save)
	: trainAI(train), saveAI(save), livesLeft(3)
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

int JettyBot::getJumpTime(int bootPosition, int gapTop, int gapBottom, float fallSpeed)
{
	Tensor input = torch::tensor({ (float)bootPosition, (float)gapTop, (float)gapBottom, fallSpeed }).unsqueeze(0);

	return trainAI ? train(input) : query(input);
}

int JettyBot::train(Tensor input)
{
	Tensor action = model->forward(input);

	// Randomize the action to add some noise for training
	action = action + torch::randn_like(action) * 10.0;
	action = torch::clamp(action, 0, 200);

	episodes.emplace_back(input.clone(), action.clone());

	return static_cast<int>(action.item<float>());
}

int JettyBot::query(torch::Tensor input)
{
	Tensor action = model->forward(input);
	return static_cast<int>(action.item<float>());
}

void JettyBot::generateReward(bool crash)
{
	float reward = crash ? -1000.0f : 1.0f;
	if (!episodes.empty())
	{
		episodes.back().reward = reward;
		totalReward += reward;
	}
}

void JettyBot::finalizeEpisode()
{
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

	livesLeft = lives;

	std::cout << "[AI] Crash recorded. Lives left: " << livesLeft << std::endl;

	if (livesLeft <= 0)
	{
		std::cout << "[AI] Episode over. Reward: " << totalReward << std::endl;
		finalizeEpisode();
		reset();
	}
}


void JettyBot::reset()
{
	livesLeft = 3;
	totalReward = 0.0f;
	episodes.clear();
}

void JettyBot::close()
{
	if (trainAI)
	{
		torch::save(model, "jettybot_model.pt");
	}
}

void JettyBot::restore()
{
	if (trainAI)
	{
		torch::load(model, "jettybot_model.pt");
	}
}