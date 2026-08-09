#pragma once

#include "FlockData.h"
#include "PlantData.h"
#include "SpatialHash.h"
#include "PlantSpatialHash.h"
#include <vector>
#include <random>
#include <string>

// Core simulation: Reynolds flocking (separation, alignment, cohesion)
// with spatial hashing for O(n) neighbor search.
// Supports dynamic flock count (2-12) with per-flock parameters and predator-prey relationships.
// IMPORTANT: Zero per-frame heap allocation -- all working buffers are pre-allocated.

// Relationship types between flocks
enum class FlockRelation : int {
    Neutral  = 0,  // Standard inter-flock repulsion
    Predator = 1,  // This flock chases the target flock
    Prey     = 2   // This flock flees from the target flock
};

// Maximum number of user-configurable flocks
inline constexpr int MAX_FLOCKS = 12;
inline constexpr int MIN_FLOCKS = 2;

// Global simulation settings (not per-flock)
struct GlobalParams {
    bool wrapBoundary = true;         // true = toroidal wrap, false = hard collision walls
    bool hungerFlashEnabled = true;   // true = boids flash red when starving
};

class Simulation {
public:
    Simulation();
    ~Simulation();

    void init(float worldW, float worldH, int maxBoids);
    void update(float dt);

    void addBoid(float x, float y);
    int  spawnRandom(int count);
    void removeBoidAt(int index);

    void setTarget(float x, float y);
    void clearTarget();

    void updateGrid();

    // Multi-flock management
    int activeFlock() const { return m_activeFlock; }
    void setActiveFlock(int id);
    int flockCount() const { return static_cast<int>(m_flockParams.size()); }

    // Dynamic flock add/remove
    int  addFlock();              // Add a new flock, returns its id. Returns -1 if at max.
    bool removeFlock(int id);     // Remove a flock and all its boids. Returns false if at min.

    const float* flockColor(int id) const;
    void setFlockColor(int id, float r, float g, float b);
    int  countInFlock(int flockId) const;
    int  removeBoidsFromFlock(int flockId, int count);

    // Custom flock names
    const std::string& flockName(int id) const { return m_flockNames[id]; }
    void setFlockName(int id, const std::string& name) { if (!m_flockNames.empty()) m_flockNames[id] = name; }
    std::string defaultFlockName(int id) const;

    // Per-flock parameters
    FlockParams& params() { return m_params; }
    const FlockParams& params() const { return m_params; }
    void saveCurrentParams();              // Save m_params to active flock's storage
    void loadFlockParams(int id);           // Load flock params into m_params for editing
    const FlockParams& flockParams(int id) const { return m_flockParams[id]; }
    FlockParams& flockParams(int id) { return m_flockParams[id]; }

    // Collect sprite names for all flocks (for renderer)
    std::vector<std::string> flockSpriteNames() const;

    // Collect upright-sprite flags for all flocks (for renderer)
    std::vector<bool> flockUprightFlags() const;

    // Predator-prey relationship matrix
    // m_relationships[viewer][target] = how viewer flock relates to target flock
    FlockRelation relationship(int viewer, int target) const;
    void setRelationship(int viewer, int target, FlockRelation rel);

    // Accessors
    FlockData& data() { return m_data; }
    const FlockData& data() const { return m_data; }
    PlantData& plants() { return m_plants; }
    const PlantData& plants() const { return m_plants; }
    PlantParams& plantParams() { return m_plantParams; }
    const PlantParams& plantParams() const { return m_plantParams; }
    GlobalParams& globalParams() { return m_globalParams; }
    const GlobalParams& globalParams() const { return m_globalParams; }

    // Batch-set maxFlockSize on all flocks (global slider convenience)
    // Soft limit: does not cull existing boids.
    void setGlobalFlockCap(int cap);
    int maxBoids() const { return m_maxBoids; }
    float worldW() const { return m_worldW; }
    float worldH() const { return m_worldH; }
    bool hasTarget() const { return m_hasTarget; }
    float targetX() const { return m_targetX; }
    float targetY() const { return m_targetY; }
    float simTime() const { return m_simTime; }
    int currentSeason() const { return static_cast<int>(m_simTime / m_plantParams.seasonLength) % 4; }

private:
    FlockData m_data;
    PlantData m_plants;
    PlantParams m_plantParams;
    GlobalParams m_globalParams;
    SpatialHash m_grid;
    PlantSpatialHash m_plantGrid;

    float m_worldW;
    float m_worldH;
    int m_maxBoids;

    // Total elapsed simulation time (for season tracking)
    float m_simTime = 0.0f;

    // Active flock for spawning + UI editing
    int m_activeFlock;

    // Per-flock parameter storage (indexed by flock id)
    std::vector<FlockParams> m_flockParams;
    // Active editing params (synced with UI sliders)
    FlockParams m_params;

    // Per-flock display names (user-customizable, default: "Flock A"..)
    std::vector<std::string> m_flockNames;

    // Per-flock custom colors (default from DEFAULT_FLOCK_COLORS palette)
    std::vector<float> m_flockColorR;
    std::vector<float> m_flockColorG;
    std::vector<float> m_flockColorB;

    // Relationship matrix: m_relationships[viewer][target]
    std::vector<std::vector<FlockRelation>> m_relationships;

    // Target seeking
    bool m_hasTarget;
    float m_targetX;
    float m_targetY;

    // Pre-allocated working buffers
    std::vector<float> m_forceX;
    std::vector<float> m_forceY;
    std::vector<int> m_neighbors;
    std::vector<int> m_plantNeighbors;  // Plant spatial hash results
    std::vector<int> m_deadIndices;    // Starved/killed boid indices to remove post-update
    std::vector<float> m_deathPosX;    // Death positions for plant fertilization
    std::vector<float> m_deathPosY;

    // Per-flock reproduction timing
    std::vector<float> m_lastReproductionTime;

    // RNG
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_angleDist{-1.0f, 1.0f};

    void wrapPosition(float& x, float& y) const;
    void updatePlants(float dt);
    void updateReproduction(float dt);

    // Helper: get per-boid rendering color based on flock + sex
    void resolveBoidColor(int fid, int sex, float& r, float& g, float& b) const;
};
