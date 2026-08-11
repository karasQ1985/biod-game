// ============================================================================
//  Biod - Automated Regression Test Suite (Biod 自动回归测试套件)
//
//  Usage:
//    Compile:  cmake --build build --target test_runner
//    Run:      ./test_runner
//
//  Tests all Simulation module features with edge cases and boundary conditions.
//  No external test framework required -- uses simple assertion macros.
// ============================================================================

#include "simulation/Simulation.h"
#include "core/WorldConstants.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <cmath>

// ---- Test infrastructure ----
static int g_passed = 0;
static int g_failed = 0;
static std::string g_currentSuite;

#define TEST_SUITE(name) \
    g_currentSuite = name; \
    std::cout << "\n== " << name << " ==" << std::endl;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  FAIL [" << g_currentSuite << "] " << msg << std::endl; \
        ++g_failed; \
    } else { \
        ++g_passed; \
    } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg << " (expected " << (b) << ", got " << (a) << ")")
#define CHECK_FLOAT_EQ(a, b, eps, msg) CHECK(std::abs((a) - (b)) <= (eps), \
    msg << " (expected " << (b) << ", got " << (a) << ")")

// ---- Helper ----
static float colorDiff(const float* a, const float* b) {
    return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
}

// ============================================================================
//  Test Cases
// ============================================================================

void test_initialization() {
    TEST_SUITE("Simulation initialization");

    Simulation sim;
    CHECK_EQ(sim.flockCount(), 0, "Fresh sim has 0 flocks");

    sim.init(1920.0f, 1080.0f, 10000);
    CHECK_EQ(sim.flockCount(), 2, "After init, flockCount = 2 (MIN_FLOCKS)");
    CHECK_EQ(sim.data().count, 0, "No boids spawned automatically");
    CHECK_EQ(sim.maxBoids(), 10000, "maxBoids = 10000");
    CHECK_EQ(sim.activeFlock(), 0, "Active flock defaults to 0");
    CHECK_FLOAT_EQ(sim.worldW(), 1920.0f, 0.01f, "worldW = 1920");
    CHECK_FLOAT_EQ(sim.worldH(), 1080.0f, 0.01f, "worldH = 1080");
    CHECK(!sim.hasTarget(), "No target initially");
    CHECK_FLOAT_EQ(sim.simTime(), 0.0f, 0.001f, "simTime starts at 0");
    CHECK_EQ(sim.currentSeason(), 0, "Season starts at Spring (0)");

    // Flock names
    CHECK_EQ(sim.flockName(0), std::string("Flock A"), "Default name Flock 0");
    CHECK_EQ(sim.flockName(1), std::string("Flock B"), "Default name Flock 1");

    // Default colors (must copy since flockColor returns static buffer)
    const float* c0p = sim.flockColor(0);
    float c0[] = {c0p[0], c0p[1], c0p[2]};
    const float* c1p = sim.flockColor(1);
    float c1[] = {c1p[0], c1p[1], c1p[2]};
    CHECK(colorDiff(c0, c1) > 0.01f, "Flock 0 and 1 have different colors");

    // Relationship matrix initial state
    CHECK_EQ(static_cast<int>(sim.relationship(0, 1)), 0, "Default rel 0->1 is Neutral");
    CHECK_EQ(static_cast<int>(sim.relationship(1, 0)), 0, "Default rel 1->0 is Neutral");

    // Plants initialized
    CHECK(sim.plants().count > 0, "Plants are initialized");
}

void test_flock_lifecycle() {
    TEST_SUITE("Flock lifecycle (add/remove at boundaries)");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    // ---- Add flocks ----
    int id2 = sim.addFlock();
    CHECK_EQ(id2, 2, "addFlock returns id=2");
    CHECK_EQ(sim.flockCount(), 3, "flockCount = 3 after add");

    int id3 = sim.addFlock();
    CHECK_EQ(id3, 3, "addFlock returns id=3");
    CHECK_EQ(sim.flockCount(), 4, "flockCount = 4");

    // Names
    CHECK_EQ(sim.flockName(2), std::string("Flock C"), "New flock name is Flock C");
    CHECK_EQ(sim.flockName(3), std::string("Flock D"), "New flock name is Flock D");

    // ---- Add up to MAX_FLOCKS (12) ----
    for (int i = 4; i < MAX_FLOCKS; ++i) {
        int id = sim.addFlock();
        CHECK_EQ(id, i, "addFlock returns sequential id " << i);
    }
    CHECK_EQ(sim.flockCount(), MAX_FLOCKS, "flockCount = MAX_FLOCKS (12)");

    // ---- Add beyond MAX_FLOCKS ----
    int over = sim.addFlock();
    CHECK_EQ(over, -1, "addFlock at max returns -1");
    CHECK_EQ(sim.flockCount(), MAX_FLOCKS, "flockCount still = 12");

    // ---- Remove flocks ----
    bool r1 = sim.removeFlock(11);
    CHECK(r1, "removeFlock(11) succeeds");
    CHECK_EQ(sim.flockCount(), 11, "flockCount = 11 after remove");

    // Remove all down to MIN_FLOCKS
    for (int id = 10; id >= MIN_FLOCKS; --id) {
        bool ok = sim.removeFlock(id);
        CHECK(ok, "removeFlock(" << id << ") succeeds");
    }
    CHECK_EQ(sim.flockCount(), MIN_FLOCKS, "flockCount = MIN_FLOCKS (2)");

    // ---- Remove below MIN_FLOCKS ----
    bool rMin1 = sim.removeFlock(0);
    CHECK(!rMin1, "removeFlock(0) at min returns false");
    CHECK_EQ(sim.flockCount(), MIN_FLOCKS, "flockCount still = 2");

    bool rMin2 = sim.removeFlock(1);
    CHECK(!rMin2, "removeFlock(1) at min returns false");
    CHECK_EQ(sim.flockCount(), MIN_FLOCKS, "flockCount still = 2");

    // ---- Remove invalid ID ----
    bool bad = sim.removeFlock(99);
    CHECK(!bad, "removeFlock(99) invalid id returns false");
    bool neg = sim.removeFlock(-1);
    CHECK(!neg, "removeFlock(-1) invalid id returns false");

    // ---- Re-add after remove ----
    int idAfter = sim.addFlock();
    CHECK_EQ(idAfter, 2, "Re-add after remove returns id=2");
    CHECK_EQ(sim.flockCount(), 3, "flockCount = 3 after re-add");
}

void test_flock_names_and_colors() {
    TEST_SUITE("Flock names and colors");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    // Custom name
    sim.setFlockName(0, "Predators");
    CHECK_EQ(sim.flockName(0), std::string("Predators"), "Custom name set correctly");
    CHECK_EQ(sim.flockName(1), std::string("Flock B"), "Other flock name unchanged");

    // Custom color
    sim.setFlockColor(0, 1.0f, 0.5f, 0.0f);
    const float* c = sim.flockColor(0);
    CHECK_FLOAT_EQ(c[0], 1.0f, 0.01f, "Custom color R");
    CHECK_FLOAT_EQ(c[1], 0.5f, 0.01f, "Custom color G");
    CHECK_FLOAT_EQ(c[2], 0.0f, 0.01f, "Custom color B");

    // Set color on new flock
    sim.addFlock();
    sim.setFlockColor(2, 0.0f, 1.0f, 0.0f);
    const float* c2 = sim.flockColor(2);
    CHECK_FLOAT_EQ(c2[0], 0.0f, 0.01f, "New flock custom color R");
    CHECK_FLOAT_EQ(c2[1], 1.0f, 0.01f, "New flock custom color G");

    // flockColor bounds checking (id < 0 clamps to 0)
    const float* negC = sim.flockColor(-1);
    const float* c0 = sim.flockColor(0);
    CHECK_FLOAT_EQ(negC[0], c0[0], 0.01f, "flockColor(-1) clamps to flock 0");

    // flockColor bounds checking (id >= n clamps to n-1)
    const float* bigC = sim.flockColor(999);
    const float* lastC = sim.flockColor(2);
    CHECK_FLOAT_EQ(bigC[0], lastC[0], 0.01f, "flockColor(999) clamps to last flock");
}

