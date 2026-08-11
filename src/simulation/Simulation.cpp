#include "Simulation.h"
#include <algorithm>
#include <cmath>

Simulation::Simulation()
    : m_worldW(1920.0f)
    , m_worldH(1080.0f)
    , m_maxBoids(0)
    , m_activeFlock(0)
    , m_hasTarget(false)
    , m_targetX(0.0f)
    , m_targetY(0.0f)
    , m_rng(std::random_device{}())
{
}

Simulation::~Simulation() = default;

void Simulation::init(float worldW, float worldH, int maxBoids)
{
    m_worldW = worldW;
    m_worldH = worldH;
    m_maxBoids = maxBoids;
    m_activeFlock = 0;

    m_data.reserve(maxBoids);

    m_forceX.resize(maxBoids);
    m_forceY.resize(maxBoids);
    m_neighbors.reserve(maxBoids);
    m_deadIndices.reserve(maxBoids);

    // Init default flocks (minimum 2)
    constexpr int initialFlocks = MIN_FLOCKS;
    m_flockParams.clear();
    m_flockNames.clear();
    m_flockColorR.clear();
    m_flockColorG.clear();
    m_flockColorB.clear();
    m_relationships.clear();
    m_lastReproductionTime.clear();

    for (int i = 0; i < initialFlocks; ++i) {
        m_flockParams.push_back(FlockParams{});
        m_flockNames.push_back(defaultFlockName(i));
        m_flockColorR.push_back(DEFAULT_FLOCK_COLORS[i][0]);
        m_flockColorG.push_back(DEFAULT_FLOCK_COLORS[i][1]);
        m_flockColorB.push_back(DEFAULT_FLOCK_COLORS[i][2]);
        m_lastReproductionTime.push_back(0.0f);

        std::vector<FlockRelation> row(initialFlocks, FlockRelation::Neutral);
        m_relationships.push_back(row);
    }
    // Ensure all rows have the correct column count
    for (int i = 0; i < initialFlocks; ++i)
        m_relationships[i].resize(initialFlocks, FlockRelation::Neutral);

    // Copy first flock params to active editor
    m_params = m_flockParams[0];

    // Grid cell size uses max cohesion radius across all flocks
    float maxCohRad = m_params.perception.cohesionRadius;
    for (auto& fp : m_flockParams)
        maxCohRad = std::max(maxCohRad, fp.perception.cohesionRadius);
    m_grid.init(worldW, worldH, maxCohRad * 1.5f, maxBoids);
    m_plantGrid.init(worldW, worldH, m_plantParams.eatRange, static_cast<int>(m_plantParams.maxPlants));

    m_hasTarget = false;

    // Initialize plants
    m_simTime = 0.0f;
    m_plants.reserve(static_cast<size_t>(m_plantParams.maxPlants));
    m_plants.clear();
    std::uniform_real_distribution<float> pxDist(20.0f, worldW - 20.0f);
    std::uniform_real_distribution<float> pyDist(20.0f, worldH - 20.0f);
    std::uniform_real_distribution<float> gDist(0.2f, 1.0f);
    for (int i = 0; i < static_cast<int>(m_plantParams.initialPlants); ++i) {
        m_plants.add(pxDist(m_rng), pyDist(m_rng), gDist(m_rng));
    }

    // Initialize nests (Phase 3.1)
    m_nests.reserve(static_cast<size_t>(m_nestParams.maxNests));
    m_nests.clear();
    int nFlocks = static_cast<int>(m_flockParams.size());
    for (int i = 0; i < static_cast<int>(m_nestParams.initialNests); ++i) {
        int owner = (i < nFlocks) ? i : -1;  // Assign to flocks round-robin, extra nests unowned
        m_nests.add(pxDist(m_rng), pyDist(m_rng), owner);
    }
}

void Simulation::resizeWorld(float worldW, float worldH)
{
    m_worldW = worldW;
    m_worldH = worldH;

    // Reinitialize spatial grids for new dimensions
    float maxCohRad = m_params.perception.cohesionRadius;
    for (auto& fp : m_flockParams)
        maxCohRad = std::max(maxCohRad, fp.perception.cohesionRadius);
    m_grid.reinit(worldW, worldH, maxCohRad * 1.5f, m_maxBoids);
    m_plantGrid.init(worldW, worldH, m_plantParams.eatRange,
                     static_cast<int>(m_plantParams.maxPlants));

    // Clamp existing boid positions to new bounds
    for (int i = 0; i < m_data.count; ++i) {
        m_data.posX[i] = std::clamp(m_data.posX[i], 0.0f, worldW);
        m_data.posY[i] = std::clamp(m_data.posY[i], 0.0f, worldH);
    }

    // Regenerate plants within new bounds
    m_plants.reserve(static_cast<size_t>(m_plantParams.maxPlants));
    m_plants.clear();
    std::uniform_real_distribution<float> pxDist(20.0f, worldW - 20.0f);
    std::uniform_real_distribution<float> pyDist(20.0f, worldH - 20.0f);
    std::uniform_real_distribution<float> gDist(0.2f, 1.0f);
    for (int i = 0; i < static_cast<int>(m_plantParams.initialPlants); ++i) {
        m_plants.add(pxDist(m_rng), pyDist(m_rng), gDist(m_rng));
    }

    // Regenerate nests within new bounds
    m_nests.reserve(static_cast<size_t>(m_nestParams.maxNests));
    m_nests.clear();
    int nFlocks = static_cast<int>(m_flockParams.size());
    for (int i = 0; i < static_cast<int>(m_nestParams.initialNests); ++i) {
        int owner = (i < nFlocks) ? i : -1;
        m_nests.add(pxDist(m_rng), pyDist(m_rng), owner);
    }

    // Reset disturbance sources (out-of-bounds after resize)
    m_disturbances.clear();
}

void Simulation::update(float dt)
{
    m_simTime += dt;

    // Auto-clear target after 3 seconds of inactivity
    if (m_hasTarget && (m_simTime - m_targetTime > 3.0f)) {
        m_hasTarget = false;
    }

    // Phase 3.2: Decay boid memories every frame
    decayBoidMemories(dt);

    // Phase 3.4: Update disturbance source lifetimes
    updateDisturbances(dt);

    // Phase 3.5: Update sanity levels
    updateSanity(dt);

    if (m_data.count == 0) { updatePlants(dt); updateNests(dt); return; }

    stepHunger(dt);
    updateNests(dt);
    stepFlocks(dt);
    stepIntegration(dt);
}

// ---- Step 0: Hunger decay for all boids ----
void Simulation::stepHunger(float dt)
{
    // ---- Phase 2.6: Environmental carrying capacity ----
    // Calculate total plant food value and average hunger decay.
    // When population exceeds sustainable capacity, extra hunger pressure is applied.
    float carryingExtraDecay = 0.0f;
    if (m_plantParams.carryingPressure > 0.001f && m_data.count > 0) {
        float totalFood = 0.0f;
        for (int p = 0; p < m_plants.count; ++p) {
            totalFood += m_plants.growth[p] * m_plantParams.plantFoodValue;
        }

        float avgDecay = 0.0f;
        for (int i = 0; i < m_data.count; ++i) {
            int fid = m_data.flockId[i];
            if (fid >= 0 && fid < static_cast<int>(m_flockParams.size())) {
                avgDecay += m_flockParams[fid].hunger.hungerDecayRate;
            }
        }
        avgDecay /= static_cast<float>(m_data.count);

        // Carrying capacity: total food per second / decay per boid per second
        float capacity = (avgDecay > 0.0001f) ? (totalFood / avgDecay) : 1e9f;

        if (static_cast<float>(m_data.count) > capacity * 1.1f) {
            float ratio = static_cast<float>(m_data.count) / capacity;
            carryingExtraDecay = m_plantParams.carryingPressure * (ratio - 1.0f) * avgDecay;
        }
    }

    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float decayRate = fp.hunger.hungerDecayRate + carryingExtraDecay;
        m_data.hunger[i] -= decayRate * dt;
        if (m_data.hunger[i] > 1.0f) m_data.hunger[i] = 1.0f;

        // Phase 1.7: Health regeneration when well-fed
        if (m_data.hunger[i] > 0.5f && m_data.health[i] < 1.0f) {
            m_data.health[i] += fp.health.healthRegenRate * dt;
            if (m_data.health[i] > 1.0f) m_data.health[i] = 1.0f;
        }
    }
}

// ---- State machine: determine current state for each boid ----
void Simulation::determineStates()
{
    int nFlock = static_cast<int>(m_flockParams.size());

    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float hungerI = m_data.hunger[i];

        // Priority: FLEEING > HUNTING > FORAGING > IDLE
        // FLEEING is set reactively in the neighbor search (when predator detected)
        // Here we set the default state based on internal conditions
        BoidState newState = BoidState::IDLE;

        // Check if this flock is a predator of any other flock
        bool isPredator = false;
        for (int j = 0; j < nFlock; ++j) {
            if (m_relationships[fid][j] == FlockRelation::Predator) {
                isPredator = true;
                break;
            }
        }

        if (hungerI < fp.hunger.forageHungerThreshold) {
            newState = BoidState::FORAGING;
        }
        if (isPredator && hungerI < fp.predation.predationMinHunger) {
            newState = BoidState::HUNTING;  // Overrides FORAGING
        }

        // Keep previous FLEEING state until state timer expires
        uint8_t prevState = m_data.state[i];
        if (prevState == static_cast<uint8_t>(BoidState::FLEEING)) {
            // Stay in FLEEING for at least 1 second after last predator sighting
            if (m_data.stateTimer[i] < 1.0f) {
                newState = BoidState::FLEEING;
            }
        }

        m_data.state[i] = static_cast<uint8_t>(newState);
        m_data.stateTimer[i] += 0.0f;  // Timer updated elsewhere
    }
}

// ---- Age: update stage for each boid ----
void Simulation::updateAgeStages()
{
    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float age = m_data.age[i];

        AgeStage newStage = AgeStage::Adult;
        if (age < fp.age.juvenileAge)
            newStage = AgeStage::Juvenile;
        else if (age < fp.age.youngAge)
            newStage = AgeStage::Young;
        else if (age >= fp.age.elderAge)
            newStage = AgeStage::Elder;

        m_data.ageStage[i] = static_cast<uint8_t>(newStage);
    }
}

