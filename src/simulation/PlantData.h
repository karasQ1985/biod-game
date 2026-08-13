#pragma once

#include <vector>
#include <cstddef>

// Plant growth/scatter parameters (global, not per-flock)
struct PlantParams {
    float maxPlants = 689.0f;
    float initialPlants = 80.0f;
    float eatRange = 18.0f;        // How close a boid must be to eat (pixels)
    float plantFoodValue = 0.6f;   // Hunger restored by a fully-grown plant (0.0-1.0)
    float growthTime = 40.0f;      // Seconds from eaten to fully grown
    float spreadChance = 0.03f;    // Per-second probability a mature plant spawns offspring
    float spreadRange = 80.0f;     // Max distance offspring can appear from parent (pixels)
    float seasonLength = 60.0f;    // Seconds per season (spring/summer/autumn/winter cycle)
    float fertilizeRadius = 60.0f; // How far a death fertilizes nearby plants (pixels)
    float fertilizeBoost = 0.30f;  // Instant growth added to nearby plants when a boid starves
    float carryingPressure = 0.0f;  // Extra hunger decay when population exceeds carrying capacity (0.0=off)

    // Derived season growth multiplier: simTime / seasonLength determines season
    // Spring (0) = 1.5x, Summer (1) = 1.0x, Autumn (2) = 0.5x, Winter (3) = 0.0x
    static float growthMultiplier(int season) {
        const float multipliers[4] = {1.5f, 1.0f, 0.5f, 0.0f};
        return multipliers[season % 4];
    }
};

// SoA layout for plants (immobile, scattered across the world)
struct PlantData {
    std::vector<float> posX;
    std::vector<float> posY;
    std::vector<float> growth;       // 0.0 = dead/eaten, 1.0 = fully mature
    std::vector<float> regrowTimer;  // seconds until regrowth begins after being eaten
    int count = 0;

    void reserve(size_t capacity) {
        posX.reserve(capacity);
        posY.reserve(capacity);
        growth.reserve(capacity);
        regrowTimer.reserve(capacity);
    }

    void add(float x, float y, float initGrowth) {
        posX.push_back(x);
        posY.push_back(y);
        growth.push_back(initGrowth);
        regrowTimer.push_back(0.0f);
        ++count;
    }

    void removeAt(int index) {
        int last = count - 1;
        posX[index] = posX[last];
        posY[index] = posY[last];
        growth[index] = growth[last];
        regrowTimer[index] = regrowTimer[last];
        posX.pop_back();
        posY.pop_back();
        growth.pop_back();
        regrowTimer.pop_back();
        --count;
    }

    // Count living plants (growth > threshold, visible)
    int aliveCount(float threshold = 0.05f) const {
        int alive = 0;
        for (int i = 0; i < count; ++i) {
            if (growth[i] > threshold) ++alive;
        }
        return alive;
    }

    void clear() {
        posX.clear();
        posY.clear();
        growth.clear();
        regrowTimer.clear();
        count = 0;
    }
};
