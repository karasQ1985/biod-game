#pragma once

#include "NestData.h"
#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

// Priority-ordered state machine for individual boids.
// Lower numeric value = higher priority (survival first).
enum class BoidState : uint8_t {
    IDLE     = 0,
    FLEEING  = 1,
    HUNTING  = 2,
    FORAGING = 3,
    // Reserved for future expansion:
    // RESTING, MATING, FIGHTING, DEFENDING, NURSING
};

// Age stages for individual boids.
// Lifecycle: Juvenile -> Young -> Adult -> Elder -> Death
enum class AgeStage : uint8_t {
    Juvenile = 0,  // Not yet reproductive, slower, smaller
    Young    = 1,  // Reproductive, growing toward peak
    Adult    = 2,  // Peak performance
    Elder    = 3,  // Declining, near death
};

// ============================================================================
// FlockParams sub-structs -- hierarchically split by functional domain.
// Each sub-struct is standard-layout to support ParamRegistry offsetof().
// Sub-struct naming: Suf struct starts with capital letter (e.g. MovementSuf).
// ============================================================================

struct MovementSuf {
    float maxSpeed = 300.0f;
    float maxForce = 3000.0f;
    float boidSize = 6.0f;
    float hardCollisionRadius = 14.0f;
    float weightSpeedPenalty = 0.3f;
};

struct PerceptionSuf {
    float separationRadius = 30.0f;
    float alignmentRadius = 60.0f;
    float cohesionRadius = 60.0f;
    float separationWeight = 2.5f;
    float alignmentWeight = 0.8f;
    float cohesionWeight = 0.5f;
};

struct BoundarySuf {
    float boundaryWeight = 2.0f;
    float boundaryMargin = 80.0f;
    float wanderWeight = 0.3f;
    float targetWeight = 1.5f;
};

struct HungerSuf {
    float hungerDecayRate = 0.008f;
    float hungerSpeedMin = 0.6f;
    float hungerSpeedMax = 1.4f;
    bool  invertHungerSpeed = false;
    float hungerFlashThreshold = 0.30f;
    float forageRange = 150.0f;
    float forageWeight = 2.0f;
    float forageHungerThreshold = 0.5f;
};

struct PredationSuf {
    float chaseSuccessBase = 0.30f;
    float escapeSuccessBase = 0.55f;
    float predationMinHunger = 0.50f;
    float predationKillHunger = 0.10f;
    float predationParticipationRate = 0.10f;
    float chaseRange = 25.0f;
};

struct BodySuf {
    float maxWeight = 2.0f;
    float minWeight = 0.5f;
    float weightGainPerKill = 0.02f;
    float weightDecayRate = 0.001f;
    float streakTimeout = 3.0f;
    float decayDelay = 5.0f;
};

struct InterFlockSuf {
    float interFlockRepulsionWeight = 1.5f;
    float predatorAttractionWeight = 2.0f;
    float preyFearWeight = 3.0f;
};

struct ReproductionSuf {
    float reproductionMinOffspring = 1.0f;
    float reproductionMaxOffspring = 5.0f;
    int   maxFlockSize = 2000;
    float reproductionMinHunger = 0.60f;
    float reproductionInterval = 60.0f;
    float adultAge = 240.0f;
};

struct AgeSuf {
    float juvenileAge = 60.0f;
    float youngAge = 240.0f;
    float elderAge = 960.0f;
    float maxLifespan = 1200.0f;
    float ageSpeedJuvenile = 0.6f;
    float ageSpeedYoung = 0.85f;
    float ageSpeedAdult = 1.0f;
    float ageSpeedElder = 0.7f;
    float ageSizeJuvenile = 0.5f;
    float ageSizeYoung = 0.8f;
    float ageSizeAdult = 1.0f;
    float ageSizeElder = 0.9f;
};

struct FatigueSuf {
    float fatigueAccumRate = 0.02f;
    float fatigueRecoveryRate = 0.15f;
    float fatigueSpeedPenalty = 0.4f;
};