// ---- Hatred: accumulate on surviving flock-mates after predation kills ----
// Called after kills are processed but before dead boids are removed.
// Surviving boids in the prey's flock gain hatred toward the predator's flock.
void Simulation::updateHatred(float dt)
{
    if (m_killsThisFrame.empty()) {
        // Only decay when no fresh kills
        for (int i = 0; i < m_data.count; ++i) {
            if (m_data.hatredLevel[i] > 0.0f) {
                int fid = m_data.flockId[i];
                const FlockParams& fp = m_flockParams[fid];
                m_data.hatredLevel[i] -= fp.hatred.hatredDecayRate * dt;
                if (m_data.hatredLevel[i] <= 0.0f) {
                    m_data.hatredLevel[i] = 0.0f;
                    m_data.hatredTarget[i] = 255;
                }
            }
        }
        return;
    }

    // Apply hatred gain for each kill: all surviving prey flock-mates hate predator flock
    for (auto& kp : m_killsThisFrame) {
        if (kp.predatorIdx >= m_data.count || kp.preyIdx >= m_data.count) continue;
        int predFlockId = m_data.flockId[kp.predatorIdx];
        int preyFlockId = m_data.flockId[kp.preyIdx];
        const FlockParams& preyFp = m_flockParams[preyFlockId];

        for (int i = 0; i < m_data.count; ++i) {
            // Only prey's own flock-mates (alive) gain hatred
            if (m_data.flockId[i] != preyFlockId) continue;
            if (m_data.hunger[i] <= 0.0f) continue;  // Skip dead/dying

            // Accumulate hatred toward predator flock, capped at 1.0
            m_data.hatredTarget[i] = static_cast<uint8_t>(predFlockId);
            m_data.hatredLevel[i] = std::min(1.0f,
                m_data.hatredLevel[i] + preyFp.hatred.hatredGainPerKill);
        }
    }

    // Decay hatred for all boids
    for (int i = 0; i < m_data.count; ++i) {
        if (m_data.hatredLevel[i] <= 0.0f) continue;
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        m_data.hatredLevel[i] -= fp.hatred.hatredDecayRate * dt;
        if (m_data.hatredLevel[i] <= 0.0f) {
            m_data.hatredLevel[i] = 0.0f;
            m_data.hatredTarget[i] = 255;
        }
    }
}

