#include "Game.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include "Audio.h"
#include "Constants.h"

Game::Game() :
    playerPos_({0.0f, 0.0f}),
    cameraPos_({0.0f, 0.0f}),
    playerRadius_(0.2f) {
    initWorld();
}

void Game::initWorld() {
    srand(time(nullptr));

    // Initialize cloud objects (parallax layer 0.2)
    for (int i = 0; i < 20; ++i) {
        float rx = (float(rand()) / RAND_MAX) * 100.0f;
        float ry = (float(rand()) / RAND_MAX) * 2.0f + 0.5f;
        cloudObjects_.push_back({rx, ry});
    }

    // Initialize tree objects (parallax layer 0.5)
    for (int i = 0; i < 40; ++i) {
        float rx = (float(rand()) / RAND_MAX) * 150.0f;
        float ry = (float(rand()) / RAND_MAX) * 0.5f - 1.5f;
        treeObjects_.push_back({rx, ry});
    }

    // Initialize static objects (same layer as player)
    for (int i = 0; i < 100; ++i) {
        float rx = (float(rand()) / RAND_MAX) * 200.0f;
        float ry = (float(rand()) / RAND_MAX) * 4.0f - 2.0f;
        staticObjects_.push_back({rx, ry});
    }
}

void Game::update(float dt) {
    // Move player to the right
    float speedX = 2.0f;
    playerPos_.x += speedX * dt;

    // Camera follows player
    cameraPos_.x = playerPos_.x;

    float pitch = Audio_getLastPitch();
    bool hasTargetY = (pitch > 0.0f);

    if (hasTargetY) {
        // Map pitch to target Y
        float normalized = (pitch - kVoicePitchMin) / (kVoicePitchMax - kVoicePitchMin);
        normalized = std::max(0.0f, std::min(1.0f, normalized));
        float targetY = normalized * (2.0f * kProjectionHalfHeight) - kProjectionHalfHeight;

        // Interpolate towards target Y for smoother movement
        float lerpFactor = 10.0f * dt;
        if (lerpFactor > 1.0f) lerpFactor = 1.0f;
        playerPos_.y = playerPos_.y + (targetY - playerPos_.y) * lerpFactor;
    }

    // Clamp Y to screen boundaries
    float maxY = kProjectionHalfHeight;
    if (playerPos_.y + playerRadius_ > maxY) playerPos_.y = maxY - playerRadius_;
    if (playerPos_.y - playerRadius_ < -maxY) playerPos_.y = -maxY + playerRadius_;
}