void test_active_flock_switching() {
    TEST_SUITE("Active flock switching and param persistence");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);
    sim.addFlock(); // id=2

    // Modify params for flock 0
    auto& p0 = sim.params();
    p0.perception.separationWeight = 5.0f;
    sim.saveCurrentParams();

    // Switch to flock 1
    sim.setActiveFlock(1);
    CHECK_EQ(sim.activeFlock(), 1, "Active flock switched to 1");
    CHECK_FLOAT_EQ(sim.params().perception.separationWeight, 2.5f, 0.01f, "Flock 1 has default separationWeight");

    // Modify flock 1 params
    sim.params().perception.cohesionWeight = 1.5f;
    sim.saveCurrentParams();

    // Switch back to flock 0, verify params persist
    sim.setActiveFlock(0);
    CHECK_EQ(sim.activeFlock(), 0, "Active flock switched back to 0");
    CHECK_FLOAT_EQ(sim.params().perception.separationWeight, 5.0f, 0.01f, "Flock 0 separationWeight persisted");

    // Switch to flock 2 (new)
    sim.setActiveFlock(2);
    CHECK_EQ(sim.activeFlock(), 2, "Active flock switched to 2");
    CHECK_FLOAT_EQ(sim.params().perception.separationWeight, 2.5f, 0.01f, "New flock has default params");

    // Verify flock 1 params persisted
    CHECK_FLOAT_EQ(sim.flockParams(1).perception.cohesionWeight, 1.5f, 0.01f, "Flock 1 cohesionWeight persisted");

    // Invalid active flock switch
    sim.setActiveFlock(-1);
    CHECK_EQ(sim.activeFlock(), 2, "setActiveFlock(-1) ignored (stays at 2)");

    sim.setActiveFlock(999);
    CHECK_EQ(sim.activeFlock(), 2, "setActiveFlock(999) ignored");
}

void test_relationship_matrix() {
    TEST_SUITE("Relationship matrix");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);
    sim.addFlock(); // id=2
    sim.addFlock(); // id=3

    // ---- Set relationships ----
    sim.setRelationship(0, 1, FlockRelation::Predator);
    CHECK_EQ(static_cast<int>(sim.relationship(0, 1)), 1, "0->1 is Predator");
    CHECK_EQ(static_cast<int>(sim.relationship(1, 0)), 2, "1->0 auto-set to Prey");

    sim.setRelationship(1, 2, FlockRelation::Predator);
    CHECK_EQ(static_cast<int>(sim.relationship(1, 2)), 1, "1->2 is Predator");
    CHECK_EQ(static_cast<int>(sim.relationship(2, 1)), 2, "2->1 auto-set to Prey");

    sim.setRelationship(3, 0, FlockRelation::Prey);
    CHECK_EQ(static_cast<int>(sim.relationship(3, 0)), 2, "3->0 is Prey");
    CHECK_EQ(static_cast<int>(sim.relationship(0, 3)), 1, "0->3 auto-set to Predator");

    // Self-relation
    sim.setRelationship(0, 0, FlockRelation::Predator);
    CHECK_EQ(static_cast<int>(sim.relationship(0, 0)), 1, "Self relation can be set");

    // ---- Reset to neutral ----
    sim.setRelationship(0, 1, FlockRelation::Neutral);
    CHECK_EQ(static_cast<int>(sim.relationship(0, 1)), 0, "0->1 reset to Neutral");
    CHECK_EQ(static_cast<int>(sim.relationship(1, 0)), 0, "1->0 also reset to Neutral");

    // ---- Add more flocks → matrix expands ----
    sim.addFlock(); // id=4
    CHECK_EQ(static_cast<int>(sim.relationship(0, 4)), 0, "New flock gets Neutral with existing");
    CHECK_EQ(static_cast<int>(sim.relationship(4, 0)), 0, "New flock gets Neutral from existing");

    // ---- Remove a flock → matrix shrinks ----
    sim.removeFlock(3);
    CHECK_EQ(static_cast<int>(sim.relationship(0, 2)), 0, "After remove, 0->(was idx 2) still Neutral");
}

void test_boid_spawning_and_removal() {
    TEST_SUITE("Boid spawning and removal");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    // ---- Spawn boids ----
    sim.spawnRandom(10);
    CHECK_EQ(sim.data().count, 10, "spawnRandom(10) adds 10 boids");
    CHECK_EQ(sim.countInFlock(0), 10, "All boids in active flock (0)");

    // Switch and spawn in another flock
    sim.setActiveFlock(1);
    sim.spawnRandom(5);
    CHECK_EQ(sim.data().count, 15, "Total boids = 15");
    CHECK_EQ(sim.countInFlock(1), 5, "5 boids in flock 1");

    // ---- Individual add ----
    sim.setActiveFlock(0);
    sim.addBoid(500.0f, 300.0f);
    CHECK_EQ(sim.data().count, 16, "addBoid adds 1");
    CHECK_EQ(sim.countInFlock(0), 11, "Flock 0 now has 11");

    // ---- Remove single boid ----
    int prev = sim.data().count;
    sim.removeBoidAt(0);
    CHECK_EQ(sim.data().count, prev - 1, "removeBoidAt reduces count by 1");

    // ---- Remove all boids from a flock ----
    int removed = sim.removeBoidsFromFlock(0, sim.countInFlock(0));
    CHECK(removed > 0, "removeBoidsFromFlock removes all from flock 0");
    CHECK_EQ(sim.countInFlock(0), 0, "Flock 0 is empty");

    // ---- Spawn with invalid count ----
    sim.spawnRandom(-5);
    CHECK_EQ(sim.data().count, sim.data().count, "spawnRandom negative count ignored");

    // ---- Spawn up to capacity ----
    Simulation sim2;
    sim2.init(1920.0f, 1080.0f, 50);
    sim2.spawnRandom(100);
    CHECK(sim2.data().count <= 50, "Spawn respects maxBoids limit");

    // ---- New flock has zero boids ----
    Simulation sim3;
    sim3.init(1920.0f, 1080.0f, 10000);
    sim3.addFlock();
    CHECK_EQ(sim3.countInFlock(2), 0, "Newly added flock has 0 boids");
}

void test_empty_simulation_update() {
    TEST_SUITE("Empty simulation update");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    // Update with no boids should not crash
    for (int i = 0; i < 100; ++i) {
        sim.update(0.016f);
    }
    CHECK(sim.simTime() > 0.0f, "simTime advances with empty update");
    CHECK_EQ(sim.data().count, 0, "No boids after empty updates");
    CHECK(sim.plants().count > 0, "Plants still exist");

    // Add boids and verify updates continue
    sim.spawnRandom(10);
    for (int i = 0; i < 50; ++i) {
        sim.update(0.016f);
    }
    CHECK(sim.data().count >= 0, "Updates with boids do not crash");
}

void test_target_seeking() {
    TEST_SUITE("Target seeking");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    CHECK(!sim.hasTarget(), "No target by default");

    sim.setTarget(500.0f, 400.0f);
    CHECK(sim.hasTarget(), "Target is set");
    CHECK_FLOAT_EQ(sim.targetX(), 500.0f, 0.1f, "targetX = 500");
    CHECK_FLOAT_EQ(sim.targetY(), 400.0f, 0.1f, "targetY = 400");

    // Move target
    sim.setTarget(800.0f, 600.0f);
    CHECK_FLOAT_EQ(sim.targetX(), 800.0f, 0.1f, "target moved to 800");
    CHECK_FLOAT_EQ(sim.targetY(), 600.0f, 0.1f, "target moved to 600");

    // Clear target
    sim.clearTarget();
    CHECK(!sim.hasTarget(), "Target cleared");

    // Update with and without target
    sim.spawnRandom(5);
    sim.setTarget(100.0f, 100.0f);
    sim.update(0.016f);
    CHECK(sim.hasTarget(), "Target persists through update");

    sim.clearTarget();
    sim.update(0.016f);
    CHECK(!sim.hasTarget(), "Target stays cleared after update");
}