// ---- Step 1: Rebuild grids + compute all forces (Reynolds, predation, foraging, boundary, wander, target) ----
void Simulation::stepFlocks(float dt)
{
    // Rebuild spatial hash grids
    m_grid.rebuild(m_data);
    m_plantGrid.rebuild(m_plants);

    float maxForceSq = m_params.movement.maxForce * m_params.movement.maxForce;

    // Compute max cohesion radius for inter-flock detection range
    float maxCohRadius = m_params.perception.cohesionRadius;
    for (auto& fp : m_flockParams)
        maxCohRadius = std::max(maxCohRadius, fp.perception.cohesionRadius);
    float interFlockRadiusSq = maxCohRadius * maxCohRadius;

    float maxChaseRange = 0.0f;
    for (auto& fp : m_flockParams)
        maxChaseRange = std::max(maxChaseRange, fp.predation.chaseRange);

    std::fill(m_forceX.begin(), m_forceX.begin() + m_data.count, 0.0f);
    std::fill(m_forceY.begin(), m_forceY.begin() + m_data.count, 0.0f);

    m_killsThisFrame.clear();
    m_killsThisFrame.reserve(256);

    // Phase 0: Determine current state for each boid
    determineStates();

    for (int i = 0; i < m_data.count; ++i) {
        float px = m_data.posX[i];
        float py = m_data.posY[i];
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float hungerI = m_data.hunger[i];
        bool killedThisFrame = false;

        float sepRadiusSq = fp.perception.separationRadius * fp.perception.separationRadius;
        float aliRadiusSq = fp.perception.alignmentRadius * fp.perception.alignmentRadius;
        float cohRadiusSq = fp.perception.cohesionRadius * fp.perception.cohesionRadius;

        float hardRadius = fp.movement.hardCollisionRadius;
        float hardRadiusSq = hardRadius * hardRadius;
        float chaseRangeSq = fp.predation.chaseRange * fp.predation.chaseRange;

        m_neighbors.clear();
        float queryRadius = std::max(maxCohRadius, maxChaseRange);
        m_grid.queryNeighbors(px, py, queryRadius, m_data, m_neighbors);

        float sepX = 0.0f, sepY = 0.0f;
        float aliX = 0.0f, aliY = 0.0f;
        float cohX = 0.0f, cohY = 0.0f;
        int aliCount = 0, cohCount = 0;
        float ifSepX = 0.0f, ifSepY = 0.0f;

        for (int j : m_neighbors) {
            if (j == i) continue;

            float dx = m_data.posX[j] - px;
            float dy = m_data.posY[j] - py;
            float distSq = dx * dx + dy * dy;

            int jFid = m_data.flockId[j];

            if (jFid == fid) {
                // --- Same-flock ---
                if (distSq < hardRadiusSq) {
                    float dist = std::sqrt(distSq) + 0.0001f;
                    float overlap = hardRadius - dist;
                    float force = (overlap / hardRadius) * fp.movement.maxSpeed * fp.perception.separationWeight * 3.0f;
                    sepX -= (dx / dist) * force;
                    sepY -= (dy / dist) * force;
                }
                else if (distSq < sepRadiusSq) {
                    float factor = 1.0f / distSq;
                    sepX -= dx * factor;
                    sepY -= dy * factor;
                }
                if (distSq < aliRadiusSq) {
                    aliX += m_data.velX[j];
                    aliY += m_data.velY[j];
                    ++aliCount;
                }
                if (distSq < cohRadiusSq) {
                    cohX += m_data.posX[j];
                    cohY += m_data.posY[j];
                    ++cohCount;
                }

                // Male combat: same-flock males fight for dominance (Phase 2.1)
                {
                    float combatRadSq = fp.combat.combatRadius * fp.combat.combatRadius;
                    if (distSq < combatRadSq
                        && m_data.sex[i] == 0 && m_data.sex[j] == 0
                        && (m_data.state[i] == static_cast<uint8_t>(BoidState::IDLE)
                            || m_data.state[i] == static_cast<uint8_t>(BoidState::HUNTING))
                        && (m_data.state[j] == static_cast<uint8_t>(BoidState::IDLE)
                            || m_data.state[j] == static_cast<uint8_t>(BoidState::HUNTING))
                        && (m_simTime - m_data.lastCombatTime[i] > fp.combat.combatCooldown)
                        && (m_simTime - m_data.lastCombatTime[j] > fp.combat.combatCooldown))
                    {
                        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
                        // Scale probability by dt for per-second semantics
                        if (roll(m_rng) < fp.combat.combatProbability * dt) {
                            // Combat triggers: mutual repulsion + fatigue for loser
                            float dist = std::sqrt(distSq) + 0.0001f;
                            float force = fp.movement.maxSpeed * 1.5f
                                          * (1.0f - distSq / combatRadSq);
                            // Push i away from j
                            sepX -= (dx / dist) * force;
                            sepY -= (dy / dist) * force;
                            // Push j away from i (opposite direction)
                            m_forceX[j] += (dx / dist) * force;
                            m_forceY[j] += (dy / dist) * force;

                            // Heavier boid wins; loser gets fatigue penalty
                            if (m_data.weight[i] >= m_data.weight[j]) {
                                m_data.fatigue[j] = std::min(1.0f,
                                    m_data.fatigue[j] + fp.combat.combatFatigueGain);
                            } else {
                                m_data.fatigue[i] = std::min(1.0f,
                                    m_data.fatigue[i] + fp.combat.combatFatigueGain);
                            }
                            m_data.lastCombatTime[i] = m_simTime;
                            m_data.lastCombatTime[j] = m_simTime;
                        }
                    }
                }
            }
            else {
                // --- Different-flock ---
                FlockRelation rel = m_relationships[fid][jFid];
                const FlockParams& jp = m_flockParams[jFid];
                float hungerJ = m_data.hunger[j];

                if (rel == FlockRelation::Neutral) {
                    if (distSq < interFlockRadiusSq) {
                        float dist = std::sqrt(distSq) + 0.0001f;
                        float force = fp.interFlock.interFlockRepulsionWeight * fp.movement.maxSpeed
                                      / (distSq / interFlockRadiusSq + 0.1f);
                        ifSepX -= (dx / dist) * force;
                        ifSepY -= (dy / dist) * force;
                        // Phase 3.2: Record hostile encounter from foreign flock
                        m_data.recordMemory(i, MemoryEvent::HOSTILE_ENCOUNTER,
                                            m_data.posX[i], m_data.posY[i],
                                            0.8f, m_simTime);
                    }
                }
                else if (rel == FlockRelation::Predator) {
                    // Determine chase participation based on hunger level
                    bool participatesInChase = false;
                    if (hungerI < fp.predation.predationKillHunger) {
                        // Below kill threshold: 100% participate in chase
                        participatesInChase = true;
                    } else if (hungerI < fp.predation.predationMinHunger) {
                        // Between kill and min thresholds: linear interpolation
                        float range = fp.predation.predationMinHunger - fp.predation.predationKillHunger;
                        float t = (fp.predation.predationMinHunger - hungerI) / (range + 0.0001f);
                        float rate = fp.predation.predationParticipationRate
                                   + (1.0f - fp.predation.predationParticipationRate) * t;
                        participatesInChase = ((i % 100) < static_cast<int>(rate * 100.0f));
                    } else {
                        // Above min hunger: base participation rate only
                        participatesInChase = ((i % 100)
                            < static_cast<int>(fp.predation.predationParticipationRate * 100.0f));
                    }

                    if (participatesInChase) {
                    // Phase 3.2: Record prey sighting at prey's position
                    m_data.recordMemory(i, MemoryEvent::PREY_SIGHTING,
                                        m_data.posX[j], m_data.posY[j],
                                        0.7f, m_simTime);
                    float preyRadiusSq = interFlockRadiusSq * 2.0f;
                    if (distSq < preyRadiusSq) {
                        float dist = std::sqrt(distSq) + 0.0001f;
                        float force = fp.interFlock.predatorAttractionWeight * fp.movement.maxSpeed
                                      * std::min(1.0f, distSq / interFlockRadiusSq)
                                      / (distSq / preyRadiusSq + 0.05f);
                        ifSepX += (dx / dist) * force;
                        ifSepY += (dy / dist) * force;
                        if (distSq < hardRadiusSq * 4.0f) {
                            float closeForce = fp.movement.maxSpeed * 2.0f
                                               / (distSq / hardRadiusSq + 0.01f);
                            ifSepX -= (dx / dist) * closeForce;
                            ifSepY -= (dy / dist) * closeForce;
                        }
                    }

                    // Predation check -- only when below kill threshold
                    if (!killedThisFrame && distSq < chaseRangeSq
                        && hungerI < fp.predation.predationKillHunger && hungerJ > 0.0f) {
                        bool alreadyDead = false;
                        for (auto& kp : m_killsThisFrame) {
                            if (kp.preyIdx == j) { alreadyDead = true; break; }
                        }
                        if (!alreadyDead) {
                            float distRatio = 1.0f - std::sqrt(distSq) / fp.predation.chaseRange;
                            float distFactor = 0.5f + 0.5f * distRatio;

                            // Multi-factor chase probability (Phase 1.6)
                            // Base * hungerMod * distance * age * fatigue * sizeRatio * sex
                            float ageMod = 1.0f;
                            switch (static_cast<AgeStage>(m_data.ageStage[i])) {
                            case AgeStage::Juvenile: ageMod = 0.4f; break;
                            case AgeStage::Young:    ageMod = 0.8f; break;
                            case AgeStage::Elder:    ageMod = 0.6f; break;
                            default:                 ageMod = 1.0f; break;
                            }
                            float fatigueMod = 1.0f - m_data.fatigue[i] * 0.5f;
                            float sizeMod = std::sqrt(m_data.weight[j] / (m_data.weight[i] + 0.001f));
                            sizeMod = std::max(0.3f, std::min(1.5f, sizeMod));
                            float sexMod = (m_data.sex[i] == 0) ? 1.0f : 0.85f;
                            float hungerMod = 2.0f - std::min(hungerI, 1.0f);

                            float chaseProb = fp.predation.chaseSuccessBase * hungerMod * distFactor
                                            * ageMod * fatigueMod * sizeMod * sexMod;
                            std::uniform_real_distribution<float> roll(0.0f, 1.0f);
                            if (roll(m_rng) < chaseProb) {
                                // Phase 1.7: Hit + Dodge + Damage instead of instant kill
                                // Prey dodge chance (multi-factor: age, fatigue, size, escape strategy)
                                float preyDodgeAgeMod = 1.0f;
                                switch (static_cast<AgeStage>(m_data.ageStage[j])) {
                                case AgeStage::Juvenile: preyDodgeAgeMod = 0.5f; break;
                                case AgeStage::Young:    preyDodgeAgeMod = 0.8f; break;
                                case AgeStage::Elder:    preyDodgeAgeMod = 0.6f; break;
                                default:                 preyDodgeAgeMod = 1.0f; break;
                                }
                                float preyDodgeFatigueMod = 1.0f - m_data.fatigue[j] * 0.5f;
                                float preyDodgeSizeMod = std::sqrt(1.0f / (m_data.weight[j] + 0.001f));
                                preyDodgeSizeMod = std::max(0.5f, std::min(2.0f, preyDodgeSizeMod));
                                // Escape strategy boost: zigzag & group flee harder to hit
                                float escapeStrategyBoost = 1.0f;
                                int strat = static_cast<int>(jp.escape.escapeStrategy);
                                if (strat == 1) escapeStrategyBoost = 1.3f;  // Zigzag
                                else if (strat == 2) escapeStrategyBoost = 1.2f;  // Group
                                else if (strat == 3) escapeStrategyBoost = 1.1f;  // Cover

                                float dodgeProb = jp.health.dodgeChanceBase * preyDodgeAgeMod
                                                * preyDodgeFatigueMod * preyDodgeSizeMod
                                                * escapeStrategyBoost;
                                dodgeProb = std::min(0.95f, dodgeProb);  // Hard cap at 95%

                                if (roll(m_rng) < dodgeProb) {
                                    // Prey dodges — escape boost
                                    float boostDirX = -m_data.velX[j];
                                    float boostDirY = -m_data.velY[j];
                                    float bmag = std::sqrt(boostDirX * boostDirX + boostDirY * boostDirY);
                                    if (bmag > 0.001f) {
                                        float boost = jp.movement.maxSpeed * 1.5f * 0.5f;
                                        m_data.velX[j] += (boostDirX / bmag) * boost * dt;
                                        m_data.velY[j] += (boostDirY / bmag) * boost * dt;
                                    }
                                } else {
                                    // Hit — apply damage instead of instant kill
                                    float sizeRatio = m_data.weight[i] / (m_data.weight[j] + 0.001f);
                                    float damage = jp.health.damageToHealth * std::min(2.0f, sizeRatio);
                                    m_data.health[j] -= damage;
                                    if (m_data.health[j] <= 0.0f) {
                                        // Lethal hit — kill as before
                                        m_killsThisFrame.push_back({i, j});
                                        killedThisFrame = true;
                                        hungerI = 1.0f;
                                    }
                                }
                            }
                            else {
                                // Multi-factor escape probability (Phase 1.6)
                                float preyAgeMod = 1.0f;
                                switch (static_cast<AgeStage>(m_data.ageStage[j])) {
                                case AgeStage::Juvenile: preyAgeMod = 0.5f; break;
                                case AgeStage::Young:    preyAgeMod = 0.8f; break;
                                case AgeStage::Elder:    preyAgeMod = 0.6f; break;
                                default:                 preyAgeMod = 1.0f; break;
                                }
                                float preyFatigueMod = 1.0f - m_data.fatigue[j] * 0.4f;
                                float escapeProb = jp.predation.escapeSuccessBase
                                                 * std::max(hungerJ, 0.1f)
                                                 * (0.6f + 0.4f * distRatio)
                                                 * preyAgeMod * preyFatigueMod;
                                if (roll(m_rng) < escapeProb) {
                                    float boostDirX = -m_data.velX[j];
                                    float boostDirY = -m_data.velY[j];
                                    float bmag = std::sqrt(boostDirX * boostDirX + boostDirY * boostDirY);
                                    if (bmag > 0.001f) {
                                        float boost = fp.movement.maxSpeed * 1.8f * 0.5f;
                                        m_data.velX[j] += (boostDirX / bmag) * boost * dt;
                                        m_data.velY[j] += (boostDirY / bmag) * boost * dt;
                                    }
                                }
                            }
                        }
                    }
                    } // end if participatesInChase
                }
                else {
                    // Prey: flee with strategy selection (Phase 2.3)
                    // Base flee direction = directly away from predator.
                    // Strategy modifies this direction, then blends with pure direct flee.
                    bool hatesThisFlock = (m_data.hatredTarget[i] == static_cast<uint8_t>(jFid))
                                          && m_data.hatredLevel[i] > 0.01f;
                    float hateMult = hatesThisFlock ? (1.0f + m_data.hatredLevel[i] * fp.hatred.hatredFleeRadiusBoost) : 1.0f;
                    float hateWeight = hatesThisFlock ? (1.0f + m_data.hatredLevel[i] * fp.hatred.hatredFleeWeightBoost) : 1.0f;
                    float fearRadiusSq = interFlockRadiusSq * 2.5f * hateMult;
                    if (distSq < fearRadiusSq) {
                        // Phase 3.2: Record predator sighting at own position
                        m_data.recordMemory(i, MemoryEvent::PREDATOR_SIGHTING,
                                            px, py,
                                            1.0f, m_simTime);
                        float dist = std::sqrt(distSq) + 0.0001f;

                        // Direction directly away from this predator
                        float fleeDirX = -(dx / dist);
                        float fleeDirY = -(dy / dist);

                        // Strategy-modified direction (defaults to direct flee)
                        int strat = static_cast<int>(fp.escape.escapeStrategy);
                        float mix = fp.escape.escapeStrategyMix;
                        float stratDirX = fleeDirX;
                        float stratDirY = fleeDirY;

                        if (strat == 1) {
                            // Zigzag: add random perpendicular component
                            float perpX = -fleeDirY;
                            float perpY = fleeDirX;
                            float amp = fp.escape.escapeZigzagAmp
                                        * (m_angleDist(m_rng) * 2.0f - 1.0f);
                            stratDirX = fleeDirX + perpX * amp;
                            stratDirY = fleeDirY + perpY * amp;
                            float mag = std::sqrt(stratDirX * stratDirX
                                               + stratDirY * stratDirY);
                            if (mag > 0.001f) { stratDirX /= mag; stratDirY /= mag; }
                        }
                        else if (strat == 2) {
                            // GroupFlee: bias toward nearest same-flock mate
                            float bestGDist = fearRadiusSq;
                            float gx = 0.0f, gy = 0.0f;
                            for (int k : m_neighbors) {
                                if (k == i) continue;
                                if (m_data.flockId[k] != fid) continue;
                                float gdx = m_data.posX[k] - px;
                                float gdy = m_data.posY[k] - py;
                                float gdSq = gdx * gdx + gdy * gdy;
                                if (gdSq < bestGDist) {
                                    bestGDist = gdSq;
                                    gx = gdx; gy = gdy;
                                }
                            }
                            if (bestGDist < fearRadiusSq) {
                                float gdist = std::sqrt(bestGDist) + 0.0001f;
                                stratDirX = gx / gdist;
                                stratDirY = gy / gdist;
                            }
                        }
                        else if (strat == 3) {
                            // CoverFlee: bias toward nearest plant for cover
                            m_plantNeighbors.clear();
                            float coverRange = fp.hunger.forageRange * 2.5f;
                            m_plantGrid.queryNeighbors(px, py, coverRange,
                                                       m_plants, m_plantNeighbors);
                            float bestCDist = coverRange * coverRange;
                            float cx = 0.0f, cy = 0.0f;
                            for (int p : m_plantNeighbors) {
                                if (m_plants.growth[p] < 0.05f) continue;
                                float cdx = m_plants.posX[p] - px;
                                float cdy = m_plants.posY[p] - py;
                                float cdSq = cdx * cdx + cdy * cdy;
                                if (cdSq < bestCDist) {
                                    bestCDist = cdSq;
                                    cx = cdx; cy = cdy;
                                }
                            }
                            if (bestCDist < coverRange * coverRange) {
                                float cdist = std::sqrt(bestCDist) + 0.0001f;
                                stratDirX = cx / cdist;
                                stratDirY = cy / cdist;
                            }
                        }

                        // Blend direct flee with strategy-modified direction
                        float blendX = (1.0f - mix) * fleeDirX + mix * stratDirX;
                        float blendY = (1.0f - mix) * fleeDirY + mix * stratDirY;
                        float blendMag = std::sqrt(blendX * blendX + blendY * blendY);
                        if (blendMag > 0.001f) {
                            blendX /= blendMag;
                            blendY /= blendMag;
                        }

                        float force = fp.interFlock.preyFearWeight
                                      * fp.movement.maxSpeed * hateWeight
                                      / (distSq / interFlockRadiusSq + 0.03f);
                        ifSepX += blendX * force;
                        ifSepY += blendY * force;
                    }
                }
            }
        }

        // Normalize alignment
        if (aliCount > 0) {
            aliX = aliX / static_cast<float>(aliCount) - m_data.velX[i];
            aliY = aliY / static_cast<float>(aliCount) - m_data.velY[i];
        }
        float aliMag = std::sqrt(aliX * aliX + aliY * aliY);
        if (aliMag > 0.001f) {
            aliX = aliX / aliMag * fp.movement.maxSpeed;
            aliY = aliY / aliMag * fp.movement.maxSpeed;
        }

        // Normalize cohesion
        if (cohCount > 0) {
            cohX = cohX / static_cast<float>(cohCount) - px;
            cohY = cohY / static_cast<float>(cohCount) - py;
        }
        float cohMag = std::sqrt(cohX * cohX + cohY * cohY);
        if (cohMag > 0.001f) {
            cohX = cohX / cohMag * fp.movement.maxSpeed;
            cohY = cohY / cohMag * fp.movement.maxSpeed;
        }

        float sepMag = std::sqrt(sepX * sepX + sepY * sepY);
        float sepMax = fp.movement.maxSpeed * fp.perception.separationWeight * 2.0f;
        if (sepMag > sepMax) {
            sepX = sepX / sepMag * sepMax;
            sepY = sepY / sepMag * sepMax;
        }

        // ---- Phase 2.5: Dynamic cohesion weight ----
        // Modulated by threat, hunger, and local density.
        float cohesionMod = fp.cohesionDyn.cohesionBaseWeight;

        // Threat boost: under predator pressure, flock tighter
        if (m_data.state[i] == static_cast<uint8_t>(BoidState::FLEEING)) {
            cohesionMod *= fp.cohesionDyn.cohesionThreatBoost;
        }

        // Hunger decay: hungry boids spread out to forage independently
        if (hungerI < fp.hunger.forageHungerThreshold) {
            float hungerDeficit = (fp.hunger.forageHungerThreshold - hungerI)
                                  / fp.hunger.forageHungerThreshold;
            cohesionMod *= (1.0f - hungerDeficit * fp.cohesionDyn.cohesionHungerDecay);
        }

        // Density decay: in dense clusters, boids can afford to spread
        if (cohCount > 10) {
            float densityRatio = static_cast<float>(cohCount - 10) / 20.0f;
            cohesionMod *= (1.0f - std::min(0.8f, densityRatio * fp.cohesionDyn.cohesionDensityDecay));
        }
        cohesionMod = std::max(0.05f, cohesionMod);

        // Phase 3.5: Sanity reduces cohesion (panicked boids scatter)
        float sl = m_data.sanityLevel[i];
        if (sl < m_flockParams[fid].sanity.panickedThreshold)
            cohesionMod *= m_flockParams[fid].sanity.panickedCohesionMult;
        else if (sl < m_flockParams[fid].sanity.uneasyThreshold)
            cohesionMod *= m_flockParams[fid].sanity.uneasyCohesionMult;

        m_forceX[i] = sepX + aliX * fp.perception.alignmentWeight
                    + cohX * fp.perception.cohesionWeight * cohesionMod + ifSepX;
        m_forceY[i] = sepY + aliY * fp.perception.alignmentWeight
                    + cohY * fp.perception.cohesionWeight * cohesionMod + ifSepY;

        // Foraging (using plant spatial hash)
        if (hungerI < fp.hunger.forageHungerThreshold) {
            m_plantNeighbors.clear();
            m_plantGrid.queryNeighbors(px, py, fp.hunger.forageRange, m_plants, m_plantNeighbors);
            float bestDistSq = fp.hunger.forageRange * fp.hunger.forageRange;
            float bestPx = 0.0f, bestPy = 0.0f;
            for (int p : m_plantNeighbors) {
                float dx = m_plants.posX[p] - px;
                float dy = m_plants.posY[p] - py;
                float dSq = dx * dx + dy * dy;
                if (dSq < bestDistSq) {
                    bestDistSq = dSq;
                    bestPx = m_plants.posX[p];
                    bestPy = m_plants.posY[p];
                }
            }
            float forageRngSq = fp.hunger.forageRange * fp.hunger.forageRange;
            if (bestDistSq < forageRngSq) {
                float dist = std::sqrt(bestDistSq) + 0.0001f;
                float hungerFactor = 1.0f - hungerI / fp.hunger.forageHungerThreshold;
                float force = fp.hunger.forageWeight * fp.movement.maxSpeed * hungerFactor;
                m_forceX[i] += (bestPx - px) / dist * force;
                m_forceY[i] += (bestPy - py) / dist * force;
            }
        }

        // Boundary avoidance (soft force for SoftWall and Hybrid modes)
        BoundaryMode bm = m_globalParams.boundaryMode;
        if (bm == BoundaryMode::SoftWall || bm == BoundaryMode::Hybrid) {
            float bx = 0.0f, by = 0.0f;
            float margin = fp.boundary.boundaryMargin;
            if (px < margin)      bx = (margin - px) / margin;
            else if (px > m_worldW - margin) bx = -(px - (m_worldW - margin)) / margin;
            if (py < margin)      by = (margin - py) / margin;
            else if (py > m_worldH - margin) by = -(py - (m_worldH - margin)) / margin;
            m_forceX[i] += bx * fp.boundary.boundaryWeight * fp.movement.maxSpeed;
            m_forceY[i] += by * fp.boundary.boundaryWeight * fp.movement.maxSpeed;
        }

        // Wander
        // Phase 3.5: Sanity-modified wander (higher in UNEASY/PANICKED)
        float sanityWanderMod = 1.0f;
        if (sl < m_flockParams[fid].sanity.panickedThreshold)
            sanityWanderMod = m_flockParams[fid].sanity.panickedWanderMult;
        else if (sl < m_flockParams[fid].sanity.uneasyThreshold)
            sanityWanderMod = m_flockParams[fid].sanity.uneasyWanderMult;

        float angle = m_angleDist(m_rng) * 3.14159265f;
        m_forceX[i] += std::cos(angle) * fp.boundary.wanderWeight * fp.movement.maxSpeed * sanityWanderMod;
        m_forceY[i] += std::sin(angle) * fp.boundary.wanderWeight * fp.movement.maxSpeed * sanityWanderMod;

        // Target: only attracts IDLE boids (survival behaviors take priority)
        if (m_hasTarget
            && m_data.state[i] == static_cast<uint8_t>(BoidState::IDLE)) {
            float tx = m_targetX - px;
            float ty = m_targetY - py;
            float tDistSq = tx * tx + ty * ty;
            if (tDistSq > 1.0f) {
                float tDist = std::sqrt(tDistSq);
                float desiredX = tx / tDist * fp.movement.maxSpeed;
                float desiredY = ty / tDist * fp.movement.maxSpeed;
                m_forceX[i] += (desiredX - m_data.velX[i]) * fp.boundary.targetWeight;
                m_forceY[i] += (desiredY - m_data.velY[i]) * fp.boundary.targetWeight;
            }
        }

        // Phase 3.4: Apply disturbance forces from mouse clicks
        if (!m_disturbances.empty()) {
            for (auto& ds : m_disturbances) {
                if (ds.expired()) continue;
                float dx = px - ds.posX;
                float dy = py - ds.posY;
                float distSq = dx * dx + dy * dy;
                float radSq = ds.radius * ds.radius;
                if (distSq < radSq) {
                    float dist = std::sqrt(distSq) + 0.0001f;
                    float falloff = 1.0f - dist / ds.radius;
                    float sign = (ds.type == DisturbanceType::REPEL) ? 1.0f : -1.0f;
                    float force = ds.strength * falloff * fp.movement.maxSpeed * 1.5f;
                    m_forceX[i] += sign * (dx / dist) * force;
                    m_forceY[i] += sign * (dy / dist) * force;
                }
            }
        }
    }

    // ---- Phase 2.4: Defensive cooperation ----
    // When a flock-mate is killed, nearby allies counter-attack the predator.
    // Requires minimum group size (defenseGroupThreshold) to trigger mobbing.
    if (!m_killsThisFrame.empty()) {
        for (auto& kp : m_killsThisFrame) {
            if (kp.predatorIdx >= m_data.count || kp.preyIdx >= m_data.count) continue;
            float predX = m_data.posX[kp.predatorIdx];
            float predY = m_data.posY[kp.predatorIdx];
            int preyFid = m_data.flockId[kp.preyIdx];
            const FlockParams& preyFp = m_flockParams[preyFid];
            float defRadSq = preyFp.defense.defenseRadius
                           * preyFp.defense.defenseRadius;

            // Count nearby allies within defense radius
            int nearbyAllies = 0;
            for (int i = 0; i < m_data.count; ++i) {
                if (i == kp.preyIdx) continue;
                if (m_data.flockId[i] != preyFid) continue;
                if (m_data.hunger[i] <= 0.0f) continue;
                float dx = m_data.posX[i] - predX;
                float dy = m_data.posY[i] - predY;
                if (dx * dx + dy * dy < defRadSq) ++nearbyAllies;
            }

            if (nearbyAllies < static_cast<int>(preyFp.defense.defenseGroupThreshold))
                continue;

            // Apply defensive attraction toward predator for all nearby allies
            for (int i = 0; i < m_data.count; ++i) {
                if (i == kp.preyIdx) continue;
                if (m_data.flockId[i] != preyFid) continue;
                if (m_data.hunger[i] <= 0.0f) continue;
                float dx = m_data.posX[i] - predX;
                float dy = m_data.posY[i] - predY;
                float dSq = dx * dx + dy * dy;
                if (dSq < defRadSq) {
                    float dist = std::sqrt(dSq) + 0.0001f;
                    float force = preyFp.defense.defenseResponseWeight
                                  * preyFp.movement.maxSpeed
                                  / (dist + 10.0f);
                    m_forceX[i] += (-dx / dist) * force;
                    m_forceY[i] += (-dy / dist) * force;
                }
            }
        }
    }
}

