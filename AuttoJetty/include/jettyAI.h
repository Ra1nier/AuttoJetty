#ifndef JETTYAI_H
#define JETTYAI_H

#include <vector>

#include <torch/torch.h>

using torch::Tensor;
using std::vector;

// Small neural network that turns normalized game state into a jump probability.
struct JettyAIImpl : torch::nn::Module
{
    JettyAIImpl();

    // Runs one forward pass and returns a value in the 0..1 range.
    Tensor forward(Tensor x);

    torch::nn::Linear fc1{ nullptr }, fc2{ nullptr };
};

// Torch module wrapper type for JettyAIImpl.
TORCH_MODULE(JettyAI);

// One recorded training step with its input state, action output, and reward.
struct EpisodeStep
{
	EpisodeStep(Tensor state, Tensor action, float reward = 0.0f) : state(state), action(action), reward(reward) {}
    Tensor state;
    Tensor action;
    float reward;
};

// Wraps the model, deterministic teacher controller, training, saving, and loading.
class JettyBot
{
    public:
        JettyBot(bool train, bool save);
        ~JettyBot();

        // Uses AI inference/training to decide whether the boot should jump.
        bool shouldJump(int bootPosition, int gapTop, int gapBottom, float fallSpeed);

        // Deterministic controller used directly in controller mode and as the AI teacher.
        bool controllerShouldJump(int bootPosition, int gapTop, int gapBottom, float fallSpeed);

        // Reward hooks called by JettyPlayer during AI training.
        void recordAliveReward();
        void recordScoreReward(int scoreGained);
        void recordCrash(int lives);
		void finalizeEpisode();

        // Persists or restores the model weights.
        void close();
        void restore();

    private:
        JettyAI model;
		torch::optim::Adam* optimizer = nullptr;
        vector<EpisodeStep> episodes;

        bool trainAI = false;
        bool saveAI = false;
		float totalReward = 0.0f;

        // Builds the normalized four-value input tensor used by both training and inference.
        Tensor makeInput(int bootPosition, int gapTop, int gapBottom, float fallSpeed);

        // Runs the model without modifying weights.
        bool queryJump(Tensor input);

        // Trains one frame against the deterministic controller's decision.
        bool trainJump(Tensor input, bool teacherJump);

        // Clears episode reward state after an episode finishes.
        void reset();

};

#endif