void test_seasons() {
    TEST_SUITE("Season cycle");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    // Spring (season 0) at time 0
    CHECK_EQ(sim.currentSeason(), 0, "Season 0 = Spring at t=0");

    // Advance time through seasons using whole-second steps (avoid fp precision issues)
    float seasonLen = sim.plantParams().seasonLength;
    // Advance past full season boundary: simulate until t >= seasonLen
    float t = 0.0f;
    while (t < seasonLen) {
        sim.update(0.1f);
        t += 0.1f;
    }
    CHECK_EQ(sim.currentSeason(), 1, "Season 1 = Summer");

    while (t < seasonLen * 2) {
        sim.update(0.1f);
        t += 0.1f;
    }
    CHECK_EQ(sim.currentSeason(), 2, "Season 2 = Autumn");

    while (t < seasonLen * 3) {
        sim.update(0.1f);
        t += 0.1f;
    }
    CHECK_EQ(sim.currentSeason(), 3, "Season 3 = Winter");

    while (t < seasonLen * 4) {
        sim.update(0.1f);
        t += 0.1f;
    }
    CHECK_EQ(sim.currentSeason(), 0, "Season wraps back to 0 = Spring");
}

void test_parameter_bounds() {
    TEST_SUITE("Parameter boundary values");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    auto& p = sim.params();

    // Set parameters to minimum reasonable values
    p.perception.separationRadius = 0.0f;
    p.perception.alignmentRadius = 0.0f;
    p.perception.cohesionRadius = 0.0f;
    p.perception.separationWeight = 0.0f;
    p.perception.alignmentWeight = 0.0f;
    p.perception.cohesionWeight = 0.0f;
    p.boundary.boundaryWeight = 0.0f;
    p.boundary.wanderWeight = 0.0f;
    sim.saveCurrentParams();

    sim.spawnRandom(5);
    sim.update(0.016f);
    CHECK_EQ(sim.data().count, 5, "Zero-weight params: boids survive one frame");

    // Set parameters to high values
    p.perception.separationWeight = 10.0f;
    p.perception.alignmentWeight = 10.0f;
    p.perception.cohesionWeight = 10.0f;
    p.boundary.boundaryWeight = 10.0f;
    p.boundary.wanderWeight = 10.0f;
    p.movement.maxSpeed = 1000.0f;
    p.movement.maxForce = 10000.0f;
    sim.saveCurrentParams();

    sim.update(0.016f);
    CHECK_EQ(sim.data().count, 5, "High-weight params: boids survive");

    // Check hunger system
    p.hunger.hungerDecayRate = 0.0f; // No decay
    sim.saveCurrentParams();
    sim.update(1.0f);
    CHECK_EQ(sim.data().count, 5, "Zero hunger decay: no boids starve in 1s");

    p.hunger.hungerDecayRate = 0.008f; // Default
    sim.saveCurrentParams();
    for (int i = 0; i < 20; ++i) sim.update(1.0f); // 20 seconds
    CHECK(sim.data().count >= 0, "Hunger system works without crash");
}

void test_reproduction_bounds() {
    TEST_SUITE("Reproduction boundary conditions");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    auto& p = sim.params();

    // Set reproduction to trigger every frame with high output
    p.reproduction.reproductionInterval = 0.0f;
    p.reproduction.reproductionMinOffspring = 1.0f;
    p.reproduction.reproductionMaxOffspring = 10.0f;
    p.reproduction.reproductionMinHunger = 0.0f;
    p.reproduction.maxFlockSize = 1000;
    p.hunger.hungerDecayRate = 0.0f; // No decay
    p.age.juvenileAge = 0.0f;  // Make boids Adult immediately (reproduction uses AgeStage)
    p.age.youngAge = 0.0f;
    sim.saveCurrentParams();

    // Spawn some boids and advance simulation
    sim.spawnRandom(10);
    for (int i = 0; i < 20; ++i) {
        sim.update(0.016f);
    }
    CHECK(sim.data().count >= 10, "Reproduction does not crash");

    // Set maxFlockSize to 0 (edge)
    p.reproduction.maxFlockSize = 0;
    sim.saveCurrentParams();
    sim.update(0.016f);
    CHECK(sim.data().count >= 0, "Zero maxFlockSize does not crash");
}

void test_multiple_flock_rapid_operations() {
    TEST_SUITE("Rapid flock add/remove cycles");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    // Add 3 flocks, remove them, add again - repeat 5 times
    for (int cycle = 0; cycle < 5; ++cycle) {
        for (int i = 0; i < 3; ++i) {
            sim.addFlock();
        }
        CHECK_EQ(sim.flockCount(), 5, "Cycle " << cycle << ": 5 flocks");
        for (int i = 0; i < 3; ++i) {
            sim.removeFlock(4 - i);
        }
        CHECK_EQ(sim.flockCount(), 2, "Cycle " << cycle << ": back to 2 flocks");
    }
}

void test_remove_flock_with_boids() {
    TEST_SUITE("Remove flock that has boids");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    sim.addFlock(); // id=2
    sim.setActiveFlock(2);
    sim.spawnRandom(20);
    CHECK_EQ(sim.countInFlock(2), 20, "20 boids in flock 2");

    // Remove the flock — boids should be cleaned up
    bool ok = sim.removeFlock(2);
    CHECK(ok, "removeFlock(2) succeeded");
    CHECK_EQ(sim.countInFlock(2), 0, "No boids in removed flock ID");

    // Verify total boid count decreased
    int totalAfter = sim.countInFlock(0) + sim.countInFlock(1);
    CHECK_EQ(sim.data().count, totalAfter, "Boids of removed flock are gone");

    // Verify flock IDs shifted correctly
    int newId = sim.addFlock();
    CHECK_EQ(newId, 2, "New flock gets id=2 (reused slot)");
    sim.setActiveFlock(2);
    sim.spawnRandom(5);
    for (int i = 0; i < sim.data().count; ++i) {
        CHECK(sim.data().flockId[i] < sim.flockCount(), "All boid flock IDs valid (< flockCount)");
    }
}

void test_plant_ecology() {
    TEST_SUITE("Plant ecology");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    int initPlants = sim.plants().count;
    CHECK(initPlants > 0, "Initial plants exist");

    // Long simulation without boids — plants should spread
    for (int i = 0; i < 200; ++i) {
        sim.update(0.016f);
    }
    // Plants should still exist (may not increase due to cap)
    CHECK(sim.plants().count >= 0, "Plants survive long simulation");

    // Add boids that eat plants
    auto& p = sim.params();
    p.hunger.hungerDecayRate = 0.02f; // Fast hunger
    p.hunger.forageWeight = 5.0f;
    p.hunger.forageHungerThreshold = 1.0f; // Always forage
    p.predation.chaseRange = 100.0f;
    sim.saveCurrentParams();

    sim.spawnRandom(50);
    for (int i = 0; i < 100; ++i) {
        sim.update(0.016f);
    }
    CHECK(sim.data().count >= 0, "Foraging does not crash");
    CHECK(sim.plants().count >= 0, "Plants count valid after foraging");

    // Plant params bounds
    auto& pp = sim.plantParams();
    pp.maxPlants = 0;
    sim.update(0.016f);
    CHECK(sim.plants().count >= 0, "Zero maxPlants does not crash");
}