// ---- Step 2: Apply forces, integrate positions, process kills, dead removal, weight decay, plants, reproduction ----
void Simulation::stepIntegration(float dt)
{
    float maxForceSq = m_params.movement.maxForce * m_params.movement.maxForce;

    // Update age stages before applying modifiers
    updateAgeStages();

    // ---- Apply forces ----
    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float hungerI = std::max(0.0f, std::min(1.0f, m_data.hunger[i]));

        // Hunger-based speed modulation
        float speedMult;
        if (fp.hunger.invertHungerSpeed) {
            // Satiated = fast, hungry = slow
            speedMult = fp.hunger.hungerSpeedMin + (fp.hunger.hungerSpeedMax - fp.hunger.hungerSpeedMin) * hungerI;
        } else {
            // Default: hungry = fast, satiated = slow
            speedMult = fp.hunger.hungerSpeedMax + (fp.hunger.hungerSpeedMin - fp.hunger.hungerSpeedMax) * hungerI;
        }

        // Weight-based speed modulation: heavier = slower, lighter = faster
        float weight = m_data.weight[i];
        float weightSpeedMod = 1.0f - (weight - 1.0f) * fp.movement.weightSpeedPenalty;

        // Age-based speed modulation
        float ageSpeedMod = 1.0f;
        switch (static_cast<AgeStage>(m_data.ageStage[i])) {
        case AgeStage::Juvenile: ageSpeedMod = fp.age.ageSpeedJuvenile; break;
        case AgeStage::Young:    ageSpeedMod = fp.age.ageSpeedYoung;    break;
        case AgeStage::Elder:    ageSpeedMod = fp.age.ageSpeedElder;    break;
        default:                 ageSpeedMod = fp.age.ageSpeedAdult;    break;
        }

        float effectiveMaxSpeed = fp.movement.maxSpeed * speedMult * weightSpeedMod * ageSpeedMod;

        // Fatigue-based speed penalty
        float fatigue = m_data.fatigue[i];
        float fatigueSpeedMod = 1.0f - fatigue * fp.fatigue.fatigueSpeedPenalty;
        effectiveMaxSpeed *= fatigueSpeedMod;

        // Gender-based speed difference
        float sexSpeedMod = (m_data.sex[i] == 0) ? fp.gender.sexSpeedMale : fp.gender.sexSpeedFemale;
        effectiveMaxSpeed *= sexSpeedMod;

        float effectiveMaxSpeedSq = effectiveMaxSpeed * effectiveMaxSpeed;

        float fx = m_forceX[i];
        float fy = m_forceY[i];
        float fMagSq = fx * fx + fy * fy;

        if (fMagSq > maxForceSq) {
            float scale = m_params.movement.maxForce / std::sqrt(fMagSq);
            fx *= scale;
            fy *= scale;
        }

        m_data.velX[i] += fx * dt;
        m_data.velY[i] += fy * dt;

        float speedSq = m_data.velX[i] * m_data.velX[i] + m_data.velY[i] * m_data.velY[i];
        if (speedSq > effectiveMaxSpeedSq) {
            float scale = effectiveMaxSpeed / std::sqrt(speedSq);
            m_data.velX[i] *= scale;
            m_data.velY[i] *= scale;
        }

        m_data.posX[i] += m_data.velX[i] * dt;
        m_data.posY[i] += m_data.velY[i] * dt;
        m_data.age[i] += dt;

        // Natural death: age exceeds lifespan
        if (m_data.age[i] > fp.age.maxLifespan)
            m_data.hunger[i] = -1.0f;  // Mark as dead (collectDead removes hunger <= 0)

        // Fatigue: accumulate when moving, recover when IDLE
        float currentSpeed = std::sqrt(m_data.velX[i] * m_data.velX[i] + m_data.velY[i] * m_data.velY[i]);
        float speedRatio = (effectiveMaxSpeed > 0.01f) ? (currentSpeed / effectiveMaxSpeed) : 0.0f;
        if (m_data.state[i] == static_cast<uint8_t>(BoidState::IDLE) && speedRatio < 0.1f) {
            m_data.fatigue[i] -= fp.fatigue.fatigueRecoveryRate * dt;
        } else {
            m_data.fatigue[i] += fp.fatigue.fatigueAccumRate * speedRatio * dt;
        }
        if (m_data.fatigue[i] < 0.0f) m_data.fatigue[i] = 0.0f;
        if (m_data.fatigue[i] > 1.0f) m_data.fatigue[i] = 1.0f;

        // Boundary handling based on mode
        BoundaryMode ibm = m_globalParams.boundaryMode;
        if (ibm == BoundaryMode::Torus) {
            wrapPosition(m_data.posX[i], m_data.posY[i]);
        }
        else if (ibm == BoundaryMode::HardWall) {
            // Elastic bounce
            if (m_data.posX[i] < 0.0f) {
                m_data.posX[i] = -m_data.posX[i];
                m_data.velX[i] = -m_data.velX[i];
            } else if (m_data.posX[i] > m_worldW) {
                m_data.posX[i] = 2.0f * m_worldW - m_data.posX[i];
                m_data.velX[i] = -m_data.velX[i];
            }
            if (m_data.posY[i] < 0.0f) {
                m_data.posY[i] = -m_data.posY[i];
                m_data.velY[i] = -m_data.velY[i];
            } else if (m_data.posY[i] > m_worldH) {
                m_data.posY[i] = 2.0f * m_worldH - m_data.posY[i];
                m_data.velY[i] = -m_data.velY[i];
            }
        }
        else if (ibm == BoundaryMode::SoftWall) {
            // Clamp at margin
            float margin = fp.boundary.boundaryMargin;
            if (m_data.posX[i] < margin) {
                m_data.posX[i] = margin;
                m_data.velX[i] *= 0.5f;
            } else if (m_data.posX[i] > m_worldW - margin) {
                m_data.posX[i] = m_worldW - margin;
                m_data.velX[i] *= 0.5f;
            }
            if (m_data.posY[i] < margin) {
                m_data.posY[i] = margin;
                m_data.velY[i] *= 0.5f;
            } else if (m_data.posY[i] > m_worldH - margin) {
                m_data.posY[i] = m_worldH - margin;
                m_data.velY[i] *= 0.5f;
            }
        }
        else {
            // Hybrid (default): soft repulsion + torus fallback
            wrapPosition(m_data.posX[i], m_data.posY[i]);
        }

        // Update per-boid render color based on sex and flock settings
        resolveBoidColor(fid, m_data.sex[i], m_data.colorR[i], m_data.colorG[i], m_data.colorB[i]);

        // Hunger flash (global toggle)
        if (m_globalParams.hungerFlashEnabled && hungerI < fp.hunger.hungerFlashThreshold) {
            float flashPhase = fmodf(m_simTime * 4.0f, 1.0f);  // 4 Hz
            if (flashPhase < 0.5f) {
                m_data.colorR[i] = 1.0f;
                m_data.colorG[i] = 0.15f;
                m_data.colorB[i] = 0.15f;
            }
        }
    }

    // ---- Apply predation kills (hunger + kill streak + weight gain) ----
    for (auto& kp : m_killsThisFrame) {
        if (kp.predatorIdx < m_data.count && kp.preyIdx < m_data.count) {
            m_data.hunger[kp.predatorIdx] = 1.0f;
            m_data.hunger[kp.preyIdx] = -1.0f;

            // Kill streak & weight gain
            int fid = m_data.flockId[kp.predatorIdx];
            const FlockParams& fp = m_flockParams[fid];
            float elapsed = m_simTime - m_data.lastKillTime[kp.predatorIdx];
            if (elapsed < fp.body.streakTimeout && m_data.killStreak[kp.predatorIdx] > 0) {
                // Consecutive kill: increase streak and weight
                m_data.killStreak[kp.predatorIdx]++;
                m_data.weight[kp.predatorIdx] += fp.body.weightGainPerKill;
            } else {
                // First kill or streak broken: reset counter
                m_data.killStreak[kp.predatorIdx] = 1;
            }
            if (m_data.weight[kp.predatorIdx] > fp.body.maxWeight)
                m_data.weight[kp.predatorIdx] = fp.body.maxWeight;
            if (m_data.weight[kp.predatorIdx] < fp.body.minWeight)
                m_data.weight[kp.predatorIdx] = fp.body.minWeight;
            m_data.lastKillTime[kp.predatorIdx] = m_simTime;
        }
    }

    // ---- Propagate hatred to prey flock-mates (Phase 2.2) ----
    updateHatred(dt);

    // ---- Collect and remove dead ----
    m_data.collectDead(m_deadIndices);
    std::sort(m_deadIndices.begin(), m_deadIndices.end(), std::greater<int>());

    m_deathPosX.clear();
    m_deathPosY.clear();
    for (int idx : m_deadIndices) {
        if (idx < m_data.count) {
            m_deathPosX.push_back(m_data.posX[idx]);
            m_deathPosY.push_back(m_data.posY[idx]);
            m_data.removeAt(idx);
        }
    }

    // ---- Weight decay over idle time ----
    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float elapsed = m_simTime - m_data.lastKillTime[i];
        if (elapsed > fp.body.decayDelay) {
            m_data.weight[i] -= fp.body.weightDecayRate * dt;
            if (m_data.weight[i] < fp.body.minWeight) {
                m_data.weight[i] = fp.body.minWeight;
                m_data.killStreak[i] = 0;
            }
        }
    }

    // ---- Plants + reproduction ----
    updatePlants(dt);
    updateReproduction(dt);
}

