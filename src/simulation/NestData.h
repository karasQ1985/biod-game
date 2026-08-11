#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>

// Global nest parameters (Phase 3.1)
struct NestParams {
    float maxNests = 20.0f;          // Global cap on nest count
    float initialNests = 4.0f;       // Nests spawned at world creation
    float nestRadius = 150.0f;       // Territory influence radius (pixels)
    float nestHealthBoost = 2.0f;    // Health regen multiplier when resting at owned nest
    float nestFoodStorageRate = 0.05f; // Food stored per second from excess boid hunger
    float contestDuration = 10.0f;   // Seconds of uncontested presence before ownership flips
    float defenseThreshold = 3.0f;   // Minimum boids needed to mount a nest defense
};

// Per-flock nest preferences (Phase 3.1)
struct NestPrefSuf {
    float nestPreferFoodDensity = 0.7f;  // Weight: prefer food-rich areas [0,1]
    float nestPreferSafety = 0.6f;       // Weight: prefer safe areas (away from predators) [0,1]
    float nestSelectionRange = 300.0f;   // Search radius for nest site (pixels)
    float nestReturnWeight = 0.4f;       // Attraction force weight toward owned nest
};

// SoA layout for nests (public spatial entities, like plants)
struct NestData {
    std::vector<float> posX;
    std::vector<float> posY;
    std::vector<int>    ownerFlock;      // Flock ID that owns this nest, -1 = unowned
    std::vector<float>  foodStored;      // Accumulated food reserve [0.0, 1.0]
    std::vector<float>  defenseRating;   // Current defensive capability (based on nearby flock boids)
    std::vector<uint8_t> isContested;    // 1 = under active contest
    std::vector<float>  contestTimer;    // Seconds of attacker presence (resets when attacker leaves)
    std::vector<int>    contestAttacker; // Flock ID currently contesting (-1 = none)
    int count = 0;

    void reserve(size_t capacity) {
        posX.reserve(capacity);
        posY.reserve(capacity);
        ownerFlock.reserve(capacity);
        foodStored.reserve(capacity);
        defenseRating.reserve(capacity);
        isContested.reserve(capacity);
        contestTimer.reserve(capacity);
        contestAttacker.reserve(capacity);
    }

    void add(float x, float y, int owner) {
        posX.push_back(x);
        posY.push_back(y);
        ownerFlock.push_back(owner);
        foodStored.push_back(0.0f);
        defenseRating.push_back(0.0f);
        isContested.push_back(0);
        contestTimer.push_back(0.0f);
        contestAttacker.push_back(-1);
        ++count;
    }

    void removeAt(int index) {
        int last = count - 1;
        posX[index] = posX[last];
        posY[index] = posY[last];
        ownerFlock[index] = ownerFlock[last];
        foodStored[index] = foodStored[last];
        defenseRating[index] = defenseRating[last];
        isContested[index] = isContested[last];
        contestTimer[index] = contestTimer[last];
        contestAttacker[index] = contestAttacker[last];
        posX.pop_back();
        posY.pop_back();
        ownerFlock.pop_back();
        foodStored.pop_back();
        defenseRating.pop_back();
        isContested.pop_back();
        contestTimer.pop_back();
        contestAttacker.pop_back();
        --count;
    }

    void clear() {
        posX.clear();
        posY.clear();
        ownerFlock.clear();
        foodStored.clear();
        defenseRating.clear();
        isContested.clear();
        contestTimer.clear();
        contestAttacker.clear();
        count = 0;
    }

    // Number of nests owned by a specific flock
    int ownedBy(int flockId) const {
        int n = 0;
        for (int i = 0; i < count; ++i) {
            if (ownerFlock[i] == flockId) ++n;
        }
        return n;
    }
};