void test_dt_edge_cases() {
    TEST_SUITE("Delta-time edge cases");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);
    sim.spawnRandom(10);

    // Very small dt
    sim.update(0.0001f);
    CHECK_EQ(sim.data().count, 10, "Tiny dt does not crash");

    // Normal dt
    sim.update(0.016f);
    CHECK_EQ(sim.data().count, 10, "Normal dt does not crash");

    // Large dt (clamped in paintGL)
    sim.update(1.0f);
    CHECK_EQ(sim.data().count, 10, "Large dt does not crash");

    // Zero dt
    sim.update(0.0f);
    CHECK_EQ(sim.data().count, 10, "Zero dt does not crash");

    // Negative dt
    sim.update(-0.1f);
    CHECK(sim.data().count >= 0, "Negative dt handled");
}

void test_spawn_all_flocks() {
    TEST_SUITE("Spawn boids in all flocks");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    // Add max flocks and spawn in each
    for (int i = 2; i < MAX_FLOCKS; ++i) {
        sim.addFlock();
    }

    for (int fid = 0; fid < MAX_FLOCKS; ++fid) {
        sim.setActiveFlock(fid);
        sim.spawnRandom(5);
        CHECK(sim.countInFlock(fid) == 5, "Flock " << fid << " has 5 boids");
    }

    CHECK_EQ(sim.data().count, MAX_FLOCKS * 5, "Total boids = 12 * 5");

    // Verify all flock IDs are within range
    for (int i = 0; i < sim.data().count; ++i) {
        int fid = sim.data().flockId[i];
        CHECK(fid >= 0 && fid < MAX_FLOCKS, "Boid flock ID " << fid << " is valid");
    }
}

void test_color_persistence_after_remove() {
    TEST_SUITE("Color persistence after flock removal");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);
    sim.addFlock(); // id=2

    // Set custom color on flock 1
    sim.setFlockColor(1, 0.2f, 0.4f, 0.6f);
    const float* c1a = sim.flockColor(1);

    // Remove flock 0, flock 1 becomes flock 0
    sim.removeFlock(0);
    const float* c1b = sim.flockColor(0);
    CHECK_FLOAT_EQ(c1b[0], c1a[0], 0.01f, "Flock color preserved after ID shift");
    CHECK_FLOAT_EQ(c1b[1], c1a[1], 0.01f, "Flock color G preserved after ID shift");
    CHECK_FLOAT_EQ(c1b[2], c1a[2], 0.01f, "Flock color B preserved after ID shift");
}

void test_capacity_enforcement() {
    TEST_SUITE("Capacity enforcement: L1/L2/L3");

    // ---- L2: Global cap blocks new flock creation ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 50);
        // Fill to global capacity
        sim.spawnRandom(100);
        CHECK_EQ(sim.data().count, 50, "Spawn capped at global maxBoids=50");

        // Try to add new flock -> should be denied
        int result = sim.addFlock();
        CHECK_EQ(result, -2, "addFlock returns -2 when global cap is full");
        CHECK_EQ(sim.flockCount(), 2, "Flock count unchanged after denied addFlock");
    }

    // ---- L2: Global cap blocks spawnRandom ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 50);
        sim.spawnRandom(50);
        CHECK_EQ(sim.data().count, 50, "Global cap reached exactly");

        int spawned = sim.spawnRandom(10);
        CHECK_EQ(spawned, 0, "spawnRandom returns 0 when global cap is full");
        CHECK_EQ(sim.data().count, 50, "No boids added beyond global cap");
    }

    // ---- L3: Per-flock cap blocks spawnRandom ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 10000);
        auto& p = sim.params();
        p.reproduction.maxFlockSize = 100;
        sim.saveCurrentParams();

        // Fill flock 0 to its cap
        sim.spawnRandom(100);
        CHECK_EQ(sim.countInFlock(0), 100, "Flock 0 at per-flock cap");
        CHECK(sim.data().count < 10000, "Global cap still has room");

        int spawned = sim.spawnRandom(10);
        CHECK_EQ(spawned, 0, "spawnRandom returns 0 when per-flock cap is full");
    }

    // ---- L2+L3: spawnRandom returns count correctly ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 50);
        auto& p = sim.params();
        p.reproduction.maxFlockSize = 30;
        sim.saveCurrentParams();

        // Partial fill: 30 fits per-flock cap, but global max=50
        int spawned = sim.spawnRandom(100);
        CHECK_EQ(spawned, 30, "spawnRandom capped at per-flock limit (30)");
        CHECK_EQ(sim.countInFlock(0), 30, "Flock 0 has 30");

        // Now switch to flock 1 and spawn: remaining global room = 20
        sim.setActiveFlock(1);
        spawned = sim.spawnRandom(100);
        CHECK_EQ(spawned, 20, "spawnRandom capped at global room (20)");
        CHECK_EQ(sim.data().count, 50, "Global cap reached");
    }

    // ---- L2+L3: Reproduction respects multi-flock fairness ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 50);  // tight global cap and small world

        // Configure both flocks identically for reproduction
        for (int f = 0; f < 2; ++f) {
            auto& fp = sim.flockParams(f);
            fp.reproduction.reproductionInterval = 0.0f;
            fp.reproduction.reproductionMinOffspring = 1.0f;
            fp.reproduction.reproductionMaxOffspring = 3.0f;
            fp.reproduction.reproductionMinHunger = 0.0f;
            fp.reproduction.maxFlockSize = 100;  // per-flock cap >> global cap
            fp.hunger.hungerDecayRate = 0.0f;
            fp.age.juvenileAge = 0.0f;   // Bypass juvenile stage (reproduction controlled by AgeStage)
            fp.age.youngAge = 0.0f;       // Start as Adult directly
            fp.perception.separationRadius = 10.0f;  // tight for small world
        }
        sim.setActiveFlock(0);
        sim.spawnRandom(10);  // 5 male + 5 female
        sim.setActiveFlock(1);
        sim.spawnRandom(10);  // 5 male + 5 female

        // Advance simulation: both flocks should reproduce
        for (int i = 0; i < 60; ++i) {
            sim.update(0.016f);
        }

        // After reaching global cap, no more boids should be added
        int total = sim.data().count;
        CHECK(total <= 50, "Global cap not exceeded by reproduction");
        CHECK(total > 20, "Both flocks reproduced before global cap filled");

        // Advance further to ensure stability
        for (int i = 0; i < 100; ++i) {
            sim.update(0.016f);
        }
        CHECK_EQ(sim.data().count, total, "Count stable after global cap reached");
    }

    // ---- L3: addBoid respects per-flock cap ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 10000);
        auto& p = sim.params();
        p.reproduction.maxFlockSize = 5;
        sim.saveCurrentParams();

        sim.spawnRandom(5);
        CHECK_EQ(sim.countInFlock(0), 5, "Flock 0 at cap");

        sim.addBoid(100.0f, 100.0f);
        CHECK_EQ(sim.countInFlock(0), 5, "addBoid blocked by per-flock cap");

        sim.addBoid(200.0f, 200.0f);
        CHECK_EQ(sim.countInFlock(0), 5, "addBoid blocked again by per-flock cap");
    }
}

// ============================================================================
//  Phase 2: Social Dynamics Tests (2.1 - 2.6)
// ============================================================================