void Simulation::resolveBoidColor(int fid, int sex, float& r, float& g, float& b) const
{
    const FlockParams& fp = m_flockParams[fid];
    if (fp.appearance.useSexColors) {
        if (sex == 0) { r = fp.appearance.maleColorR; g = fp.appearance.maleColorG; b = fp.appearance.maleColorB; return; }
        else          { r = fp.appearance.femaleColorR; g = fp.appearance.femaleColorG; b = fp.appearance.femaleColorB; return; }
    }
    r = m_flockColorR[fid];
    g = m_flockColorG[fid];
    b = m_flockColorB[fid];
}

// ---- Plant + reproduction logic identical to before, adapted for vector access ----

void Simulation::updatePlants(float dt)
{
    auto& pp = m_plantParams;
    int season = currentSeason();
    float growthMul = PlantParams::growthMultiplier(season);

    // Fertilization from dead boids (using plant spatial hash)
    float fertRadiusSq = pp.fertilizeRadius * pp.fertilizeRadius;
    for (size_t d = 0; d < m_deathPosX.size(); ++d) {
        float dx = m_deathPosX[d];
        float dy = m_deathPosY[d];
        m_plantNeighbors.clear();
        m_plantGrid.queryNeighbors(dx, dy, pp.fertilizeRadius, m_plants, m_plantNeighbors);
        for (int p : m_plantNeighbors) {
            float pdx = m_plants.posX[p] - dx;
            float pdy = m_plants.posY[p] - dy;
            if (pdx * pdx + pdy * pdy < fertRadiusSq) {
                m_plants.growth[p] = std::min(1.0f, m_plants.growth[p] + pp.fertilizeBoost);
            }
        }
    }

    // Grazing: boids eat plants to restore hunger (energy conservation base)
    // Uses per-flock forage parameters for consistent threshold/distance.
    // Consumption is proportional to plant growth — partial energy transfer
    // rather than all-or-nothing. This is the foundation for a future
    // closed energy loop: plants -> boids -> death -> fertilization -> plants.
    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        if (m_data.hunger[i] >= fp.hunger.forageHungerThreshold) continue;

        float px = m_data.posX[i];
        float py = m_data.posY[i];
        float grazeRange = fp.hunger.forageRange;
        float grazeRangeSq = grazeRange * grazeRange;
        int bestPlant = -1;
        float bestDistSq = grazeRangeSq;

        m_plantNeighbors.clear();
        m_plantGrid.queryNeighbors(px, py, grazeRange, m_plants, m_plantNeighbors);
        for (int p : m_plantNeighbors) {
            float dx = m_plants.posX[p] - px;
            float dy = m_plants.posY[p] - py;
            float dSq = dx * dx + dy * dy;
            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                bestPlant = p;
            }
        }

        if (bestPlant >= 0) {
            float plantGrowth = m_plants.growth[bestPlant];
            float needed = 1.0f - m_data.hunger[i];
            float maxFromPlant = plantGrowth * pp.plantFoodValue;
            float gain = std::min(needed, maxFromPlant);
            m_data.hunger[i] += gain;

            // Phase 3.2: Record food source memory
            m_data.recordMemory(i, MemoryEvent::FOOD_SOURCE,
                                m_plants.posX[bestPlant], m_plants.posY[bestPlant],
                                0.6f, m_simTime);

            // Consume plant proportionally (energy transfer)
            float consumed = gain / pp.plantFoodValue;
            m_plants.growth[bestPlant] -= consumed;
            if (m_plants.growth[bestPlant] <= 0.0f) {
                m_plants.growth[bestPlant] = 0.0f;
                m_plants.regrowTimer[bestPlant] = pp.growthTime;
            }
        }
    }

    // Plant growth & spread
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    std::uniform_real_distribution<float> angleDist(-1.0f, 1.0f);

    struct NewPlant { float x, y; };
    std::vector<NewPlant> newPlants;
    newPlants.reserve(32);

    for (int p = 0; p < m_plants.count; ++p) {
        if (m_plants.growth[p] <= 0.0f) {
            if (m_plants.regrowTimer[p] > 0.0f) {
                m_plants.regrowTimer[p] -= dt;
                if (m_plants.regrowTimer[p] <= 0.0f) {
                    m_plants.growth[p] = 0.01f;
                }
            }
        }
        else {
            if (m_plants.growth[p] < 1.0f) {
                float growRate = growthMul / pp.growthTime;
                m_plants.growth[p] += growRate * dt;
                if (m_plants.growth[p] > 1.0f) m_plants.growth[p] = 1.0f;
            }

            if (m_plants.growth[p] >= 1.0f && season < 3 && m_plants.count < static_cast<int>(pp.maxPlants)) {
                if (roll(m_rng) < pp.spreadChance * dt * growthMul) {
                    float spreadX = m_plants.posX[p] + angleDist(m_rng) * pp.spreadRange;
                    float spreadY = m_plants.posY[p] + angleDist(m_rng) * pp.spreadRange;
                    if (spreadX < 5.0f) spreadX = 5.0f;
                    if (spreadX > m_worldW - 5.0f) spreadX = m_worldW - 5.0f;
                    if (spreadY < 5.0f) spreadY = 5.0f;
                    if (spreadY > m_worldH - 5.0f) spreadY = m_worldH - 5.0f;
                    newPlants.push_back({spreadX, spreadY});
                }
            }
        }
    }

    for (auto& np : newPlants) {
        if (m_plants.count >= static_cast<int>(pp.maxPlants)) break;
        m_plants.add(np.x, np.y, 0.02f);
    }
}

