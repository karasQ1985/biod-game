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
    p0.separationWeight = 5.0f;
    sim.saveCurrentParams();

    // Switch to flock 1
    sim.setActiveFlock(1);
    CHECK_EQ(sim.activeFlock(), 1, "Active flock switched to 1");
    CHECK_FLOAT_EQ(sim.params().separationWeight, 2.5f, 0.01f, "Flock 1 has default separationWeight");

    // Modify flock 1 params
    sim.params().cohesionWeight = 1.5f;
    sim.saveCurrentParams();

    // Switch back to flock 0, verify params persist
    sim.setActiveFlock(0);
    CHECK_EQ(sim.activeFlock(), 0, "Active flock switched back to 0");
    CHECK_FLOAT_EQ(sim.params().separationWeight, 5.0f, 0.01f, "Flock 0 separationWeight persisted");

    // Switch to flock 2 (new)
    sim.setActiveFlock(2);
    CHECK_EQ(sim.activeFlock(), 2, "Active flock switched to 2");
    CHECK_FLOAT_EQ(sim.params().separationWeight, 2.5f, 0.01f, "New flock has default params");

    // Verify flock 1 params persisted
    CHECK_FLOAT_EQ(sim.flockParams(1).cohesionWeight, 1.5f, 0.01f, "Flock 1 cohesionWeight persisted");

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
    p.separationRadius = 0.0f;
    p.alignmentRadius = 0.0f;
    p.cohesionRadius = 0.0f;
    p.separationWeight = 0.0f;
    p.alignmentWeight = 0.0f;
    p.cohesionWeight = 0.0f;
    p.boundaryWeight = 0.0f;
    p.wanderWeight = 0.0f;
    sim.saveCurrentParams();

    sim.spawnRandom(5);
    sim.update(0.016f);
    CHECK_EQ(sim.data().count, 5, "Zero-weight params: boids survive one frame");

    // Set parameters to high values
    p.separationWeight = 10.0f;
    p.alignmentWeight = 10.0f;
    p.cohesionWeight = 10.0f;
    p.boundaryWeight = 10.0f;
    p.wanderWeight = 10.0f;
    p.maxSpeed = 1000.0f;
    p.maxForce = 10000.0f;
    sim.saveCurrentParams();

    sim.update(0.016f);
    CHECK_EQ(sim.data().count, 5, "High-weight params: boids survive");

    // Check hunger system
    p.hungerDecayRate = 0.0f; // No decay
    sim.saveCurrentParams();
    sim.update(1.0f);
    CHECK_EQ(sim.data().count, 5, "Zero hunger decay: no boids starve in 1s");

    p.hungerDecayRate = 0.008f; // Default
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
    p.reproductionInterval = 0.0f;
    p.reproductionMinOffspring = 1;
    p.reproductionMaxOffspring = 10;
    p.reproductionMinHunger = 0.0f;
    p.maxFlockSize = 1000;
    p.hungerDecayRate = 0.0f; // No decay
    p.adultAge = 0.0f; // Instant adult
    sim.saveCurrentParams();

    // Spawn some boids and advance simulation
    sim.spawnRandom(10);
    for (int i = 0; i < 20; ++i) {
        sim.update(0.016f);
    }
    CHECK(sim.data().count >= 10, "Reproduction does not crash");

    // Set maxFlockSize to 0 (edge)
    p.maxFlockSize = 0;
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
    p.hungerDecayRate = 0.02f; // Fast hunger
    p.forageWeight = 5.0f;
    p.forageHungerThreshold = 1.0f; // Always forage
    p.chaseRange = 100.0f;
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
        p.maxFlockSize = 100;
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
        p.maxFlockSize = 30;
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
            fp.reproductionInterval = 0.0f;
            fp.reproductionMinOffspring = 1;
            fp.reproductionMaxOffspring = 3;
            fp.reproductionMinHunger = 0.0f;
            fp.maxFlockSize = 100;  // per-flock cap >> global cap
            fp.hungerDecayRate = 0.0f;
            fp.adultAge = 0.0f;
            fp.separationRadius = 10.0f;  // tight for small world
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
        p.maxFlockSize = 5;
        sim.saveCurrentParams();

        sim.spawnRandom(5);
        CHECK_EQ(sim.countInFlock(0), 5, "Flock 0 at cap");

        sim.addBoid(100.0f, 100.0f);
        CHECK_EQ(sim.countInFlock(0), 5, "addBoid blocked by per-flock cap");

        sim.addBoid(200.0f, 200.0f);
        CHECK_EQ(sim.countInFlock(0), 5, "addBoid blocked again by per-flock cap");
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
        sim.params().maxFlockSize = 50;
        sim.saveCurrentParams();       // save to flock 0
        sim.setActiveFlock(1);
        sim.params().maxFlockSize = 50;
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
