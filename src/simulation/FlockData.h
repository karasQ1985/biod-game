#pragma once

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

// SoA (Structure of Arrays) layout for cache-friendly memory access.
// All per-boid attributes are stored as separate contiguous arrays.
// IMPORTANT: No per-frame heap allocation; all vectors are pre-allocated via reserve().

struct FlockParams {
    // Reynolds rules (weights multiply normalized maxSpeed forces)
    float separationRadius = 30.0f;
    float alignmentRadius = 60.0f;
    float cohesionRadius = 60.0f;
    float separationWeight = 2.5f;
    float alignmentWeight = 0.8f;
    float cohesionWeight = 0.5f;

    // Boundary avoidance: soft repulsion from world edges
    float boundaryWeight = 2.0f;
    float boundaryMargin = 80.0f;

    // Random wander: organic noise for natural behavior
    float wanderWeight = 0.3f;

    // Target seeking
    float targetWeight = 3.0f;

    // Inter-flock repulsion: boids from different flocks repel
    float interFlockRepulsionWeight = 1.5f;

    // Predator-prey: strengths applied when this flock acts as predator or prey
    float predatorAttractionWeight = 2.0f;   // chase strength (as predator)
    float preyFearWeight = 3.0f;             // flee strength (as prey)

    // Hunger/satiety system: 0.0 = starved (dead), 1.0 = fully satiated
    // hunger decays each second by hungerDecayRate
    float hungerDecayRate = 0.008f;          // per-second decay (125s from full to death)

    // Speed modulation: when satiated, boids are slower/lazier; when hungry, faster/aggressive
    // Effective speed = maxSpeed * lerp(hungerSpeedMax, hungerSpeedMin, hunger) * weightSpeedMod
    // invertHungerSpeed reverses the curve: true = faster when full, slower when hungry
    float hungerSpeedMin = 0.6f;             // speed multiplier when fully satiated (slower)
    float hungerSpeedMax = 1.4f;             // speed multiplier when extremely hungry (faster)
    bool  invertHungerSpeed = false;          // reverse hunger-speed curve

    // Weight system: body mass affects speed and rendering size
    // weight range [minWeight, maxWeight], default 1.0 for all boids
    float weightSpeedPenalty = 0.3f;     // weight-to-speed penalty coefficient (0=no effect)
    float maxWeight = 2.0f;              // max body weight multiplier
    float minWeight = 0.5f;              // min body weight multiplier

    // Predation success/escape rates
    float chaseSuccessBase = 0.30f;          // base probability predator catches prey
    float escapeSuccessBase = 0.55f;         // base probability prey escapes
    float predationMinHunger = 0.50f;        // only hunt when hunger is below this threshold
    float predationKillHunger = 0.10f;       // below this hunger: 100% flock chases AND kills
    float predationParticipationRate = 0.10f; // base fraction of flock that chases when well-fed (0.01-1.0)

    // Kill streak & weight gain: consecutive kills build body mass
    float weightGainPerKill = 0.02f;      // weight increase per consecutive kill
    float weightDecayRate = 0.001f;       // weight lost per second when idle (beyond decayDelay)
    float streakTimeout = 3.0f;           // seconds between kills to count as consecutive
    float decayDelay = 5.0f;             // seconds idle before weight starts decaying

    // Chase range: how close predator must be to trigger a predation check (pixels)
    float chaseRange = 25.0f;

    // Visual: hunger flash when hunger drops below this threshold
    float hungerFlashThreshold = 0.30f;

    // Reproduction: per-flock population dynamics (sex-based pairing)
    // Triggered once per reproductionInterval (1 pseudo-quarter by default)
    float reproductionMinOffspring = 1.0f;  // Min offspring per male-female pair
    float reproductionMaxOffspring = 5.0f;  // Max offspring per male-female pair
    int   maxFlockSize = 2000;              // Hard cap: no new spawns when flock reaches this size
    float reproductionMinHunger = 0.60f;   // Min hunger (0.0-1.0) to be eligible (must be satiated)
    float reproductionInterval = 60.0f;    // Seconds between reproduction cycles (1 pseudo-quarter default)
    float adultAge = 240.0f;                // Seconds to reach full adult size (same as 1 pseudo-year default)

    // Foraging: active plant-seeking when hungry
    float forageRange = 150.0f;             // How far a boid can detect plants (pixels)
    float forageWeight = 2.0f;              // Turn force toward nearest plant
    float forageHungerThreshold = 0.5f;     // Hunger below this value activates foraging

    // Limits
    float maxSpeed = 300.0f;
    float maxForce = 3000.0f;
    float boidSize = 6.0f;

    // Appearance: sex-based coloring
    bool  useSexColors = false;
    float maleColorR = 0.50f, maleColorG = 0.70f, maleColorB = 0.95f;    // Blue-tinted male
    float femaleColorR = 0.95f, femaleColorG = 0.50f, femaleColorB = 0.55f; // Pink-tinted female

    // Sprite: per-flock texture (empty = use solid color)
    std::string spriteName;

    // Sprite upright mode: when true, sprite always stays upright (horizontal
    // mirror instead of 180-degree rotation when moving left)
    bool uprightSprite = false;