// ---- Phase 3.1: Nest system update ----
void Simulation::updateNests(float dt)
{
    if (m_nests.count == 0) return;

    // 1. Compute defense rating per nest (based on nearby flock boids)
    const NestParams& np = m_nestParams;
    for (int n = 0; n < m_nests.count; ++n) {
        int owner = m_nests.ownerFlock[n];
        if (owner < 0) continue;

        float nearbyBoids = 0.0f;
        float nestRadSq = np.nestRadius * np.nestRadius;
        for (int i = 0; i < m_data.count; ++i) {
            if (m_data.flockId[i] != owner) continue;
            float dx = m_data.posX[i] - m_nests.posX[n];
            float dy = m_data.posY[i] - m_nests.posY[n];
            if (dx * dx + dy * dy < nestRadSq) nearbyBoids += 1.0f;
        }
        m_nests.defenseRating[n] = nearbyBoids / std::max(1.0f, np.defenseThreshold);
    }

    // 2. Territory attraction: gently pull owned-flock boids toward their nest
    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        if (fp.nestPref.nestReturnWeight <= 0.0f) continue;
        if (m_data.state[i] == static_cast<uint8_t>(BoidState::FLEEING)) continue;
        if (m_data.state[i] == static_cast<uint8_t>(BoidState::HUNTING)) continue;

        // Find nearest nest owned by this flock
        int bestNest = -1;
        float bestDistSq = np.nestRadius * np.nestRadius * 4.0f;  // Attract only within 2x radius
        for (int n = 0; n < m_nests.count; ++n) {
            if (m_nests.ownerFlock[n] != fid) continue;
            float dx = m_nests.posX[n] - m_data.posX[i];
            float dy = m_nests.posY[n] - m_data.posY[i];
            float dsq = dx * dx + dy * dy;
            if (dsq < bestDistSq) {
                bestDistSq = dsq;
                bestNest = n;
            }
        }

        if (bestNest >= 0) {
            float dx = m_nests.posX[bestNest] - m_data.posX[i];
            float dy = m_nests.posY[bestNest] - m_data.posY[i];
            float dist = std::sqrt(bestDistSq) + 0.0001f;
            float weight = fp.nestPref.nestReturnWeight * fp.movement.maxSpeed
                         * std::min(1.0f, dist / (np.nestRadius * 2.0f));
            m_forceX[i] += (dx / dist) * weight;
            m_forceY[i] += (dy / dist) * weight;
        }
    }

    // 3. Nest benefits: health regen boost + food storage (Phase 3.1b)
    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float px = m_data.posX[i];
        float py = m_data.posY[i];

        // Find nearest owned nest within nestRadius
        int bestNest = -1;
        float bestDistSq = np.nestRadius * np.nestRadius;
        for (int n = 0; n < m_nests.count; ++n) {
            if (m_nests.ownerFlock[n] != fid) continue;
            float dx = m_nests.posX[n] - px;
            float dy = m_nests.posY[n] - py;
            float dsq = dx * dx + dy * dy;
            if (dsq < bestDistSq) { bestDistSq = dsq; bestNest = n; }
        }
        if (bestNest < 0) continue;

        // Phase 3.2: Record nest discovery memory
        m_data.recordMemory(i, MemoryEvent::NEST_DISCOVERY,
                            m_nests.posX[bestNest], m_nests.posY[bestNest],
                            0.9f, m_simTime);

        // a) Health regen boost when resting at owned nest
        if (m_data.health[i] < 1.0f && m_data.hunger[i] > 0.3f) {
            float boostRate = fp.health.healthRegenRate * np.nestHealthBoost;
            m_data.health[i] += boostRate * dt;
            if (m_data.health[i] > 1.0f) m_data.health[i] = 1.0f;
        }

        // b) Food storage: well-fed boids deposit excess hunger to nest
        if (m_data.hunger[i] > 0.5f && m_nests.foodStored[bestNest] < 1.0f) {
            float deposit = np.nestFoodStorageRate * dt;
            deposit = std::min(deposit, m_data.hunger[i] - 0.5f);
            deposit = std::min(deposit, 1.0f - m_nests.foodStored[bestNest]);
            m_data.hunger[i] -= deposit;
            m_nests.foodStored[bestNest] += deposit;
        }
    }

    // 4. Nest contest (Phase 3.1c): foreign boids can contest ownership
    //    If enough foreign boids occupy an owned nest for long enough,
    //    ownership transfers to the strongest challenger flock.
    int numFlocks = static_cast<int>(m_flockParams.size());
    for (int n = 0; n < m_nests.count; ++n) {
        int owner = m_nests.ownerFlock[n];
        if (owner < 0) continue;

        // Count foreign boids per flock within nestRadius
        int foreignCount[MAX_FLOCKS] = {};  // FIXME: stack array, bounded by MAX_FLOCKS=12
        float nestRadSq = np.nestRadius * np.nestRadius;
        for (int i = 0; i < m_data.count; ++i) {
            int fid = m_data.flockId[i];
            if (fid == owner) continue;
            float dx = m_data.posX[i] - m_nests.posX[n];
            float dy = m_data.posY[i] - m_nests.posY[n];
            if (dx * dx + dy * dy < nestRadSq)
                foreignCount[fid]++;
        }

        // Find strongest challenger (most boids from a single foreign flock)
        int bestAttacker = -1;
        int bestCount = 0;
        for (int f = 0; f < numFlocks; ++f) {
            if (foreignCount[f] > bestCount) {
                bestCount = foreignCount[f];
                bestAttacker = f;
            }
        }

        // Check if challenger exceeds defense threshold
        float defenseRequired = m_nests.defenseRating[n] * np.defenseThreshold;
        bool underAttack = (bestCount > 0) &&
                           (static_cast<float>(bestCount) > defenseRequired);

        if (underAttack) {
            m_nests.isContested[n] = 1;
            m_nests.contestAttacker[n] = bestAttacker;
            m_nests.contestTimer[n] += dt;

            // Ownership flips when contestTimer exceeds contestDuration
            if (m_nests.contestTimer[n] > np.contestDuration) {
                m_nests.ownerFlock[n] = bestAttacker;
                m_nests.isContested[n] = 0;
                m_nests.contestTimer[n] = 0.0f;
                m_nests.contestAttacker[n] = -1;
                m_nests.defenseRating[n] = 0.0f;
            }
        } else {
            // No threat or threat retreated: timer decays (2x speed)
            if (m_nests.contestTimer[n] > 0.0f) {
                m_nests.contestTimer[n] -= dt * 2.0f;
                if (m_nests.contestTimer[n] <= 0.0f) {
                    m_nests.contestTimer[n] = 0.0f;
                    m_nests.isContested[n] = 0;
                    m_nests.contestAttacker[n] = -1;
                }
            }
        }
    }
}

