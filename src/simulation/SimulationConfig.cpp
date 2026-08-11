#include "Simulation.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// ---- Config save/load ----

// Helper: serialize a FlockParams to QJsonObject
static QJsonObject flockParamsToJson(const FlockParams& fp) {
    QJsonObject o;
    o["separationRadius"]     = fp.perception.separationRadius;
    o["alignmentRadius"]      = fp.perception.alignmentRadius;
    o["cohesionRadius"]       = fp.perception.cohesionRadius;
    o["separationWeight"]     = fp.perception.separationWeight;
    o["alignmentWeight"]      = fp.perception.alignmentWeight;
    o["cohesionWeight"]       = fp.perception.cohesionWeight;
    o["boundaryWeight"]       = fp.boundary.boundaryWeight;
    o["boundaryMargin"]       = fp.boundary.boundaryMargin;
    o["wanderWeight"]         = fp.boundary.wanderWeight;
    o["targetWeight"]         = fp.boundary.targetWeight;
    o["interFlockRepulsionWeight"] = fp.interFlock.interFlockRepulsionWeight;
    o["predatorAttractionWeight"]  = fp.interFlock.predatorAttractionWeight;
    o["preyFearWeight"]            = fp.interFlock.preyFearWeight;
    o["hungerDecayRate"]     = fp.hunger.hungerDecayRate;
    o["hungerSpeedMin"]      = fp.hunger.hungerSpeedMin;
    o["hungerSpeedMax"]      = fp.hunger.hungerSpeedMax;
    o["invertHungerSpeed"]   = fp.hunger.invertHungerSpeed;
    o["weightSpeedPenalty"]  = fp.movement.weightSpeedPenalty;
    o["maxWeight"]           = fp.body.maxWeight;
    o["minWeight"]           = fp.body.minWeight;
    o["chaseSuccessBase"]    = fp.predation.chaseSuccessBase;
    o["escapeSuccessBase"]   = fp.predation.escapeSuccessBase;
    o["predationMinHunger"]  = fp.predation.predationMinHunger;
    o["predationKillHunger"] = fp.predation.predationKillHunger;
    o["predationParticipationRate"] = fp.predation.predationParticipationRate;
    o["weightGainPerKill"]   = fp.body.weightGainPerKill;
    o["weightDecayRate"]     = fp.body.weightDecayRate;
    o["streakTimeout"]       = fp.body.streakTimeout;
    o["decayDelay"]          = fp.body.decayDelay;
    o["chaseRange"]          = fp.predation.chaseRange;
    o["hungerFlashThreshold"] = fp.hunger.hungerFlashThreshold;
    o["reproductionMinOffspring"] = fp.reproduction.reproductionMinOffspring;
    o["reproductionMaxOffspring"] = fp.reproduction.reproductionMaxOffspring;
    o["maxFlockSize"]        = fp.reproduction.maxFlockSize;
    o["reproductionMinHunger"] = fp.reproduction.reproductionMinHunger;
    o["reproductionInterval"]  = fp.reproduction.reproductionInterval;
    o["adultAge"]            = fp.reproduction.adultAge;
    o["forageRange"]         = fp.hunger.forageRange;
    o["forageWeight"]        = fp.hunger.forageWeight;
    o["forageHungerThreshold"] = fp.hunger.forageHungerThreshold;
    o["maxSpeed"]            = fp.movement.maxSpeed;
    o["maxForce"]            = fp.movement.maxForce;
    o["boidSize"]            = fp.movement.boidSize;
    o["useSexColors"]        = fp.appearance.useSexColors;
    o["maleColorR"] = fp.appearance.maleColorR; o["maleColorG"] = fp.appearance.maleColorG; o["maleColorB"] = fp.appearance.maleColorB;
    o["femaleColorR"] = fp.appearance.femaleColorR; o["femaleColorG"] = fp.appearance.femaleColorG; o["femaleColorB"] = fp.appearance.femaleColorB;
    o["hardCollisionRadius"] = fp.movement.hardCollisionRadius;
    o["uprightSprite"]       = fp.appearance.uprightSprite;
    o["spriteName"]          = QString::fromStdString(fp.appearance.spriteName);

    // Age system (Phase 1.1)
    o["juvenileAge"]     = fp.age.juvenileAge;
    o["youngAge"]        = fp.age.youngAge;
    o["elderAge"]        = fp.age.elderAge;
    o["maxLifespan"]     = fp.age.maxLifespan;
    o["ageSpeedJuvenile"] = fp.age.ageSpeedJuvenile;
    o["ageSpeedYoung"]    = fp.age.ageSpeedYoung;
    o["ageSpeedAdult"]    = fp.age.ageSpeedAdult;
    o["ageSpeedElder"]    = fp.age.ageSpeedElder;
    o["ageSizeJuvenile"]  = fp.age.ageSizeJuvenile;
    o["ageSizeYoung"]     = fp.age.ageSizeYoung;
    o["ageSizeAdult"]     = fp.age.ageSizeAdult;
    o["ageSizeElder"]     = fp.age.ageSizeElder;

    // Fatigue (Phase 1.3)
    o["fatigueAccumRate"]    = fp.fatigue.fatigueAccumRate;
    o["fatigueRecoveryRate"] = fp.fatigue.fatigueRecoveryRate;
    o["fatigueSpeedPenalty"] = fp.fatigue.fatigueSpeedPenalty;

    // Gender dimorphism (Phase 1.4)
    o["sexSpeedMale"]   = fp.gender.sexSpeedMale;
    o["sexSpeedFemale"] = fp.gender.sexSpeedFemale;
    o["sexSizeMale"]    = fp.gender.sexSizeMale;
    o["sexSizeFemale"]  = fp.gender.sexSizeFemale;

    // Pregnancy (Phase 1.5)
    o["pregnancyDuration"]    = fp.pregnancy.pregnancyDuration;
    o["postpartumRecovery"]   = fp.pregnancy.postpartumRecovery;
    o["offspringHungerBoost"] = fp.pregnancy.offspringHungerBoost;

    // Combat (Phase 2.1)
    o["combatRadius"]       = fp.combat.combatRadius;
    o["combatProbability"]  = fp.combat.combatProbability;
    o["combatFatigueGain"]  = fp.combat.combatFatigueGain;
    o["combatCooldown"]     = fp.combat.combatCooldown;

    // Hatred (Phase 2.2)
    o["hatredGainPerKill"]      = fp.hatred.hatredGainPerKill;
    o["hatredDecayRate"]        = fp.hatred.hatredDecayRate;
    o["hatredFleeRadiusBoost"]  = fp.hatred.hatredFleeRadiusBoost;
    o["hatredFleeWeightBoost"]  = fp.hatred.hatredFleeWeightBoost;

    // Health / Combat (Phase 1.7)
    o["dodgeChanceBase"] = fp.health.dodgeChanceBase;
    o["damageToHealth"]  = fp.health.damageToHealth;
    o["healthRegenRate"] = fp.health.healthRegenRate;
    o["healthInitial"]   = fp.health.healthInitial;

    // Escape (Phase 2.3)
    o["escapeStrategy"]   = fp.escape.escapeStrategy;
    o["escapeStrategyMix"]= fp.escape.escapeStrategyMix;
    o["escapeZigzagAmp"]  = fp.escape.escapeZigzagAmp;

    // Defense (Phase 2.4)
    o["defenseRadius"]          = fp.defense.defenseRadius;
    o["defenseResponseWeight"]  = fp.defense.defenseResponseWeight;
    o["defenseGroupThreshold"]  = fp.defense.defenseGroupThreshold;

    // Cohesion Dynamics (Phase 2.5)
    o["cohesionBaseWeight"]  = fp.cohesionDyn.cohesionBaseWeight;
    o["cohesionThreatBoost"] = fp.cohesionDyn.cohesionThreatBoost;
    o["cohesionHungerDecay"] = fp.cohesionDyn.cohesionHungerDecay;
    o["cohesionDensityDecay"] = fp.cohesionDyn.cohesionDensityDecay;

    // Nest Pref (Phase 3.1)
    o["nestReturnWeight"]      = fp.nestPref.nestReturnWeight;
    o["nestPreferFoodDensity"] = fp.nestPref.nestPreferFoodDensity;
    o["nestPreferSafety"]      = fp.nestPref.nestPreferSafety;
    o["nestSelectionRange"]    = fp.nestPref.nestSelectionRange;

    return o;
}