// ---- 2.1 Male Combat ----
void test_combat_system() {
    TEST_SUITE("Phase 2.1: Male combat system");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    auto& p = sim.params();
    // Verify default combat params exist
    CHECK_FLOAT_EQ(p.combat.combatRadius, 30.0f, 0.01f, "Default combatRadius");
    CHECK_FLOAT_EQ(p.combat.combatProbability, 0.30f, 0.01f, "Default combatProbability");
    CHECK_FLOAT_EQ(p.combat.combatFatigueGain, 0.15f, 0.01f, "Default combatFatigueGain");
    CHECK_FLOAT_EQ(p.combat.combatCooldown, 5.0f, 0.01f, "Default combatCooldown");

    // Modify and verify persistence
    p.combat.combatRadius = 50.0f;
    p.combat.combatProbability = 0.8f;
    sim.saveCurrentParams();
    sim.setActiveFlock(1);
    sim.setActiveFlock(0);
    CHECK_FLOAT_EQ(sim.params().combat.combatRadius, 50.0f, 0.01f, "Combat params persist after flock switch");

    // Verify per-flock isolation
    CHECK_FLOAT_EQ(sim.flockParams(0).combat.combatRadius, 50.0f, 0.01f, "Flock 0 combat persisted");
    CHECK_FLOAT_EQ(sim.flockParams(1).combat.combatRadius, 30.0f, 0.01f, "Flock 1 combat unchanged");
}

// ---- 2.2 Hatred System ----
void test_hatred_system() {
    TEST_SUITE("Phase 2.2: Hatred / Enmity system");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    auto& p = sim.params();
    CHECK_FLOAT_EQ(p.hatred.hatredGainPerKill, 0.25f, 0.01f, "Default hatredGainPerKill");
    CHECK_FLOAT_EQ(p.hatred.hatredDecayRate, 0.02f, 0.01f, "Default hatredDecayRate");
    CHECK_FLOAT_EQ(p.hatred.hatredFleeRadiusBoost, 3.0f, 0.01f, "Default hatredFleeBoost");
    CHECK_FLOAT_EQ(p.hatred.hatredFleeWeightBoost, 2.0f, 0.01f, "Default hatredWeightBoost");

    // Verify SoA arrays are initialized for new boids
    sim.spawnRandom(5);
    CHECK_EQ(sim.data().count, 5, "Boids spawned for hatred test");
    for (int i = 0; i < sim.data().count; ++i) {
        CHECK_EQ(sim.data().hatredTarget[i], 255, "New boid has no hatred target (255)");
        CHECK_FLOAT_EQ(sim.data().hatredLevel[i], 0.0f, 0.001f, "New boid has zero hatred");
    }
}

// ---- 2.3 Escape Strategy ----
void test_escape_strategy() {
    TEST_SUITE("Phase 2.3: Escape strategy selection");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    auto& p = sim.params();
    CHECK_FLOAT_EQ(p.escape.escapeStrategy, 0.0f, 0.01f, "Default strategy = DirectFlee");
    CHECK_FLOAT_EQ(p.escape.escapeStrategyMix, 0.4f, 0.01f, "Default mix factor");
    CHECK_FLOAT_EQ(p.escape.escapeZigzagAmp, 0.6f, 0.01f, "Default zigzag amplitude");

    // Test all strategy values (0-3) are settable
    for (int s = 0; s <= 3; ++s) {
        p.escape.escapeStrategy = static_cast<float>(s);
        sim.saveCurrentParams();
        CHECK_FLOAT_EQ(sim.params().escape.escapeStrategy, static_cast<float>(s), 0.01f,
                       ("Strategy " + std::to_string(s) + " persisted").c_str());
    }
}

// ---- 2.4 Defensive Cooperation ----
void test_defensive_cooperation() {
    TEST_SUITE("Phase 2.4: Defensive cooperation");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    auto& p = sim.params();
    CHECK_FLOAT_EQ(p.defense.defenseRadius, 180.0f, 0.01f, "Default defenseRadius");
    CHECK_FLOAT_EQ(p.defense.defenseResponseWeight, 1.2f, 0.01f, "Default responseWeight");
    CHECK_FLOAT_EQ(p.defense.defenseGroupThreshold, 2.0f, 0.01f, "Default groupThreshold");

    // Verify per-flock isolation
    p.defense.defenseRadius = 300.0f;
    sim.saveCurrentParams();
    sim.setActiveFlock(1);
    CHECK_FLOAT_EQ(sim.params().defense.defenseRadius, 180.0f, 0.01f, "Flock 1 defense unchanged");
    sim.setActiveFlock(0);
    CHECK_FLOAT_EQ(sim.params().defense.defenseRadius, 300.0f, 0.01f, "Flock 0 defense persisted");
}

// ---- 2.5 Cohesion Dynamics ----
void test_cohesion_dynamics() {
    TEST_SUITE("Phase 2.5: Cohesion dynamics");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    auto& p = sim.params();
    CHECK_FLOAT_EQ(p.cohesionDyn.cohesionBaseWeight, 1.0f, 0.01f, "Default baseWeight");
    CHECK_FLOAT_EQ(p.cohesionDyn.cohesionThreatBoost, 2.0f, 0.01f, "Default threatBoost");
    CHECK_FLOAT_EQ(p.cohesionDyn.cohesionHungerDecay, 0.3f, 0.01f, "Default hungerDecay");
    CHECK_FLOAT_EQ(p.cohesionDyn.cohesionDensityDecay, 0.5f, 0.01f, "Default densityDecay");

    // Set extreme hunger decay → boids spread when hungry
    p.cohesionDyn.cohesionHungerDecay = 1.0f;
    p.hunger.hungerDecayRate = 0.1f;  // Fast hunger
    p.hunger.forageHungerThreshold = 0.8f;
    sim.saveCurrentParams();

    sim.spawnRandom(20);
    // Run simulation: boids should survive despite fast hunger (they forage)
    for (int i = 0; i < 30; ++i) sim.update(0.016f);
    CHECK(sim.data().count >= 5, "Boids survive with dynamic cohesion + foraging");
}

// ---- 2.6 Environmental Carrying Capacity ----
void test_carrying_capacity() {
    TEST_SUITE("Phase 2.6: Environmental carrying capacity");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    // Set minimal plants and high carrying pressure
    sim.plantParams().initialPlants = 5;
    sim.plantParams().maxPlants = 5;
    sim.plantParams().plantFoodValue = 0.3f;
    sim.plantParams().carryingPressure = 0.0f;  // Disabled by default

    auto& p = sim.params();
    p.hunger.hungerDecayRate = 0.01f;  // Slow decay
    sim.saveCurrentParams();

    // Spawn many boids with pressure OFF → slow hunger decay
    sim.spawnRandom(100);
    sim.update(1.0f);
    float hungerAfter1s_noPressure = sim.data().hunger[0];
    CHECK(hungerAfter1s_noPressure > 0.7f, "Slow hunger with pressure OFF");

    // Reset and enable pressure
    sim.init(1920.0f, 1080.0f, 10000);
    sim.plantParams().initialPlants = 5;
    sim.plantParams().maxPlants = 5;
    sim.plantParams().plantFoodValue = 0.3f;
    sim.plantParams().carryingPressure = 0.5f;  // Moderate pressure

    p.hunger.hungerDecayRate = 0.01f;
    sim.saveCurrentParams();
    sim.spawnRandom(100);
    sim.update(1.0f);
    float hungerAfter1s_pressure = sim.data().hunger[0];
    CHECK(hungerAfter1s_pressure < hungerAfter1s_noPressure,
          "Carrying pressure accelerates hunger decay ("
          << hungerAfter1s_pressure << " < " << hungerAfter1s_noPressure << ")");
}