void Simulation::updateReproduction(float /*dt*/)
{
    if (m_data.count >= m_maxBoids) return;

    std::uniform_int_distribution<int> coinFlip(0, 1);

    int nFlock = static_cast<int>(m_flockParams.size());
    for (int f = 0; f < nFlock; ++f) {
        const FlockParams& fp = m_flockParams[f];

        if (m_simTime - m_lastReproductionTime[f] < fp.reproduction.reproductionInterval) continue;
        m_lastReproductionTime[f] = m_simTime;

        int males = 0, females = 0;
        float totalWeight = 0.0f;
        int weightCount = 0;
        std::vector<int> eligibleFemales;  // Indices of females eligible for mating
        eligibleFemales.reserve(m_data.count);
        for (int i = 0; i < m_data.count; ++i) {
            if (m_data.flockId[i] != f) continue;
            if (m_data.hunger[i] < fp.reproduction.reproductionMinHunger) continue;
            auto stage = static_cast<AgeStage>(m_data.ageStage[i]);
            if (stage == AgeStage::Juvenile || stage == AgeStage::Elder) continue;  // Only Young/Adult reproduce
            totalWeight += m_data.weight[i];
            ++weightCount;
            if (m_data.sex[i] == 0) {
                ++males;
            } else {
                // Postpartum recovery check
                if (m_simTime - m_data.lastBirthTime[i] >= fp.pregnancy.postpartumRecovery) {
                    ++females;
                    eligibleFemales.push_back(i);
                }
            }
        }
        float avgWeight = (weightCount > 0) ? (totalWeight / weightCount) : 1.0f;
        int pairs = std::min(males, static_cast<int>(eligibleFemales.size()));
        if (pairs == 0) continue;

        int flockSize = countInFlock(f);
        if (flockSize >= fp.reproduction.maxFlockSize) continue;

        float capScale = 1.0f;
        float softCap = fp.reproduction.maxFlockSize * 0.9f;
        if (flockSize > softCap)
            capScale = static_cast<float>(fp.reproduction.maxFlockSize - flockSize)
                     / static_cast<float>(fp.reproduction.maxFlockSize - static_cast<int>(softCap));

        int globalRoom = m_maxBoids - m_data.count;
        if (globalRoom <= 0) continue;  // use continue to let subsequent flocks also check

        const float* fc = flockColor(f);
        std::uniform_real_distribution<float> offsetDist(-fp.perception.separationRadius, fp.perception.separationRadius);
        std::uniform_real_distribution<float> velDist(-80.0f, 80.0f);

        // Phase 3.1b: Prefer nest-based spawning if flock owns a nest with food
        float spawnX = 0.0f, spawnY = 0.0f;
        bool foundSpawn = false;

        int bestNest = -1;
        float bestFood = 0.0f;
        for (int n = 0; n < m_nests.count; ++n) {
            if (m_nests.ownerFlock[n] != f) continue;
            if (m_nests.foodStored[n] > bestFood) {
                bestFood = m_nests.foodStored[n];
                bestNest = n;
            }
        }

        if (bestNest >= 0 && bestFood > 0.02f) {
            // Spawn at nest, consume some stored food
            spawnX = m_nests.posX[bestNest];
            spawnY = m_nests.posY[bestNest];
            m_nests.foodStored[bestNest] -= 0.02f;  // Reproduction cost
            if (m_nests.foodStored[bestNest] < 0.0f)
                m_nests.foodStored[bestNest] = 0.0f;
            foundSpawn = true;
        } else {
            // Fallback: spawn near a parent boid
            for (int i = 0; i < m_data.count; ++i) {
                if (m_data.flockId[i] == f && m_data.hunger[i] >= fp.reproduction.reproductionMinHunger) {
                    spawnX = m_data.posX[i];
                    spawnY = m_data.posY[i];
                    foundSpawn = true;
                    break;
                }
            }
        }
        if (!foundSpawn) continue;

        int spawned = 0;
        int maxNew = std::min(globalRoom, static_cast<int>(pairs * fp.reproduction.reproductionMaxOffspring));
        maxNew = std::min(maxNew, fp.reproduction.maxFlockSize - flockSize);
        maxNew = static_cast<int>(maxNew * capScale);
        if (maxNew <= 0) continue;

        int maxOffspringInt = static_cast<int>(fp.reproduction.reproductionMaxOffspring);
        int minOffspringInt = static_cast<int>(fp.reproduction.reproductionMinOffspring);
        for (int p = 0; p < pairs && spawned < maxNew; ++p) {
            int babies = minOffspringInt +
                         rand() % (maxOffspringInt - minOffspringInt + 1);
            babies = std::min(babies, maxNew - spawned);

            // Mark mother's last birth time for postpartum recovery
            if (p < static_cast<int>(eligibleFemales.size()))
                m_data.lastBirthTime[eligibleFemales[p]] = m_simTime;

            for (int b = 0; b < babies; ++b) {
                float nx = spawnX + offsetDist(m_rng);
                float ny = spawnY + offsetDist(m_rng);
                wrapPosition(nx, ny);
                uint8_t childSex = coinFlip(m_rng);
                m_data.add(nx, ny, velDist(m_rng), velDist(m_rng),
                           f, fc[0], fc[1], fc[2], childSex, avgWeight);
                // Nursing: offspring get hunger boost
                m_data.hunger[m_data.count - 1] += fp.pregnancy.offspringHungerBoost;
                if (m_data.hunger[m_data.count - 1] > 1.0f)
                    m_data.hunger[m_data.count - 1] = 1.0f;
                ++spawned;
            }
        }
    }
}

// ---- Dynamic flock management ----

int Simulation::addFlock()
{
    int n = static_cast<int>(m_flockParams.size());
    if (n >= MAX_FLOCKS) return -1;

    // L2 check: deny new flock if global boid count is already at capacity
    // (empty flock has no room to grow)
    if (m_data.count >= m_maxBoids) return -2;

    int newId = n;
    m_flockParams.push_back(FlockParams{});
    m_flockNames.push_back(defaultFlockName(newId));

    int colorIdx = newId % 12;
    m_flockColorR.push_back(DEFAULT_FLOCK_COLORS[colorIdx][0]);
    m_flockColorG.push_back(DEFAULT_FLOCK_COLORS[colorIdx][1]);
    m_flockColorB.push_back(DEFAULT_FLOCK_COLORS[colorIdx][2]);

    m_lastReproductionTime.push_back(m_simTime);

    // Expand relationship matrix (N x N -> (N+1) x (N+1))
    for (auto& row : m_relationships)
        row.push_back(FlockRelation::Neutral);
    std::vector<FlockRelation> newRow(n + 1, FlockRelation::Neutral);
    m_relationships.push_back(newRow);

    return newId;
}

bool Simulation::removeFlock(int id)
{
    int n = static_cast<int>(m_flockParams.size());
    if (n <= MIN_FLOCKS || id < 0 || id >= n) return false;

    // Remove all boids of this flock
    removeBoidsFromFlock(id, countInFlock(id));

    // Shift flock IDs of boids in higher-indexed flocks
    for (int i = 0; i < m_data.count; ++i) {
        if (m_data.flockId[i] > id)
            m_data.flockId[i]--;
    }

    // Remove flock data
    m_flockParams.erase(m_flockParams.begin() + id);
    m_flockNames.erase(m_flockNames.begin() + id);
    m_flockColorR.erase(m_flockColorR.begin() + id);
    m_flockColorG.erase(m_flockColorG.begin() + id);
    m_flockColorB.erase(m_flockColorB.begin() + id);
    m_lastReproductionTime.erase(m_lastReproductionTime.begin() + id);

    // Shrink relationship matrix
    m_relationships.erase(m_relationships.begin() + id);
    for (auto& row : m_relationships)
        row.erase(row.begin() + id);

    // Fix active flock
    if (m_activeFlock >= n - 1)
        m_activeFlock = n - 2;
    m_params = m_flockParams[m_activeFlock];

    return true;
}

// ---- Standard methods ----

void Simulation::wrapPosition(float& x, float& y) const
{
    if (x < 0.0f) x += m_worldW;
    else if (x > m_worldW) x -= m_worldW;
    if (y < 0.0f) y += m_worldH;
    else if (y > m_worldH) y -= m_worldH;
}

void Simulation::addBoid(float x, float y)
{
    if (m_data.count >= m_maxBoids) return;
    // Per-flock capacity check
    int flockCap = m_flockParams[m_activeFlock].reproduction.maxFlockSize;
    if (countInFlock(m_activeFlock) >= flockCap) return;
    std::uniform_real_distribution<float> velDist(-100.0f, 100.0f);
    std::uniform_int_distribution<int> coinFlip(0, 1);
    float vx = velDist(m_rng);
    float vy = velDist(m_rng);
    const float* c = flockColor(m_activeFlock);
    m_data.add(x, y, vx, vy, m_activeFlock, c[0], c[1], c[2], coinFlip(m_rng));
}

