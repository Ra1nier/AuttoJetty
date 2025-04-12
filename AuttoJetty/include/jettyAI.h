#ifndef JETTYAI_H
#define JETTYAI_H

#include <torch/torch.h>

using torch::Tensor;
using std::vector;

/// <summary>
/// This is the AI model that will be used to predict the jump time.
/// </summary>
struct JettyAIImpl : torch::nn::Module
{
    JettyAIImpl();

    Tensor forward(Tensor x);

    torch::nn::Linear fc1{ nullptr }, fc2{ nullptr };
};

// Assign the model to a variable.
TORCH_MODULE(JettyAI);

/// <summary>
/// Defines an play episode for the AI.
/// </summary>
struct EpisodeStep
{
	EpisodeStep(Tensor state, Tensor action, float reward = 0.0f) : state(state), action(action), reward(reward) {}
    Tensor state;
    Tensor action;
    float reward;
};

/// <summary>
/// This is a wrapper for the JettyAI model and will be used for interacting with the AI model.
/// </summary>
class JettyBot
{
    public:
        JettyBot(bool train, bool save);
        ~JettyBot();

		int getJumpTime(int bootPosition, int gapTop, int gapBottom, float fallSpeed);
        void generateReward(bool crash);

        void recordAliveReward();
        void recordCrash();
		void finalizeEpisode();

        void close();
        void restore();

    private:
        JettyAI model;
		torch::optim::Adam* optimizer = nullptr;
        vector<EpisodeStep> episodes;

        bool trainAI = false;
        bool saveAI = false;
        bool gameOver = false;
        int livesLeft = 3;
		float totalReward = 0.0f;

        int train(Tensor data);
		int query(Tensor data);

        void reset();

};

#endif