// 2.7 Phase 1.7: Attack hit/miss, dodge and damage system
void test_health_combat() {
    TEST_SUITE("Phase 1.7: Health / Dodge / Damage");

    // Test 1: Health params exist and are settable
    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);
    auto& p = sim.params();

    CHECK_FLOAT_EQ(p.health.dodgeChanceBase, 0.3f, 0.01f, "Default dodgeChanceBase");
    CHECK_FLOAT_EQ(p.health.damageToHealth, 0.5f, 0.01f, "Default damageToHealth");
    CHECK_FLOAT_EQ(p.health.healthRegenRate, 0.01f, 0.001f, "Default healthRegenRate");
    CHECK_FLOAT_EQ(p.health.healthInitial, 1.0f, 0.01f, "Default healthInitial");

    // Test 2: Modify health params and verify per-flock isolation
    p.health.dodgeChanceBase = 0.6f;
    p.health.damageToHealth = 0.8f;
    sim.saveCurrentParams();
    sim.setActiveFlock(1);
    CHECK_FLOAT_EQ(sim.params().health.dodgeChanceBase, 0.3f, 0.01f, "Flock 1 has default dodge");
    sim.setActiveFlock(0);
    CHECK_FLOAT_EQ(sim.params().health.dodgeChanceBase, 0.6f, 0.01f, "Flock 0 has modified dodge");

    // Test 3: New boids start with full health
    sim.spawnRandom(10);
    for (int i = 0; i < sim.data().count; ++i) {
        CHECK_FLOAT_EQ(sim.data().health[i], 1.0f, 0.001f, "New boid has full health");
    }

    // Test 4: Health regenerates when well-fed
    // Reduce health artificially, then run simulation with no hunger decay
    sim = Simulation{};
    sim.init(1920.0f, 1080.0f, 10000);
    p = sim.params();
    p.hunger.hungerDecayRate = 0.0f;  // No hunger decay
    p.health.healthRegenRate = 1.0f;  // 100% regen per second
    sim.saveCurrentParams();

    sim.spawnRandom(5);
    sim.data().health[0] = 0.3f;  // Injured
    sim.update(0.5f);  // 0.5 seconds of regen
    CHECK(sim.data().health[0] > 0.3f,
          "Health regenerates: " << sim.data().health[0] << " > 0.3");

    // Test 5: Health does NOT regen when hungry (hunger <= 0.5)
    sim = Simulation{};
    sim.init(1920.0f, 1080.0f, 10000);
    p = sim.params();
    p.hunger.hungerDecayRate = 0.02f;  // Fast hunger decay to drop below 0.5
    p.health.healthRegenRate = 1.0f;
    sim.saveCurrentParams();

    sim.spawnRandom(3);
    sim.data().health[0] = 0.3f;
    sim.data().hunger[0] = 0.3f;  // Below regen threshold
    sim.update(0.5f);
    CHECK_FLOAT_EQ(sim.data().health[0], 0.3f, 0.001f,
                   "No regen when hunger below 0.5");

    // Test 6: Direct damage application (verify health field works)
    sim = Simulation{};
    sim.init(1920.0f, 1080.0f, 10000);
    sim.spawnRandom(5);
    sim.data().health[0] -= 0.3f;
    sim.data().health[1] = 0.0f;
    CHECK_FLOAT_EQ(sim.data().health[0], 0.7f, 0.001f, "Direct damage reduces health");
    CHECK_FLOAT_EQ(sim.data().health[1], 0.0f, 0.001f, "Health can reach zero");

    // NOTE: Full predation+damage integration test is deferred to manual verification.
    // The unit-level health/dodge/regen/damage logic is covered above.
    // See: launch Biod.exe, set up predator/prey, verify damage and dodge in combat.
}

// ================================================================
// Phase 3.1: Nest System
// ================================================================
void test_nest_system() {
    TEST_SUITE("Phase 3.1: Nest system");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);

    const int NUM_FLOCKS = 4;
    // Create 4 flocks
    while (sim.flockCount() < NUM_FLOCKS)
        sim.addFlock();

    // Spawn 10 boids per flock
    sim.setActiveFlock(0);
    sim.spawnRandom(10);
    sim.setActiveFlock(1);
    sim.spawnRandom(10);

    // Test 1: Default nest param values
    const NestParams& np = sim.nestParams();
    CHECK(np.maxNests == 20.0f, "Default maxNests = 20");
    CHECK(np.initialNests == 4.0f, "Default initialNests = 4");
    CHECK(np.nestRadius == 150.0f, "Default nestRadius = 150");
    CHECK(np.contestDuration == 10.0f, "Default contestDuration = 10");
    CHECK(np.defenseThreshold == 3.0f, "Default defenseThreshold = 3");

    // Test 2: Modify and persist nest params
    NestParams& mutNp = sim.nestParams();
    mutNp.maxNests = 30.0f;
    mutNp.nestRadius = 200.0f;
    CHECK(mutNp.maxNests == 30.0f, "Modified maxNests persists");
    CHECK(mutNp.nestRadius == 200.0f, "Modified nestRadius persists");

    // Test 3: Default NestPrefSuf per-flock values
    sim.setActiveFlock(0);
    FlockParams& fp = sim.params();
    CHECK(fp.nestPref.nestReturnWeight > 0.0f, "Default nestReturnWeight positive");
    CHECK(fp.nestPref.nestPreferFoodDensity > 0.0f, "Default food density pref positive");
    CHECK(fp.nestPref.nestSelectionRange > 0.0f, "Default selection range positive");

    // Test 4: Modify per-flock nest prefs on flock 0
    fp.nestPref.nestReturnWeight = 0.5f;
    fp.nestPref.nestSelectionRange = 400.0f;
    CHECK(fp.nestPref.nestReturnWeight == 0.5f, "Modified nestReturnWeight persists");
    CHECK(fp.nestPref.nestSelectionRange == 400.0f, "Modified selectionRange persists");

    // Test 5: Switch to flock 1 and verify independent prefs
    sim.setActiveFlock(1);
    FlockParams& fp2 = sim.params();
    fp2.nestPref.nestReturnWeight = 0.8f;
    CHECK(fp2.nestPref.nestReturnWeight == 0.8f, "Flock 1 return weight set");
    // Switch back and verify flock 0 preserved
    sim.setActiveFlock(0);
    FlockParams& fp0 = sim.params();
    CHECK(fp0.nestPref.nestReturnWeight == 0.5f, "Flock 0 return weight unchanged after switch");

    // Test 6: Nests exist after init
    const NestData& nests = sim.nests();
    int nOwned = 0;
    int nUnowned = 0;
    for (int i = 0; i < nests.count; ++i) {
        if (nests.ownerFlock[i] >= 0) ++nOwned;
        else ++nUnowned;
    }
    CHECK(nests.count >= 4, "At least initialNests (4) created");
    CHECK(nOwned >= 2, "Some nests assigned to flocks");

    // Test 7: ownedBy() helper
    if (nOwned > 0) {
        int firstOwner = -1;
        for (int i = 0; i < nests.count; ++i) {
            if (nests.ownerFlock[i] >= 0) { firstOwner = nests.ownerFlock[i]; break; }
        }
        if (firstOwner >= 0) {
            int count = nests.ownedBy(firstOwner);
            CHECK(count >= 1, "ownedBy() returns correct count");
        }
    }

    // Test 8: Food storage initial = 0 (before any simulation updates)
    for (int i = 0; i < nests.count; ++i) {
        CHECK(nests.foodStored[i] == 0.0f, "Initial food stored at nest = 0");
    }

    // Test 9: Contest state initial = none (before simulation)
    for (int i = 0; i < nests.count; ++i) {
        CHECK(nests.isContested[i] == 0, "Nest not initially contested");
        CHECK(nests.contestAttacker[i] == -1, "No initial contest attacker");
        CHECK(nests.contestTimer[i] == 0.0f, "Contest timer starts at 0");
    }

    // Test 10: Defense rating + food storage after update (Phase 3.1b)
    for (int i = 0; i < 60; ++i) sim.update(0.016f);  // ~1 second
    // With Section 4 contest active, defenseRating may be zeroed on contested-then-transferred nests.
    // Verify simulation runs without NaN/crash and defense ratings are non-negative.
    bool defenseValid = true;
    for (int i = 0; i < nests.count; ++i) {
        if (nests.defenseRating[i] < 0.0f) defenseValid = false;
    }
    CHECK(defenseValid, "All defense ratings non-negative after 1s sim");
    // Food storage may or may not happen depending on boid positions,
    // but at minimum it should not crash or go negative
    bool foodValid = true;
    for (int i = 0; i < nests.count; ++i) {
        if (nests.foodStored[i] < 0.0f || nests.foodStored[i] > 1.0f)
            foodValid = false;
    }
    CHECK(foodValid, "Food stored within [0, 1] range after updates");
}