// Helper: deserialize a QJsonObject back to FlockParams
static void flockParamsFromJson(const QJsonObject& o, FlockParams& fp) {
    auto rf = [&](const char* key, float& f) { if (o.contains(key)) f = static_cast<float>(o[key].toDouble()); };
    auto rb = [&](const char* key, bool& b)  { if (o.contains(key)) b = o[key].toBool(); };
    auto ri = [](const char* key, const QJsonObject& o, int def) -> int {
        if (o.contains(key)) return o[key].toInt(); return def;
    };
    rf("separationRadius",     fp.perception.separationRadius);
    rf("alignmentRadius",      fp.perception.alignmentRadius);
    rf("cohesionRadius",       fp.perception.cohesionRadius);
    rf("separationWeight",     fp.perception.separationWeight);
    rf("alignmentWeight",      fp.perception.alignmentWeight);
    rf("cohesionWeight",       fp.perception.cohesionWeight);
    rf("boundaryWeight",       fp.boundary.boundaryWeight);
    rf("boundaryMargin",       fp.boundary.boundaryMargin);
    rf("wanderWeight",         fp.boundary.wanderWeight);
    rf("targetWeight",         fp.boundary.targetWeight);
    rf("interFlockRepulsionWeight", fp.interFlock.interFlockRepulsionWeight);
    rf("predatorAttractionWeight",  fp.interFlock.predatorAttractionWeight);
    rf("preyFearWeight",            fp.interFlock.preyFearWeight);
    rf("hungerDecayRate",     fp.hunger.hungerDecayRate);
    rf("hungerSpeedMin",      fp.hunger.hungerSpeedMin);
    rf("hungerSpeedMax",      fp.hunger.hungerSpeedMax);
    rb("invertHungerSpeed",   fp.hunger.invertHungerSpeed);
    rf("weightSpeedPenalty",  fp.movement.weightSpeedPenalty);
    rf("maxWeight",           fp.body.maxWeight);
    rf("minWeight",           fp.body.minWeight);
    rf("chaseSuccessBase",    fp.predation.chaseSuccessBase);
    rf("escapeSuccessBase",   fp.predation.escapeSuccessBase);
    rf("predationMinHunger",  fp.predation.predationMinHunger);
    rf("predationKillHunger", fp.predation.predationKillHunger);
    rf("predationParticipationRate", fp.predation.predationParticipationRate);
    rf("weightGainPerKill",   fp.body.weightGainPerKill);
    rf("weightDecayRate",     fp.body.weightDecayRate);
    rf("streakTimeout",       fp.body.streakTimeout);
    rf("decayDelay",          fp.body.decayDelay);
    rf("chaseRange",          fp.predation.chaseRange);
    rf("hungerFlashThreshold", fp.hunger.hungerFlashThreshold);
    rf("reproductionMinOffspring", fp.reproduction.reproductionMinOffspring);
    rf("reproductionMaxOffspring", fp.reproduction.reproductionMaxOffspring);
    fp.reproduction.maxFlockSize = ri("maxFlockSize", o, fp.reproduction.maxFlockSize);
    rf("reproductionMinHunger", fp.reproduction.reproductionMinHunger);
    rf("reproductionInterval",  fp.reproduction.reproductionInterval);
    rf("adultAge",            fp.reproduction.adultAge);
    rf("forageRange",         fp.hunger.forageRange);
    rf("forageWeight",        fp.hunger.forageWeight);
    rf("forageHungerThreshold", fp.hunger.forageHungerThreshold);
    rf("maxSpeed",            fp.movement.maxSpeed);
    rf("maxForce",            fp.movement.maxForce);
    rf("boidSize",            fp.movement.boidSize);
    rb("useSexColors",        fp.appearance.useSexColors);
    rf("maleColorR", fp.appearance.maleColorR); rf("maleColorG", fp.appearance.maleColorG); rf("maleColorB", fp.appearance.maleColorB);
    rf("femaleColorR", fp.appearance.femaleColorR); rf("femaleColorG", fp.appearance.femaleColorG); rf("femaleColorB", fp.appearance.femaleColorB);
    rf("hardCollisionRadius", fp.movement.hardCollisionRadius);
    rb("uprightSprite",       fp.appearance.uprightSprite);
    if (o.contains("spriteName")) fp.appearance.spriteName = o["spriteName"].toString().toStdString();

    // Age system (Phase 1.1)
    rf("juvenileAge",     fp.age.juvenileAge);
    rf("youngAge",        fp.age.youngAge);
    rf("elderAge",        fp.age.elderAge);
    rf("maxLifespan",     fp.age.maxLifespan);
    rf("ageSpeedJuvenile", fp.age.ageSpeedJuvenile);
    rf("ageSpeedYoung",    fp.age.ageSpeedYoung);
    rf("ageSpeedAdult",    fp.age.ageSpeedAdult);
    rf("ageSpeedElder",    fp.age.ageSpeedElder);
    rf("ageSizeJuvenile",  fp.age.ageSizeJuvenile);
    rf("ageSizeYoung",     fp.age.ageSizeYoung);
    rf("ageSizeAdult",     fp.age.ageSizeAdult);
    rf("ageSizeElder",     fp.age.ageSizeElder);

    // Fatigue (Phase 1.3)
    rf("fatigueAccumRate",    fp.fatigue.fatigueAccumRate);
    rf("fatigueRecoveryRate", fp.fatigue.fatigueRecoveryRate);
    rf("fatigueSpeedPenalty", fp.fatigue.fatigueSpeedPenalty);

    // Gender dimorphism (Phase 1.4)
    rf("sexSpeedMale",   fp.gender.sexSpeedMale);
    rf("sexSpeedFemale", fp.gender.sexSpeedFemale);
    rf("sexSizeMale",    fp.gender.sexSizeMale);
    rf("sexSizeFemale",  fp.gender.sexSizeFemale);

    // Pregnancy (Phase 1.5)
    rf("pregnancyDuration",    fp.pregnancy.pregnancyDuration);
    rf("postpartumRecovery",   fp.pregnancy.postpartumRecovery);
    rf("offspringHungerBoost", fp.pregnancy.offspringHungerBoost);

    // Combat (Phase 2.1)
    rf("combatRadius",       fp.combat.combatRadius);
    rf("combatProbability",  fp.combat.combatProbability);
    rf("combatFatigueGain",  fp.combat.combatFatigueGain);
    rf("combatCooldown",     fp.combat.combatCooldown);

    // Hatred (Phase 2.2)
    rf("hatredGainPerKill",      fp.hatred.hatredGainPerKill);
    rf("hatredDecayRate",        fp.hatred.hatredDecayRate);
    rf("hatredFleeRadiusBoost",  fp.hatred.hatredFleeRadiusBoost);
    rf("hatredFleeWeightBoost",  fp.hatred.hatredFleeWeightBoost);

    // Escape (Phase 2.3)
    rf("escapeStrategy",   fp.escape.escapeStrategy);
    rf("escapeStrategyMix",fp.escape.escapeStrategyMix);
    rf("escapeZigzagAmp",  fp.escape.escapeZigzagAmp);

    // Defense (Phase 2.4)
    rf("defenseRadius",          fp.defense.defenseRadius);
    rf("defenseResponseWeight",  fp.defense.defenseResponseWeight);
    rf("defenseGroupThreshold",  fp.defense.defenseGroupThreshold);

    // Cohesion Dynamics (Phase 2.5)
    rf("cohesionBaseWeight",  fp.cohesionDyn.cohesionBaseWeight);
    rf("cohesionThreatBoost", fp.cohesionDyn.cohesionThreatBoost);
    rf("cohesionHungerDecay", fp.cohesionDyn.cohesionHungerDecay);
    rf("cohesionDensityDecay",fp.cohesionDyn.cohesionDensityDecay);

    // Health / Combat (Phase 1.7)
    rf("dodgeChanceBase", fp.health.dodgeChanceBase);
    rf("damageToHealth",  fp.health.damageToHealth);
    rf("healthRegenRate", fp.health.healthRegenRate);
    rf("healthInitial",   fp.health.healthInitial);

    // Nest Pref (Phase 3.1)
    rf("nestReturnWeight",      fp.nestPref.nestReturnWeight);
    rf("nestPreferFoodDensity", fp.nestPref.nestPreferFoodDensity);
    rf("nestPreferSafety",      fp.nestPref.nestPreferSafety);
    rf("nestSelectionRange",    fp.nestPref.nestSelectionRange);
}