struct GenderSuf {
    float sexSpeedMale = 1.05f;
    float sexSpeedFemale = 0.95f;
    float sexSizeMale = 1.0f;
    float sexSizeFemale = 0.9f;
};

struct PregnancySuf {
    float pregnancyDuration = 10.0f;
    float postpartumRecovery = 30.0f;
    float offspringHungerBoost = 0.3f;
};

struct CombatSuf {
    float combatRadius = 30.0f;       // Distance within which same-flock males may fight
    float combatProbability = 0.30f;  // Base chance per eligible encounter per second
    float combatFatigueGain = 0.15f;  // Fatigue added to loser per fight
    float combatCooldown = 5.0f;      // Seconds before a boid can fight again
};

struct HatredSuf {
    float hatredGainPerKill = 0.25f;       // Hatred gained by surviving flock-mates per kill
    float hatredDecayRate = 0.02f;         // Hatred decay per second
    float hatredFleeRadiusBoost = 3.0f;    // Multiplier on flee range when hating
    float hatredFleeWeightBoost = 2.0f;    // Extra flee weight when hating
};

// Escape strategy: how prey flees from predators
// NOTE: This is a float pretending to be an enum for ParamRegistry compatibility.
// 0.0=DirectFlee, 1.0=ZigzagFlee, 2.0=GroupFlee, 3.0=CoverFlee
enum class EscapeStrategy : uint8_t {
    DirectFlee = 0,  // Flee directly away from predator (fastest)
    ZigzagFlee = 1,   // Erratic zigzag pattern (harder to track)
    GroupFlee  = 2,   // Flee toward nearest flock-mate (safety in numbers)
    CoverFlee  = 3,   // Flee toward nearest plant (use as cover)
};

struct EscapeSuf {
    float escapeStrategy = 0.0f;        // Dominant strategy (0=Direct, 1=Zigzag, 2=Group, 3=Cover)
    float escapeStrategyMix = 0.4f;     // Blend with DirectFlee (0.0=pure, 0.5=50/50, 1.0=equal)
    float escapeZigzagAmp = 0.6f;       // Random perpendicular amplitude for zigzag [0.0-1.0]
};

// Defensive cooperation: flock-mates counter-attack when one is killed
struct DefenseSuf {
    float defenseRadius = 180.0f;           // How far flock-mates notice an attack (pixels)
    float defenseResponseWeight = 1.2f;    // Attraction force toward predator when defending
    float defenseGroupThreshold = 2.0f;    // Min nearby allies needed to trigger mobbing
};

// Cohesion dynamics: cohesion weight is modulated by environmental factors
struct CohesionDynSuf {
    float cohesionThreatBoost = 2.0f;   // Cohesion multiplier when predator nearby
    float cohesionHungerDecay = 0.3f;   // Cohesion reduction per unit hunger below threshold
    float cohesionDensityDecay = 0.5f;  // Cohesion reduction in high local density areas
    float cohesionBaseWeight = 1.0f;    // Base cohesion weight (default 1.0)
};

// Phase 1.7: Attack hit/miss, damage and dodge system
struct HealthSuf {
    float dodgeChanceBase = 0.3f;      // Base dodge probability when attacked [0.0, 1.0]
    float damageToHealth = 0.5f;       // Damage converted to health loss per attack
    float healthRegenRate = 0.01f;     // Health recovered per second (when hunger > 0.5)
    float healthInitial = 1.0f;        // Health for newly spawned boids
};

struct AppearanceSuf {
    bool  useSexColors = false;
    float maleColorR = 0.50f, maleColorG = 0.70f, maleColorB = 0.95f;
    float femaleColorR = 0.95f, femaleColorG = 0.50f, femaleColorB = 0.55f;
    std::string spriteName;
    bool uprightSprite = false;
};

