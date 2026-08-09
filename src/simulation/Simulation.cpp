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
    float maxCohRad = m_params.cohesionRadius;
    for (auto& fp : m_flockParams)
        maxCohRad = std::max(maxCohRad, fp.cohesionRadius);
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
}

void Simulation::update(float dt)
{
    m_simTime += dt;
    if (m_data.count == 0) { updatePlants(dt); return; }

    // ---- Step 0: Hunger decay for all boids ----
    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        m_data.hunger[i] -= fp.hungerDecayRate * dt;
        if (m_data.hunger[i] > 1.0f) m_data.hunger[i] = 1.0f;
    }

    // Rebuild spatial hash grids
    m_grid.rebuild(m_data);
    m_plantGrid.rebuild(m_plants);

    float maxForceSq = m_params.maxForce * m_params.maxForce;

    // Compute max cohesion radius for inter-flock detection range
    float maxCohRadius = m_params.cohesionRadius;
    for (auto& fp : m_flockParams)
        maxCohRadius = std::max(maxCohRadius, fp.cohesionRadius);
    float interFlockRadiusSq = maxCohRadius * maxCohRadius;

    float maxChaseRange = 0.0f;
    for (auto& fp : m_flockParams)
        maxChaseRange = std::max(maxChaseRange, fp.chaseRange);

    std::fill(m_forceX.begin(), m_forceX.begin() + m_data.count, 0.0f);
    std::fill(m_forceY.begin(), m_forceY.begin() + m_data.count, 0.0f);

    struct KillPair { int predatorIdx; int preyIdx; };
    std::vector<KillPair> killsThisFrame;
    killsThisFrame.reserve(256);

    for (int i = 0; i < m_data.count; ++i) {
        float px = m_data.posX[i];
        float py = m_data.posY[i];
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float hungerI = m_data.hunger[i];
        bool killedThisFrame = false;

        float sepRadiusSq = fp.separationRadius * fp.separationRadius;
        float aliRadiusSq = fp.alignmentRadius * fp.alignmentRadius;
        float cohRadiusSq = fp.cohesionRadius * fp.cohesionRadius;

        float hardRadius = fp.hardCollisionRadius;
        float hardRadiusSq = hardRadius * hardRadius;
        float chaseRangeSq = fp.chaseRange * fp.chaseRange;

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
                    float force = (overlap / hardRadius) * fp.maxSpeed * fp.separationWeight * 3.0f;
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
            }
            else {
                // --- Different-flock ---
                FlockRelation rel = m_relationships[fid][jFid];
                const FlockParams& jp = m_flockParams[jFid];
                float hungerJ = m_data.hunger[j];

                if (rel == FlockRelation::Neutral) {
                    if (distSq < interFlockRadiusSq) {
                        float dist = std::sqrt(distSq) + 0.0001f;
                        float force = fp.interFlockRepulsionWeight * fp.maxSpeed
                                      / (distSq / interFlockRadiusSq + 0.1f);
                        ifSepX -= (dx / dist) * force;
                        ifSepY -= (dy / dist) * force;
                    }
                }
                else if (rel == FlockRelation::Predator) {
                    // Determine chase participation based on hunger level
                    bool participatesInChase = false;
                    if (hungerI < fp.predationKillHunger) {
                        // Below kill threshold: 100% participate in chase
                        participatesInChase = true;
                    } else if (hungerI < fp.predationMinHunger) {
                        // Between kill and min thresholds: linear interpolation
                        float range = fp.predationMinHunger - fp.predationKillHunger;
                        float t = (fp.predationMinHunger - hungerI) / (range + 0.0001f);
                        float rate = fp.predationParticipationRate
                                   + (1.0f - fp.predationParticipationRate) * t;
                        participatesInChase = ((i % 100) < static_cast<int>(rate * 100.0f));
                    } else {
                        // Above min hunger: base participation rate only
                        participatesInChase = ((i % 100)
                            < static_cast<int>(fp.predationParticipationRate * 100.0f));
                    }

                    if (participatesInChase) {
                    float preyRadiusSq = interFlockRadiusSq * 2.0f;
                    if (distSq < preyRadiusSq) {
                        float dist = std::sqrt(distSq) + 0.0001f;
                        float force = fp.predatorAttractionWeight * fp.maxSpeed
                                      * std::min(1.0f, distSq / interFlockRadiusSq)
                                      / (distSq / preyRadiusSq + 0.05f);
                        ifSepX += (dx / dist) * force;
                        ifSepY += (dy / dist) * force;
                        if (distSq < hardRadiusSq * 4.0f) {
                            float closeForce = fp.maxSpeed * 2.0f
                                               / (distSq / hardRadiusSq + 0.01f);
                            ifSepX -= (dx / dist) * closeForce;
                            ifSepY -= (dy / dist) * closeForce;
                        }
                    }

                    // Predation check -- only when below kill threshold
                    if (!killedThisFrame && distSq < chaseRangeSq
                        && hungerI < fp.predationKillHunger && hungerJ > 0.0f) {
                        bool alreadyDead = false;
                        for (auto& kp : killsThisFrame) {
                            if (kp.preyIdx == j) { alreadyDead = true; break; }
                        }
                        if (!alreadyDead) {
                            float distRatio = 1.0f - std::sqrt(distSq) / fp.chaseRange;
                            float chaseProb = fp.chaseSuccessBase
                                            * (2.0f - std::min(hungerI, 1.0f))
                                            * (0.5f + 0.5f * distRatio);
                            std::uniform_real_distribution<float> roll(0.0f, 1.0f);
                            if (roll(m_rng) < chaseProb) {
                                killsThisFrame.push_back({i, j});
                                killedThisFrame = true;
                                hungerI = 1.0f;
                            }
                            else {
                                float escapeProb = jp.escapeSuccessBase
                                                 * std::max(hungerJ, 0.1f)
                                                 * (0.6f + 0.4f * distRatio);
                                if (roll(m_rng) < escapeProb) {
                                    float boostDirX = -m_data.velX[j];
                                    float boostDirY = -m_data.velY[j];
                                    float bmag = std::sqrt(boostDirX * boostDirX + boostDirY * boostDirY);
                                    if (bmag > 0.001f) {
                                        float boost = fp.maxSpeed * 1.8f * 0.5f;
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
                    // Prey: flee
                    float fearRadiusSq = interFlockRadiusSq * 2.5f;
                    if (distSq < fearRadiusSq) {
                        float dist = std::sqrt(distSq) + 0.0001f;
                        float force = fp.preyFearWeight * fp.maxSpeed
                                      / (distSq / interFlockRadiusSq + 0.03f);
                        ifSepX -= (dx / dist) * force;
                        ifSepY -= (dy / dist) * force;
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
            aliX = aliX / aliMag * fp.maxSpeed;
            aliY = aliY / aliMag * fp.maxSpeed;
        }

        // Normalize cohesion
        if (cohCount > 0) {
            cohX = cohX / static_cast<float>(cohCount) - px;
            cohY = cohY / static_cast<float>(cohCount) - py;
        }
        float cohMag = std::sqrt(cohX * cohX + cohY * cohY);
        if (cohMag > 0.001f) {
            cohX = cohX / cohMag * fp.maxSpeed;
            cohY = cohY / cohMag * fp.maxSpeed;
        }

        float sepMag = std::sqrt(sepX * sepX + sepY * sepY);
        float sepMax = fp.maxSpeed * fp.separationWeight * 2.0f;
        if (sepMag > sepMax) {
            sepX = sepX / sepMag * sepMax;
            sepY = sepY / sepMag * sepMax;
        }

        m_forceX[i] = sepX + aliX * fp.alignmentWeight
                    + cohX * fp.cohesionWeight + ifSepX;
        m_forceY[i] = sepY + aliY * fp.alignmentWeight
                    + cohY * fp.cohesionWeight + ifSepY;

        // Foraging (using plant spatial hash)
        if (hungerI < fp.forageHungerThreshold) {
            m_plantNeighbors.clear();
            m_plantGrid.queryNeighbors(px, py, fp.forageRange, m_plants, m_plantNeighbors);
            float bestDistSq = fp.forageRange * fp.forageRange;
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
            float forageRngSq = fp.forageRange * fp.forageRange;
            if (bestDistSq < forageRngSq) {
                float dist = std::sqrt(bestDistSq) + 0.0001f;
                float hungerFactor = 1.0f - hungerI / fp.forageHungerThreshold;
                float force = fp.forageWeight * fp.maxSpeed * hungerFactor;
                m_forceX[i] += (bestPx - px) / dist * force;
                m_forceY[i] += (bestPy - py) / dist * force;
            }
        }

        // Boundary avoidance (soft force: only in wrap mode; hard mode uses clamp below)
        if (m_globalParams.wrapBoundary) {
            float bx = 0.0f, by = 0.0f;
            float margin = fp.boundaryMargin;
            if (px < margin)      bx = (margin - px) / margin;
            else if (px > m_worldW - margin) bx = -(px - (m_worldW - margin)) / margin;
            if (py < margin)      by = (margin - py) / margin;
            else if (py > m_worldH - margin) by = -(py - (m_worldH - margin)) / margin;
            m_forceX[i] += bx * fp.boundaryWeight * fp.maxSpeed;
            m_forceY[i] += by * fp.boundaryWeight * fp.maxSpeed;
        }

        // Wander
        float angle = m_angleDist(m_rng) * 3.14159265f;
        m_forceX[i] += std::cos(angle) * fp.wanderWeight * fp.maxSpeed;
        m_forceY[i] += std::sin(angle) * fp.wanderWeight * fp.maxSpeed;

        // Target
        if (m_hasTarget) {
            float tx = m_targetX - px;
            float ty = m_targetY - py;
            float tDistSq = tx * tx + ty * ty;
            if (tDistSq > 1.0f) {
                float tDist = std::sqrt(tDistSq);
                float desiredX = tx / tDist * fp.maxSpeed;
                float desiredY = ty / tDist * fp.maxSpeed;
                m_forceX[i] += (desiredX - m_data.velX[i]) * fp.targetWeight;
                m_forceY[i] += (desiredY - m_data.velY[i]) * fp.targetWeight;
            }
        }
    }

    // ---- Apply forces ----
    for (int i = 0; i < m_data.count; ++i) {
        int fid = m_data.flockId[i];
        const FlockParams& fp = m_flockParams[fid];
        float hungerI = std::max(0.0f, std::min(1.0f, m_data.hunger[i]));

        // Hunger-based speed modulation
        float speedMult;
        if (fp.invertHungerSpeed) {
            // Satiated = fast, hungry = slow
            speedMult = fp.hungerSpeedMin + (fp.hungerSpeedMax - fp.hungerSpeedMin) * hungerI;
        } else {
            // Default: hungry = fast, satiated = slow
            speedMult = fp.hungerSpeedMax + (fp.hungerSpeedMin - fp.hungerSpeedMax) * hungerI;
        }

        // Weight-based speed modulation: heavier = slower, lighter = faster
        float weight = m_data.weight[i];
        float weightSpeedMod = 1.0f - (weight - 1.0f) * fp.weightSpeedPenalty;

        float effectiveMaxSpeed = fp.maxSpeed * speedMult * weightSpeedMod;
        float effectiveMaxSpeedSq = effectiveMaxSpeed * effectiveMaxSpeed;

        float fx = m_forceX[i];
        float fy = m_forceY[i];
        float fMagSq = fx * fx + fy * fy;

        if (fMagSq > maxForceSq) {
            float scale = m_params.maxForce / std::sqrt(fMagSq);
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

        // Boundary: toroidal wrap vs hard collision walls
        if (m_globalParams.wrapBoundary) {
            wrapPosition(m_data.posX[i], m_data.posY[i]);
        } else {
            float margin = fp.boundaryMargin;
            if (m_data.posX[i] < margin) {
                m_data.posX[i] = margin;
                m_data.velX[i] = -m_data.velX[i];
            } else if (m_data.posX[i] > m_worldW - margin) {
                m_data.posX[i] = m_worldW - margin;
                m_data.velX[i] = -m_data.velX[i];
            }
            if (m_data.posY[i] < margin) {
                m_data.posY[i] = margin;
                m_data.velY[i] = -m_data.velY[i];
            } else if (m_data.posY[i] > m_worldH - margin) {
                m_data.posY[i] = m_worldH - margin;
                m_data.velY[i] = -m_data.velY[i];
            }
        }

        // Update per-boid render color based on sex and flock settings
        resolveBoidColor(fid, m_data.sex[i], m_data.colorR[i], m_data.colorG[i], m_data.colorB[i]);

        // Hunger flash (global toggle)
        if (m_globalParams.hungerFlashEnabled && hungerI < fp.hungerFlashThreshold) {
            float flashPhase = fmodf(m_simTime * 4.0f, 1.0f);  // 4 Hz
            if (flashPhase < 0.5f) {
                m_data.colorR[i] = 1.0f;
                m_data.colorG[i] = 0.15f;
                m_data.colorB[i] = 0.15f;
            }
        }
    }

    // ---- Apply predation kills (hunger + kill streak + weight gain) ----
    for (auto& kp : killsThisFrame) {
        if (kp.predatorIdx < m_data.count && kp.preyIdx < m_data.count) {
            m_data.hunger[kp.predatorIdx] = 1.0f;
            m_data.hunger[kp.preyIdx] = -1.0f;

            // Kill streak & weight gain
            int fid = m_data.flockId[kp.predatorIdx];
            const FlockParams& fp = m_flockParams[fid];
            float elapsed = m_simTime - m_data.lastKillTime[kp.predatorIdx];
            if (elapsed < fp.streakTimeout && m_data.killStreak[kp.predatorIdx] > 0) {
                // Consecutive kill: increase streak and weight
                m_data.killStreak[kp.predatorIdx]++;
                m_data.weight[kp.predatorIdx] += fp.weightGainPerKill;
            } else {
                // First kill or streak broken: reset counter
                m_data.killStreak[kp.predatorIdx] = 1;
            }
            if (m_data.weight[kp.predatorIdx] > fp.maxWeight)
                m_data.weight[kp.predatorIdx] = fp.maxWeight;
            if (m_data.weight[kp.predatorIdx] < fp.minWeight)
                m_data.weight[kp.predatorIdx] = fp.minWeight;
            m_data.lastKillTime[kp.predatorIdx] = m_simTime;
        }
    }

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
        if (elapsed > fp.decayDelay) {
            m_data.weight[i] -= fp.weightDecayRate * dt;
            if (m_data.weight[i] < fp.minWeight) {
                m_data.weight[i] = fp.minWeight;
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
    if (fp.useSexColors) {
        if (sex == 0) { r = fp.maleColorR; g = fp.maleColorG; b = fp.maleColorB; return; }
        else          { r = fp.femaleColorR; g = fp.femaleColorG; b = fp.femaleColorB; return; }
    }
    r = m_flockColorR[fid];
    g = m_flockColorG[fid];
    b = m_flockColorB[fid];
}

// ---- Plant + reproduction logic identical to before, adapted for vector access ----

void Simulation::updatePlants(float dt)
{
    auto& pp = m_plantParams;
    float eatRangeSq = pp.eatRange * pp.eatRange;
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

    // Grazing (using plant spatial hash)
    for (int i = 0; i < m_data.count; ++i) {
        if (m_data.hunger[i] >= 0.5f) continue;

        float px = m_data.posX[i];
        float py = m_data.posY[i];
        int bestPlant = -1;
        float bestDistSq = eatRangeSq;

        m_plantNeighbors.clear();
        m_plantGrid.queryNeighbors(px, py, pp.eatRange, m_plants, m_plantNeighbors);
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
            m_data.hunger[i] = 1.0f;
            m_plants.growth[bestPlant] = 0.0f;
            m_plants.regrowTimer[bestPlant] = pp.growthTime;
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

void Simulation::updateReproduction(float /*dt*/)
{
    if (m_data.count >= m_maxBoids) return;

    std::uniform_int_distribution<int> coinFlip(0, 1);

    int nFlock = static_cast<int>(m_flockParams.size());
    for (int f = 0; f < nFlock; ++f) {
        const FlockParams& fp = m_flockParams[f];

        if (m_simTime - m_lastReproductionTime[f] < fp.reproductionInterval) continue;
        m_lastReproductionTime[f] = m_simTime;

        int males = 0, females = 0;
        for (int i = 0; i < m_data.count; ++i) {
            if (m_data.flockId[i] != f) continue;
            if (m_data.hunger[i] < fp.reproductionMinHunger) continue;
            if (m_data.age[i] < fp.adultAge * 0.25f) continue;
            if (m_data.sex[i] == 0) ++males;
            else                    ++females;
        }
        int pairs = std::min(males, females);
        if (pairs == 0) continue;

        int flockSize = countInFlock(f);
        if (flockSize >= fp.maxFlockSize) continue;

        float capScale = 1.0f;
        float softCap = fp.maxFlockSize * 0.9f;
        if (flockSize > softCap)
            capScale = static_cast<float>(fp.maxFlockSize - flockSize)
                     / static_cast<float>(fp.maxFlockSize - static_cast<int>(softCap));

        int globalRoom = m_maxBoids - m_data.count;
        if (globalRoom <= 0) continue;  // use continue to let subsequent flocks also check

        const float* fc = flockColor(f);
        std::uniform_real_distribution<float> offsetDist(-fp.separationRadius, fp.separationRadius);
        std::uniform_real_distribution<float> velDist(-80.0f, 80.0f);

        float spawnX = 0.0f, spawnY = 0.0f;
        bool foundParent = false;
        for (int i = 0; i < m_data.count; ++i) {
            if (m_data.flockId[i] == f && m_data.hunger[i] >= fp.reproductionMinHunger) {
                spawnX = m_data.posX[i];
                spawnY = m_data.posY[i];
                foundParent = true;
                break;
            }
        }
        if (!foundParent) continue;

        int spawned = 0;
        int maxNew = std::min(globalRoom, static_cast<int>(pairs * fp.reproductionMaxOffspring));
        maxNew = std::min(maxNew, fp.maxFlockSize - flockSize);
        maxNew = static_cast<int>(maxNew * capScale);
        if (maxNew <= 0) continue;

        int maxOffspringInt = static_cast<int>(fp.reproductionMaxOffspring);
        int minOffspringInt = static_cast<int>(fp.reproductionMinOffspring);
        for (int p = 0; p < pairs && spawned < maxNew; ++p) {
            int babies = minOffspringInt +
                         rand() % (maxOffspringInt - minOffspringInt + 1);
            babies = std::min(babies, maxNew - spawned);

            for (int b = 0; b < babies; ++b) {
                float nx = spawnX + offsetDist(m_rng);
                float ny = spawnY + offsetDist(m_rng);
                wrapPosition(nx, ny);
                uint8_t childSex = coinFlip(m_rng);
                m_data.add(nx, ny, velDist(m_rng), velDist(m_rng),
                           f, fc[0], fc[1], fc[2], childSex);
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
    int flockCap = m_flockParams[m_activeFlock].maxFlockSize;
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
    int flockCap = m_flockParams[m_activeFlock].maxFlockSize;
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
        fp.maxFlockSize = cap;
    m_params.maxFlockSize = cap;  // keep editor copy in sync
}

std::vector<std::string> Simulation::flockSpriteNames() const
{
    std::vector<std::string> names;
    names.reserve(m_flockParams.size());
    for (const auto& fp : m_flockParams)
        names.push_back(fp.spriteName);
    return names;
}

std::vector<bool> Simulation::flockUprightFlags() const
{
    std::vector<bool> flags;
    flags.reserve(m_flockParams.size());
    for (const auto& fp : m_flockParams)
        flags.push_back(fp.uprightSprite);
    return flags;
}

void Simulation::removeBoidAt(int index)
{
    if (index < 0 || index >= m_data.count) return;
    m_data.removeAt(index);
}

void Simulation::setTarget(float x, float y) { m_targetX = x; m_targetY = y; m_hasTarget = true; }
void Simulation::clearTarget() { m_hasTarget = false; }

void Simulation::updateGrid()
{
    float maxCohRad = m_params.cohesionRadius;
    for (auto& fp : m_flockParams)
        maxCohRad = std::max(maxCohRad, fp.cohesionRadius);
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