bool Simulation::saveConfig(const char* path) const
{
    QJsonObject root;
    root["version"] = 1;

    // World
    QJsonObject world;
    world["width"]  = static_cast<double>(m_worldW);
    world["height"] = static_cast<double>(m_worldH);
    root["world"] = world;

    // Global
    QJsonObject glob;
    glob["boundaryMode"]       = static_cast<int>(m_globalParams.boundaryMode);
    glob["hungerFlashEnabled"] = m_globalParams.hungerFlashEnabled;
    root["global"] = glob;

    // Plants
    QJsonObject plant;
    plant["maxPlants"]     = static_cast<int>(m_plantParams.maxPlants);
    plant["initialPlants"] = static_cast<int>(m_plantParams.initialPlants);
    plant["eatRange"]      = m_plantParams.eatRange;
    plant["plantFoodValue"] = m_plantParams.plantFoodValue;
    plant["growthTime"]    = m_plantParams.growthTime;
    plant["spreadChance"]  = m_plantParams.spreadChance;
    plant["spreadRange"]   = m_plantParams.spreadRange;
    plant["seasonLength"]  = m_plantParams.seasonLength;
    plant["fertilizeRadius"] = m_plantParams.fertilizeRadius;
    plant["fertilizeBoost"]  = m_plantParams.fertilizeBoost;
    plant["carryingPressure"]= m_plantParams.carryingPressure;
    root["plants"] = plant;

    // Nests (Phase 3.1)
    QJsonObject nest;
    nest["initialNests"]       = static_cast<int>(m_nestParams.initialNests);
    nest["nestRadius"]         = m_nestParams.nestRadius;
    nest["nestHealthBoost"]    = m_nestParams.nestHealthBoost;
    nest["nestFoodStorageRate"]= m_nestParams.nestFoodStorageRate;
    nest["contestDuration"]    = m_nestParams.contestDuration;
    nest["defenseThreshold"]   = m_nestParams.defenseThreshold;
    root["nests"] = nest;

    // Flocks
    QJsonArray flocks;
    int n = static_cast<int>(m_flockParams.size());
    for (int i = 0; i < n; ++i) {
        QJsonObject f;
        f["name"]   = QString::fromStdString(m_flockNames[i]);
        QJsonArray col;
        col.append(static_cast<double>(m_flockColorR[i]));
        col.append(static_cast<double>(m_flockColorG[i]));
        col.append(static_cast<double>(m_flockColorB[i]));
        f["color"]  = col;
        f["params"] = flockParamsToJson(m_flockParams[i]);

        // Relationships (this flock's view of others)
        QJsonArray rels;
        for (int j = 0; j < n; ++j)
            rels.append(static_cast<int>(m_relationships[i][j]));
        f["relationships"] = rels;

        flocks.append(f);
    }
    root["flocks"] = flocks;

    QFile file(QString::fromUtf8(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool Simulation::loadConfig(const char* path)
{
    QFile file(QString::fromUtf8(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return false;

    QJsonObject root = doc.object();
    // Version check (currently only v1)
    int ver = root["version"].toInt(1);
    if (ver < 1) return false;

    // World: resize without re-allocating boids
    QJsonObject world = root["world"].toObject();
    if (!world.isEmpty()) {
        m_worldW = static_cast<float>(world["width"].toDouble(1920.0));
        m_worldH = static_cast<float>(world["height"].toDouble(1080.0));
    }

    // Global
    QJsonObject glob = root["global"].toObject();
    if (!glob.isEmpty()) {
        m_globalParams.boundaryMode       = static_cast<BoundaryMode>(glob["boundaryMode"].toInt(3));
        m_globalParams.hungerFlashEnabled = glob["hungerFlashEnabled"].toBool(true);
    }

    // Plants
    QJsonObject plant = root["plants"].toObject();
    if (!plant.isEmpty()) {
        m_plantParams.maxPlants     = static_cast<float>(plant["maxPlants"].toInt(200));
        m_plantParams.initialPlants = static_cast<float>(plant["initialPlants"].toInt(80));
        auto rpf = [&](const char* k, float& f) { if (plant.contains(k)) f = static_cast<float>(plant[k].toDouble()); };
        rpf("eatRange",      m_plantParams.eatRange);
        rpf("growthTime",    m_plantParams.growthTime);
        rpf("spreadChance",  m_plantParams.spreadChance);
        rpf("spreadRange",   m_plantParams.spreadRange);
        rpf("seasonLength",  m_plantParams.seasonLength);
        rpf("fertilizeRadius", m_plantParams.fertilizeRadius);
        rpf("fertilizeBoost",  m_plantParams.fertilizeBoost);
        rpf("plantFoodValue",  m_plantParams.plantFoodValue);
        rpf("carryingPressure",m_plantParams.carryingPressure);
    }

    // Nests (Phase 3.1)
    QJsonObject nestObj = root["nests"].toObject();
    if (!nestObj.isEmpty()) {
        auto rpf = [&](const char* key, float& f) { if (nestObj.contains(key)) f = static_cast<float>(nestObj[key].toDouble()); };
        rpf("initialNests",        m_nestParams.initialNests);
        rpf("nestRadius",          m_nestParams.nestRadius);
        rpf("nestHealthBoost",     m_nestParams.nestHealthBoost);
        rpf("nestFoodStorageRate", m_nestParams.nestFoodStorageRate);
        rpf("contestDuration",     m_nestParams.contestDuration);
        rpf("defenseThreshold",    m_nestParams.defenseThreshold);
    }

    // Flocks: clear and rebuild
    QJsonArray flocks = root["flocks"].toArray();
    if (!flocks.isEmpty()) {
        m_flockParams.clear();
        m_flockNames.clear();
        m_flockColorR.clear();
        m_flockColorG.clear();
        m_flockColorB.clear();
        m_relationships.clear();
        m_lastReproductionTime.clear();

        int n = flocks.size();
        for (int i = 0; i < n; ++i) {
            QJsonObject f = flocks[i].toObject();

            m_flockNames.push_back(f["name"].toString("Flock ?").toStdString());

            QJsonArray col = f["color"].toArray();
            m_flockColorR.push_back(static_cast<float>(col[0].toDouble(0.8)));
            m_flockColorG.push_back(static_cast<float>(col[1].toDouble(0.3)));
            m_flockColorB.push_back(static_cast<float>(col[2].toDouble(0.3)));

            FlockParams fp;
            flockParamsFromJson(f["params"].toObject(), fp);
            m_flockParams.push_back(fp);
            m_lastReproductionTime.push_back(0.0f);

            // Relationships
            QJsonArray rels = f["relationships"].toArray();
            std::vector<FlockRelation> row;
            for (int j = 0; j < rels.size(); ++j)
                row.push_back(static_cast<FlockRelation>(rels[j].toInt()));
            m_relationships.push_back(row);
        }
        // Ensure all rows have the correct size
        for (auto& row : m_relationships)
            row.resize(n, FlockRelation::Neutral);
    }

    // Sync active flock
    m_activeFlock = 0;
    if (!m_flockParams.empty())
        m_params = m_flockParams[0];

    // Rebuild grids with new dimensions/params
    float maxCohRad = m_params.perception.cohesionRadius;
    for (auto& fp : m_flockParams)
        maxCohRad = std::max(maxCohRad, fp.perception.cohesionRadius);
    m_grid.reinit(m_worldW, m_worldH, maxCohRad * 1.5f, m_maxBoids);
    m_plantGrid.init(m_worldW, m_worldH, m_plantParams.eatRange, static_cast<int>(m_plantParams.maxPlants));

    return true;
}