// Composite FlockParams -- all float sub-structs are standard-layout for offsetof().
// AppearanceSuf is non-standard-layout (contains std::string) but is never used
// with offsetof(); its fields are accessed directly.
struct FlockParams {
    MovementSuf    movement;
    PerceptionSuf  perception;
    BoundarySuf    boundary;
    HungerSuf      hunger;
    PredationSuf   predation;
    BodySuf        body;
    InterFlockSuf  interFlock;
    ReproductionSuf reproduction;
    AgeSuf         age;
    FatigueSuf     fatigue;
    GenderSuf      gender;
    PregnancySuf   pregnancy;
    CombatSuf      combat;
    HatredSuf      hatred;
    EscapeSuf      escape;
    DefenseSuf     defense;
    CohesionDynSuf cohesionDyn;
    HealthSuf      health;
    NestPrefSuf    nestPref;
    AppearanceSuf  appearance;
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
    std::vector<uint8_t> state;     // BoidState enum, 1 byte each
    std::vector<float> stateTimer;  // seconds in current state
    std::vector<uint8_t> ageStage;  // AgeStage enum, 1 byte each
    std::vector<float> fatigue;     // 0.0 = fresh, 1.0 = exhausted
    std::vector<float> lastBirthTime;// simTime of last birth (-1e9f = never)
    std::vector<float> lastCombatTime;// simTime of last fight (-1e9f = never)
    std::vector<uint8_t> hatredTarget;   // Flock ID of most-hated flock (255 = none)
    std::vector<float>    hatredLevel;   // Hatred intensity [0.0, 1.0]
    std::vector<float>    health;        // 0.0 = dead, 1.0 = full health (Phase 1.7)
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
        state.reserve(capacity);
        stateTimer.reserve(capacity);
        ageStage.reserve(capacity);
        fatigue.reserve(capacity);
        lastBirthTime.reserve(capacity);
        lastCombatTime.reserve(capacity);
        hatredTarget.reserve(capacity);
        hatredLevel.reserve(capacity);
        health.reserve(capacity);
    }

    void add(float x, float y, float vx, float vy, int fid, float cr, float cg, float cb,
             uint8_t s = 0, float w = 1.0f) {
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
        weight.push_back(w);           // Inherited body mass
        killStreak.push_back(0);      // No kills yet
        lastKillTime.push_back(-1e9f);// Never killed
        state.push_back(static_cast<uint8_t>(BoidState::IDLE));
        stateTimer.push_back(0.0f);
        ageStage.push_back(static_cast<uint8_t>(AgeStage::Adult));  // New spawns start as Adult
        fatigue.push_back(0.0f);  // Start fresh
        lastBirthTime.push_back(-1e9f);  // Never given birth
        lastCombatTime.push_back(-1e9f);  // Never fought
        hatredTarget.push_back(255);       // No hated flock
        hatredLevel.push_back(0.0f);       // No hatred
        health.push_back(1.0f);            // Full health for new spawns
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
        state[index] = state[last];
        stateTimer[index] = stateTimer[last];
        ageStage[index] = ageStage[last];
        fatigue[index] = fatigue[last];
        lastBirthTime[index] = lastBirthTime[last];
        lastCombatTime[index] = lastCombatTime[last];
        hatredTarget[index] = hatredTarget[last];
        hatredLevel[index] = hatredLevel[last];
        health[index] = health[last];
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
        state.pop_back();
        stateTimer.pop_back();
        ageStage.pop_back();
        fatigue.pop_back();
        lastBirthTime.pop_back();
        lastCombatTime.pop_back();
        hatredTarget.pop_back();
        hatredLevel.pop_back();
        health.pop_back();
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
        state.clear();
        stateTimer.clear();
        ageStage.clear();
        fatigue.clear();
        lastBirthTime.clear();
        lastCombatTime.clear();
        hatredTarget.clear();
        hatredLevel.clear();
        health.clear();
        count = 0;
    }
};