int Simulation::spawnRandom(int count)
{
    int requested = count;

    // ---- Global capacity check (L2: m_maxBoids) ----
    if (m_data.count >= m_maxBoids) return 0;
    if (m_data.count + count > m_maxBoids)
        count = m_maxBoids - m_data.count;
    if (count <= 0) return 0;

    // ---- Per-flock capacity check (L3: maxFlockSize) ----
    int flockCap = m_flockParams[m_activeFlock].reproduction.maxFlockSize;
    int current = countInFlock(m_activeFlock);
    int flockRoom = flockCap - current;
    if (flockRoom <= 0) return 0;
    if (count > flockRoom) count = flockRoom;
    if (count <= 0) return 0;

    std::uniform_real_distribution<float> xDist(50.0f, m_worldW - 50.0f);
    std::uniform_real_distribution<float> yDist(50.0f, m_worldH - 50.0f);
    std::uniform_real_distribution<float> velDist(-100.0f, 100.0f);
    const float* c = flockColor(m_activeFlock);
    for (int i = 0; i < count; ++i) {
        uint8_t s = (i % 2 == 0) ? 0 : 1;
        m_data.add(xDist(m_rng), yDist(m_rng), velDist(m_rng), velDist(m_rng),
                   m_activeFlock, c[0], c[1], c[2], s);
    }
    return count;
}

void Simulation::setGlobalFlockCap(int cap)
{
    if (cap < 1) cap = 1;
    if (cap > 2000) cap = 2000;
    for (auto& fp : m_flockParams)
        fp.reproduction.maxFlockSize = cap;
    m_params.reproduction.maxFlockSize = cap;  // keep editor copy in sync
}

std::vector<std::string> Simulation::flockSpriteNames() const
{
    std::vector<std::string> names;
    names.reserve(m_flockParams.size());
    for (const auto& fp : m_flockParams)
        names.push_back(fp.appearance.spriteName);
    return names;
}

std::vector<bool> Simulation::flockUprightFlags() const
{
    std::vector<bool> flags;
    flags.reserve(m_flockParams.size());
    for (const auto& fp : m_flockParams)
        flags.push_back(fp.appearance.uprightSprite);
    return flags;
}

std::vector<float> Simulation::flockAgeSizes() const
{
    std::vector<float> sizes;
    sizes.reserve(m_flockParams.size() * 4);
    for (const auto& fp : m_flockParams) {
        sizes.push_back(fp.age.ageSizeJuvenile);
        sizes.push_back(fp.age.ageSizeYoung);
        sizes.push_back(fp.age.ageSizeAdult);
        sizes.push_back(fp.age.ageSizeElder);
    }
    return sizes;
}

std::vector<float> Simulation::flockSexSizes() const
{
    std::vector<float> sizes;
    sizes.reserve(m_flockParams.size() * 2);
    for (const auto& fp : m_flockParams) {
        sizes.push_back(fp.gender.sexSizeMale);
        sizes.push_back(fp.gender.sexSizeFemale);
    }
    return sizes;
}

void Simulation::removeBoidAt(int index)
{
    if (index < 0 || index >= m_data.count) return;
    m_data.removeAt(index);
}

void Simulation::setTarget(float x, float y) { m_targetX = x; m_targetY = y; m_targetTime = m_simTime; m_hasTarget = true; }
void Simulation::clearTarget() { m_hasTarget = false; m_targetTime = -1e9f; }

void Simulation::addDisturbance(float x, float y, DisturbanceType type)
{
    DisturbanceSource ds;
    ds.posX = x;
    ds.posY = y;
    ds.strength = 1.0f;
    ds.radius = 150.0f;
    ds.type = type;
    m_disturbances.push_back(ds);
    if (m_disturbances.size() > 32)
        m_disturbances.erase(m_disturbances.begin());  // Cap at 32 (oldest first)
}

void Simulation::updateDisturbances(float dt)
{
    float decayRate = 0.33f;  // Full decay in ~3 seconds
    size_t write = 0;
    for (size_t i = 0; i < m_disturbances.size(); ++i) {
        m_disturbances[i].strength -= decayRate * dt;
        if (!m_disturbances[i].expired()) {
            if (write != i)
                m_disturbances[write] = m_disturbances[i];
            ++write;
        }
    }
    m_disturbances.resize(write);
}

void Simulation::updateSanity(float dt)
{
    if (m_flockParams.empty()) return;
    const NestParams& np = m_nestParams;
    int nBoids = m_data.count;

    for (int i = 0; i < nBoids; ++i) {
        int fid = m_data.flockId[i];
        const auto& sp = m_flockParams[fid].sanity;
        float s = m_data.sanityLevel[i];

        // ---- Decay factors ----
        float decay = 0.0f;

        // Predator memory intensity (query nearby)
        float predMem = m_data.queryMemory(i, MemoryEvent::PREDATOR_SIGHTING,
                                           m_data.posX[i], m_data.posY[i],
                                           200.0f, m_simTime);
        decay += predMem * sp.memoryDecayScale;

        // Hostile encounter memory
        float hostMem = m_data.queryMemory(i, MemoryEvent::HOSTILE_ENCOUNTER,
                                           m_data.posX[i], m_data.posY[i],
                                           200.0f, m_simTime);
        decay += hostMem * sp.memoryDecayScale * 0.5f;

        // Hunger decay
        if (m_data.hunger[i] < sp.hungerDecayTrigger) {
            decay += sp.sanityDecayRate * (sp.hungerDecayTrigger - m_data.hunger[i]) / sp.hungerDecayTrigger;
        }

        // Fatigue decay
        if (m_data.fatigue[i] > sp.fatigueDecayTrigger) {
            decay += sp.sanityDecayRate * (m_data.fatigue[i] - sp.fatigueDecayTrigger) / (1.0f - sp.fatigueDecayTrigger);
        }

        s -= decay * dt;

        // ---- Recovery factors ----
        float recovery = sp.sanityRecoveryRate;

        // Nest recovery boost
        if (m_data.state[i] == static_cast<uint8_t>(BoidState::IDLE)) {
            float nestRadSq = np.nestRadius * np.nestRadius;
            for (int n = 0; n < m_nests.count; ++n) {
                if (m_nests.ownerFlock[n] != fid) continue;
                float dx = m_nests.posX[n] - m_data.posX[i];
                float dy = m_nests.posY[n] - m_data.posY[i];
                if (dx * dx + dy * dy < nestRadSq) {
                    recovery *= sp.nestRecoveryBoost;
                    break;
                }
            }
        }

        s += recovery * dt;

        // Clamp
        if (s < 0.0f) s = 0.0f;
        if (s > 1.0f) s = 1.0f;
        m_data.sanityLevel[i] = s;
    }
}

void Simulation::decayBoidMemories(float dt)
{
    // Build per-event-type decay rates from the first flock's MemorySuf
    // (decay is global; per-flock parameters are identical unless explicitly edited)
    if (m_flockParams.empty()) return;
    const auto& mem = m_flockParams[0].memory;
    float decayRates[static_cast<int>(MemoryEvent::COUNT)];
    decayRates[static_cast<int>(MemoryEvent::PREDATOR_SIGHTING)] = mem.memoryDecayRate * mem.predatorMemoryDecayMult;
    decayRates[static_cast<int>(MemoryEvent::PREY_SIGHTING)]     = mem.memoryDecayRate * mem.preyMemoryDecayMult;
    decayRates[static_cast<int>(MemoryEvent::FOOD_SOURCE)]       = mem.memoryDecayRate * mem.foodMemoryDecayMult;
    decayRates[static_cast<int>(MemoryEvent::NEST_DISCOVERY)]    = mem.memoryDecayRate * mem.nestMemoryDecayMult;
    decayRates[static_cast<int>(MemoryEvent::HOSTILE_ENCOUNTER)] = mem.memoryDecayRate * mem.hostileMemoryDecayMult;
    decayRates[static_cast<int>(MemoryEvent::MATE_SIGHTING)]     = mem.memoryDecayRate * mem.mateMemoryDecayMult;

    m_data.decayMemories(dt, m_simTime, decayRates, mem.memoryIntensityThreshold);
}

void Simulation::updateGrid()
{
    float maxCohRad = m_params.perception.cohesionRadius;
    for (auto& fp : m_flockParams)
        maxCohRad = std::max(maxCohRad, fp.perception.cohesionRadius);
    m_grid.reinit(m_worldW, m_worldH, maxCohRad * 1.5f, m_maxBoids);
}

void Simulation::saveCurrentParams()
{
    m_flockParams[m_activeFlock] = m_params;
    updateGrid();
}

void Simulation::loadFlockParams(int id)
{
    m_params = m_flockParams[id];
}

void Simulation::setActiveFlock(int id)
{
    int n = static_cast<int>(m_flockParams.size());
    if (id < 0 || id >= n) return;
    saveCurrentParams();
    m_activeFlock = id;
    loadFlockParams(id);
}

int Simulation::countInFlock(int flockId) const
{
    int cnt = 0;
    for (int i = 0; i < m_data.count; ++i) {
        if (m_data.flockId[i] == flockId) ++cnt;
    }
    return cnt;
}

int Simulation::removeBoidsFromFlock(int flockId, int count)
{
    int removed = 0;
    for (int i = m_data.count - 1; i >= 0 && removed < count; --i) {
        if (m_data.flockId[i] == flockId) {
            m_data.removeAt(i);
            ++removed;
        }
    }
    return removed;
}

const float* Simulation::flockColor(int id) const
{
    static float buf[3];
    int n = static_cast<int>(m_flockColorR.size());
    if (n == 0) { buf[0] = buf[1] = buf[2] = 1.0f; return buf; }
    if (id < 0) id = 0;
    if (id >= n) id = n - 1;
    buf[0] = m_flockColorR[id];
    buf[1] = m_flockColorG[id];
    buf[2] = m_flockColorB[id];
    return buf;
}

void Simulation::setFlockColor(int id, float r, float g, float b)
{
    int n = static_cast<int>(m_flockColorR.size());
    if (id < 0 || id >= n) return;
    m_flockColorR[id] = r;
    m_flockColorG[id] = g;
    m_flockColorB[id] = b;
}

std::string Simulation::defaultFlockName(int id) const
{
    // Generate "Flock A", "Flock B", ..., "Flock L"
    char letter = static_cast<char>('A' + (id % 26));
    return std::string("Flock ") + letter;
}

FlockRelation Simulation::relationship(int viewer, int target) const
{
    return m_relationships[viewer][target];
}

void Simulation::setRelationship(int viewer, int target, FlockRelation rel)
{
    m_relationships[viewer][target] = rel;
    if (viewer != target) {
        if (rel == FlockRelation::Predator)
            m_relationships[target][viewer] = FlockRelation::Prey;
        else if (rel == FlockRelation::Prey)
            m_relationships[target][viewer] = FlockRelation::Predator;
        else if (rel == FlockRelation::Neutral)
            m_relationships[target][viewer] = FlockRelation::Neutral;
    }
}