    // Hard collision radius in pixels (0 = no collision; slider 0-50, 1 unit = 2 px)
    float hardCollisionRadius = 14.0f;
};

// Default flock color palette (12 entries, recycled for user-added flocks)
inline constexpr float DEFAULT_FLOCK_COLORS[12][4] = {
    {0.80f, 0.30f, 0.30f, 1.0f},  // 0: Red
    {0.30f, 0.50f, 0.80f, 1.0f},  // 1: Blue
    {0.30f, 0.80f, 0.40f, 1.0f},  // 2: Green
    {0.90f, 0.70f, 0.20f, 1.0f},  // 3: Gold
    {0.20f, 0.80f, 0.85f, 1.0f},  // 4: Cyan
    {0.80f, 0.30f, 0.70f, 1.0f},  // 5: Magenta
    {0.95f, 0.55f, 0.20f, 1.0f},  // 6: Orange
    {0.55f, 0.30f, 0.75f, 1.0f},  // 7: Purple
    {0.40f, 0.85f, 0.25f, 1.0f},  // 8: Lime
    {0.15f, 0.55f, 0.55f, 1.0f},  // 9: Teal
    {0.90f, 0.45f, 0.40f, 1.0f},  // 10: Coral
    {0.65f, 0.65f, 0.70f, 1.0f},  // 11: Silver
};

struct FlockData {
    std::vector<float> posX;
    std::vector<float> posY;
    std::vector<float> velX;
    std::vector<float> velY;
    std::vector<int>   flockId;   // 0 = flock A, 1 = flock B, etc.
    std::vector<float> colorR;    // per-boid color for rendering
    std::vector<float> colorG;
    std::vector<float> colorB;
    std::vector<float> hunger;    // 0.0=starved(dead), 1.0=fully satiated
    std::vector<float> age;       // seconds since birth (0.0 = newborn)
    std::vector<uint8_t> sex;     // 0 = male, 1 = female

    // Weight / kill streak system: per-boid body mass and kill tracking
    std::vector<float> weight;      // body mass, default 1.0, range [minWeight, maxWeight]
    std::vector<int>   killStreak;  // consecutive kills count
    std::vector<float> lastKillTime;// simTime of last kill (-1e9f = never killed)
    int count = 0;

    void reserve(size_t capacity) {
        posX.reserve(capacity);
        posY.reserve(capacity);
        velX.reserve(capacity);
        velY.reserve(capacity);
        flockId.reserve(capacity);
        colorR.reserve(capacity);
        colorG.reserve(capacity);
        colorB.reserve(capacity);
        hunger.reserve(capacity);
        age.reserve(capacity);
        sex.reserve(capacity);
        weight.reserve(capacity);
        killStreak.reserve(capacity);
        lastKillTime.reserve(capacity);
    }

    void add(float x, float y, float vx, float vy, int fid, float cr, float cg, float cb,
             uint8_t s = 0) {
        posX.push_back(x);
        posY.push_back(y);
        velX.push_back(vx);
        velY.push_back(vy);
        flockId.push_back(fid);
        colorR.push_back(cr);
        colorG.push_back(cg);
        colorB.push_back(cb);
        hunger.push_back(0.8f);  // Start moderately satiated
        age.push_back(0.0f);     // Newborn age
        sex.push_back(s);
        weight.push_back(1.0f);       // Default body mass
        killStreak.push_back(0);      // No kills yet
        lastKillTime.push_back(-1e9f);// Never killed
        ++count;
    }

    // Swap-remove: O(1) removal, does not preserve order (order is irrelevant for boids)
    void removeAt(int index) {
        int last = count - 1;
        posX[index] = posX[last];
        posY[index] = posY[last];
        velX[index] = velX[last];
        velY[index] = velY[last];
        flockId[index] = flockId[last];
        colorR[index] = colorR[last];
        colorG[index] = colorG[last];
        colorB[index] = colorB[last];
        hunger[index] = hunger[last];
        age[index] = age[last];
        sex[index] = sex[last];
        weight[index] = weight[last];
        killStreak[index] = killStreak[last];
        lastKillTime[index] = lastKillTime[last];
        posX.pop_back();
        posY.pop_back();
        velX.pop_back();
        velY.pop_back();
        flockId.pop_back();
        colorR.pop_back();
        colorG.pop_back();
        colorB.pop_back();
        hunger.pop_back();
        age.pop_back();
        sex.pop_back();
        weight.pop_back();
        killStreak.pop_back();
        lastKillTime.pop_back();
        --count;
    }

    // Collect indices of boids that have starved to death (hunger <= 0)
    // Must be called after all updates, before removal.
    // Returns number of dead boids found.
    int collectDead(std::vector<int>& outDeadIndices) {
        outDeadIndices.clear();
        for (int i = 0; i < count; ++i) {
            if (hunger[i] <= 0.0f) {
                outDeadIndices.push_back(i);
            }
        }
        return static_cast<int>(outDeadIndices.size());
    }

    void clear() {
        posX.clear();
        posY.clear();
        velX.clear();
        velY.clear();
        flockId.clear();
        colorR.clear();
        colorG.clear();
        colorB.clear();
        hunger.clear();
        age.clear();
        sex.clear();
        weight.clear();
        killStreak.clear();
        lastKillTime.clear();
        count = 0;
    }
};