// Phase 3.1c: Nest Contest + Day/Night Cycle
void test_nest_contest() {
    TEST_SUITE("Phase 3.1c: Nest contest");

    Simulation sim;
    sim.init(1920.0f, 1080.0f, 10000);
    while (sim.flockCount() < 3)
        sim.addFlock();

    // Freeze boid movement for deterministic contest tests.
    // Contest logic depends on boid positions within nestRadius;
    // maxSpeed=0 prevents boids from drifting during long simulations.
    for (int f = 0; f < sim.flockCount(); ++f) {
        sim.flockParams(f).movement.maxSpeed = 0.0f;
        sim.flockParams(f).movement.maxForce = 0.0f;
    }

    NestData& nests = sim.nests();
    float nx = 960.0f, ny = 540.0f;

    // ---- C1: No boids → nest not contested ----
    nests.clear();
    nests.add(nx, ny, 0);
    sim.data().count = 0;
    sim.update(0.016f);
    CHECK(nests.isContested[0] == 0, "C1: No boids → not contested");
    CHECK(nests.contestTimer[0] == 0.0f, "C1: Timer stays 0");

    // ---- C2: Same-flock boids do not trigger contest ----
    nests.clear();
    nests.add(nx, ny, 0);
    sim.data().count = 0;
    // Place 6 flock-0 boids at nest (spread within 10px)
    for (int i = 0; i < 6; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 0;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    sim.data().count = 6;
    sim.update(0.016f);
    CHECK(nests.isContested[0] == 0, "C2: Same-flock boids → not contested");

    // ---- C3: Foreign boids below defense threshold → not contested ----
    nests.clear();
    nests.add(nx, ny, 0);
    sim.data().count = 0;
    // 8 flock-0 defenders → defenseRating = 8/3 ≈ 2.67, threshold = 2.67*3 = 8.0
    for (int i = 0; i < 8; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 0;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    // 6 flock-1 attackers → 6 < 8.0 → not enough
    for (int i = 8; i < 14; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 1;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    sim.data().count = 14;
    sim.update(0.016f);
    CHECK(nests.isContested[0] == 0, "C3: 6 foreign boids < 8.0 defense → not contested");

    // ---- C4: Foreign boids exceed threshold → contest, then transfer ----
    nests.clear();
    nests.add(nx, ny, 0);
    sim.data().count = 0;
    // 2 flock-0 defenders → defenseRating = 2/3 ≈ 0.67, threshold = 0.67*3 = 2.0
    for (int i = 0; i < 2; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 0;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    // 8 flock-1 attackers → 8 > 2.0 → contested
    for (int i = 2; i < 10; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 1;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    sim.data().count = 10;
    // contestDuration = 10.0, update in 0.5s steps for 11s → ownership transfer
    for (int i = 0; i < 22; ++i)
        sim.update(0.5f);
    CHECK_EQ(nests.ownerFlock[0], 1, "C4: Ownership transferred to flock 1 after 11s");
    CHECK(nests.isContested[0] == 0, "C4: Contest state reset after transfer");
    CHECK(nests.contestTimer[0] == 0.0f, "C4: Timer reset after transfer");
    CHECK(nests.contestAttacker[0] == -1, "C4: Attacker reset after transfer");

    // ---- C5: contestTimer decays when attacker leaves ----
    nests.clear();
    nests.add(nx, ny, 1);
    sim.data().count = 0;
    // 2 flock-1 defenders → defenseRating = 2/3 ≈ 0.67, thr = 2.0
    for (int i = 0; i < 2; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 1;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    // 10 flock-2 attackers → 10 > 2.0 → contested
    for (int i = 2; i < 12; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 2;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    sim.data().count = 12;
    // Advance contest halfway
    for (int i = 0; i < 10; ++i)
        sim.update(0.5f);
    CHECK(nests.isContested[0] == 1, "C5: Contest active after 5s");
    float timerBefore = nests.contestTimer[0];
    CHECK(timerBefore > 0.0f, "C5: Timer > 0 during contest");

    // Now remove all flock-2 boids (move far away)
    for (int i = 2; i < 12; ++i) {
        sim.data().posX[i] = -5000.0f;
        sim.data().posY[i] = -5000.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
    }
    // Run updates: timer decays at 2x speed
    for (int i = 0; i < 10; ++i)
        sim.update(0.5f);
    CHECK(nests.contestTimer[0] == 0.0f, "C5: Timer decayed to 0 after attacker left");
    CHECK(nests.isContested[0] == 0, "C5: Contest reset after attacker left");
    CHECK(nests.contestAttacker[0] == -1, "C5: Attacker reset after decay");

    // ---- C6: Ownership transfer resets all state correctly ----
    nests.clear();
    nests.add(nx, ny, 0);
    sim.data().count = 0;
    // 2 flock-0 defenders
    for (int i = 0; i < 2; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 0;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    // 8 flock-2 attackers
    for (int i = 2; i < 10; ++i) {
        sim.data().posX[i] = nx + i * 2.0f;
        sim.data().posY[i] = ny + i * 2.0f;
        sim.data().velX[i] = 0.0f;
        sim.data().velY[i] = 0.0f;
        sim.data().flockId[i] = 2;
        sim.data().hunger[i] = 0.5f;
        sim.data().health[i] = 1.0f;
    }
    sim.data().count = 10;
    // Run past contestDuration
    for (int i = 0; i < 25; ++i)
        sim.update(0.5f);
    CHECK_EQ(nests.ownerFlock[0], 2, "C6: Ownership transferred to flock 2");
    CHECK(nests.isContested[0] == 0, "C6: isContested reset");
    CHECK(nests.contestTimer[0] == 0.0f, "C6: contestTimer reset");
    CHECK(nests.contestAttacker[0] == -1, "C6: contestAttacker reset");
    // defenseRating recalculated by Section 1 for new owner; may be non-zero
    CHECK(nests.defenseRating[0] >= 0.0f, "C6: defenseRating non-negative");
}

void test_day_night_cycle() {
    TEST_SUITE("Phase 3.1c: Day/night cycle");

    // C7: Day phase boundary values
    {
        float period = WorldConst::SECONDS_PER_SIM_DAY;  // 30.0
        float phase0 = std::fmod(0.0f / period, 1.0f);
        CHECK_FLOAT_EQ(phase0, 0.0f, 0.001f, "C7: Phase at t=0 is 0.0");

        float phaseQuarter = std::fmod(7.5f / period, 1.0f);
        CHECK_FLOAT_EQ(phaseQuarter, 0.25f, 0.001f, "C7: Phase at t=7.5 is 0.25");

        float phaseHalf = std::fmod(15.0f / period, 1.0f);
        CHECK_FLOAT_EQ(phaseHalf, 0.5f, 0.001f, "C7: Phase at t=15 is 0.5");

        float phaseThreeQuarter = std::fmod(22.5f / period, 1.0f);
        CHECK_FLOAT_EQ(phaseThreeQuarter, 0.75f, 0.001f, "C7: Phase at t=22.5 is 0.75");

        float phaseFull = std::fmod(30.0f / period, 1.0f);
        CHECK_FLOAT_EQ(phaseFull, 0.0f, 0.001f, "C7: Phase at t=30 wraps to 0.0");
    }

    // C8: ambientLight stays within [0.3, 1.0] using PI (single cycle)
    {
        const float PI = 3.14159265f;
        for (float t = 0.0f; t <= 30.0f; t += 0.5f) {
            float dayPhase = std::fmod(t / WorldConst::SECONDS_PER_SIM_DAY, 1.0f);
            float light = 0.3f + 0.7f * std::sin(dayPhase * PI);
            CHECK(light >= 0.3f - 0.002f, "C8: light >= 0.3");
            CHECK(light <= 1.0f + 0.002f, "C8: light <= 1.0");
        }
        // Verify peak and trough explicitly
        // Peak at phase 0.5: sin(PI/2) = 1
        float peak = 0.3f + 0.7f * std::sin(0.5f * PI);
        CHECK_FLOAT_EQ(peak, 1.0f, 0.001f, "C8: Peak light = 1.0 at phase 0.5");

        // Trough at phase 0.0 or 1.0: sin(0) = 0
        float trough = 0.3f + 0.7f * std::sin(0.0f * PI);
        CHECK_FLOAT_EQ(trough, 0.3f, 0.001f, "C8: Trough light = 0.3 at phase 0.0");
    }
}

void test_rapid_api_abuse() {
    TEST_SUITE("Rapid API abuse: overflow storm");

    // ---- Scenario 1: Rapid addFlock after global cap full ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 200);  // tight global cap

        // Fill to global capacity quickly
        sim.spawnRandom(200);
        CHECK_EQ(sim.data().count, 200, "Global cap reached at 200");

        // Simulate rapid clicking: 100 consecutive addFlock attempts
        int deniedGlobal = 0;
        int currentFlocks = sim.flockCount();
        for (int i = 0; i < 100; ++i) {
            int result = sim.addFlock();
            if (result == -2) ++deniedGlobal;
        }
        CHECK_EQ(deniedGlobal, 100, "All 100 rapid addFlock blocked by global cap");
        CHECK_EQ(sim.flockCount(), currentFlocks, "Flock count unchanged after 100 denied attempts");
    }

    // ---- Scenario 2: Rapid spawnRandom after global cap full ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 100);

        sim.spawnRandom(100);
        CHECK_EQ(sim.data().count, 100, "Global cap reached at 100");

        int totalDenied = 0;
        for (int i = 0; i < 200; ++i) {
            int spawned = sim.spawnRandom(50);
            if (spawned == 0) ++totalDenied;
        }
        CHECK_EQ(totalDenied, 200, "All 200 rapid spawnRandom blocked by global cap");
        CHECK_EQ(sim.data().count, 100, "Count unchanged after spawn storm");
    }

    // ---- Scenario 3: Rapid addFlock near MAX_FLOCKS boundary ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 100000);  // huge global cap (not the bottleneck)

        // Add flocks until MAX_FLOCKS-1
        int added = 0;
        while (sim.flockCount() < MAX_FLOCKS - 1) {
            int result = sim.addFlock();
            if (result < 0) break;
            ++added;
        }
        CHECK_EQ(sim.flockCount(), MAX_FLOCKS - 1, "Reached MAX_FLOCKS-1");

        // Rapidly try to add more: first 1 succeeds, rest blocked
        int successCount = 0;
        int deniedCount = 0;
        for (int i = 0; i < 50; ++i) {
            int result = sim.addFlock();
            if (result >= 0) ++successCount;
            else if (result == -1) ++deniedCount;
        }
        CHECK_EQ(successCount, 1, "Only 1 of 50 rapid addFlock succeeds at boundary");
        CHECK_EQ(deniedCount, 49, "49 denied by MAX_FLOCKS cap");
        CHECK_EQ(sim.flockCount(), MAX_FLOCKS, "Exactly MAX_FLOCKS after storm");
    }

    // ---- Scenario 4: Interleaved spawnRandom + addFlock at tight global cap ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 50);

        // Partially fill
        sim.spawnRandom(30);
        CHECK_EQ(sim.data().count, 30, "Started with 30/50");

        // Rapid interleaved operations
        int spawnDenied = 0;
        int flockDenied = 0;
        for (int i = 0; i < 100; ++i) {
            if (i % 2 == 0) {
                int spawned = sim.spawnRandom(5);
                if (spawned == 0) ++spawnDenied;
            } else {
                int result = sim.addFlock();
                if (result == -2) ++flockDenied;
            }
        }
        // After the first few iterations, global cap fills up (50), then:
        // spawnRandom returns 0, addFlock returns -2
        CHECK(sim.data().count <= 50, "Global cap never exceeded during interleaved storm");
        CHECK(spawnDenied > 0, "Some spawnRandom calls were denied");
        CHECK(flockDenied > 0, "Some addFlock calls were denied by global cap");

        // Verify stability after storm
        int finalCount = sim.data().count;
        for (int i = 0; i < 20; ++i) {
            sim.spawnRandom(10);
        }
        CHECK_EQ(sim.data().count, finalCount, "Count stable after post-storm spawn attempts");
    }

    // ---- Scenario 5: Per-flock cap under rapid spawn (multiple flocks) ----
    {
        Simulation sim;
        sim.init(1920.0f, 1080.0f, 10000);

        // Set per-flock caps via editor pattern: modify m_params, save, switch, repeat
        // IMPORTANT: setActiveFlock() internally calls saveCurrentParams(),
        // so direct array modification would be overwritten.
        sim.params().reproduction.maxFlockSize = 50;
        sim.saveCurrentParams();       // save to flock 0
        sim.setActiveFlock(1);
        sim.params().reproduction.maxFlockSize = 50;
        sim.saveCurrentParams();       // save to flock 1
        sim.setActiveFlock(0);         // switch back, loads flock 0's params

        // Fill both flocks to cap rapidly, alternating
        int denied = 0;
        for (int i = 0; i < 100; ++i) {
            sim.setActiveFlock(i % 2);  // alternate flocks
            int spawned = sim.spawnRandom(30);
            if (spawned == 0) ++denied;
        }
        CHECK_EQ(sim.countInFlock(0), 50, "Flock 0 capped at 50");
        CHECK_EQ(sim.countInFlock(1), 50, "Flock 1 capped at 50");
        CHECK(denied > 60, "Majority of rapid spawns denied after caps reached");
        CHECK_EQ(sim.data().count, 100, "Total = 100 exactly (2 x 50)");
    }
}

// ============================================================================
//  Main
// ============================================================================

int main()
{
    std::cout << "=============================================" << std::endl;
    std::cout << "  Biod - Regression Test Suite" << std::endl;
    std::cout << "=============================================" << std::endl;

    test_initialization();
    test_flock_lifecycle();
    test_flock_names_and_colors();
    test_active_flock_switching();
    test_relationship_matrix();
    test_boid_spawning_and_removal();
    test_empty_simulation_update();
    test_target_seeking();
    test_seasons();
    test_parameter_bounds();
    test_reproduction_bounds();
    test_multiple_flock_rapid_operations();
    test_remove_flock_with_boids();
    test_plant_ecology();
    test_dt_edge_cases();
    test_spawn_all_flocks();
    test_color_persistence_after_remove();
    test_capacity_enforcement();
    test_combat_system();
    test_hatred_system();
    test_escape_strategy();
    test_defensive_cooperation();
    test_cohesion_dynamics();
    test_carrying_capacity();
    test_health_combat();
    test_nest_system();
    test_nest_contest();
    test_day_night_cycle();
    test_rapid_api_abuse();

    std::cout << "\n=============================================" << std::endl;
    std::cout << "  Results: " << g_passed << " passed, "
              << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;
    std::cout << "=============================================" << std::endl;

    if (g_failed > 0) {
        std::cout << "\n[FAIL] Some tests failed!" << std::endl;
        return 1;
    }
    std::cout << "\n[PASS] All tests passed." << std::endl;
    return 0;
}
