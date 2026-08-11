# Biod -- Task Tracking

> Auto-generated: 2026-08-11
> Source: 需求分析与发展规划.md
> Convention: After each task completion, this document is auto-updated and next tasks are prioritized.

---

## Status Legend

| Mark | Meaning |
|------|---------|
| ✅ | Done (tested, integrated) |
| ⚠️ | In progress |
| ❌ | Not started |
| ⏸️ | Deferred (dependency not met or user decision) |
| 🔮 | Future milestone (Phase 4+) |

---

## Phase 0: Architecture Foundation

| ID | Task | Status | Priority | Depends On | Notes |
|----|------|--------|----------|------------|-------|
| 0.1 | WorldConstants.h unit system | ✅ | -- | -- | src/core/WorldConstants.h; 5 physical quantity categories |
| 0.2 | Boid state machine (BoidState) | ✅ | -- | -- | 10 states; determineStates() in stepFlocks |
| 0.3 | Simulation::update() modular split | ✅ | -- | 0.2 | stepHunger/updateNests/stepFlocks/stepIntegration |
| 0.4 | FlockParams hierarchical split | ✅ | -- | -- | 19 sub-structs, standard-layout for offsetof() |
| 0.5 | Config save/load (JSON) | ✅ | -- | 0.4 | SimulationConfig.cpp; .biodcfg format |
| 0.6 | Boundary mode configurable | ✅ | -- | -- | BoundaryMode enum: Torus/SoftWall/HardWall/Hybrid |
| 0.7 | Map size configurable UI | ⏸️ | Low | -- | init(w,h) accepts params; UI entry deferred |
| 0.8 | Parameter labels + units | ✅ | Low | 0.1 | Slider labels with px/s, px, %, s, /s, x etc. |

---

## Phase 1: Individual Life Systems

| ID | Task | Status | Priority | Depends On | Notes |
|----|------|--------|----------|------------|-------|
| 1.1 | Age system + natural death | ✅ | -- | 0.2 | AgeSuf, AgeStage enum, age/ageStage arrays |
| 1.2 | Body/weight system | ✅ | -- | 0.2 | BodySuf, weight/killStreak/lastKillTime arrays |
| 1.3 | Fatigue system | ✅ | -- | 0.2 | FatigueSuf, fatigue array; accumulation + recovery |
| 1.4 | Gender dimorphism | ✅ | -- | 1.2 | GenderSuf, sex array; speed/color differences |
| 1.5 | Pregnancy + postnatal recovery | ✅ | -- | 0.2 | PregnancySuf, lastBirthTime array |
| 1.6 | Predation multi-factor formula | ✅ | -- | 1.2, 1.3 | PredationSuf; age/fatigue/size/sex/hunger factors |
| 1.7 | Hit/dodge/damage system | ✅ | -- | 1.2, 1.3 | HealthSuf, health array; dodge+damage instead of instant kill |

---

## Phase 2: Social Dynamics

| ID | Task | Status | Priority | Depends On | Notes |
|----|------|--------|----------|------------|-------|
| 2.1 | Male combat | ✅ | -- | Phase 1 | CombatSuf; same-flock male fights; fatigue penalty |
| 2.2 | Hatred/enmity system | ✅ | -- | 0.2 | HatredSuf; hatredTarget/Level arrays; flee boost |
| 2.3 | Escape strategy selection | ✅ | -- | 0.2 | EscapeSuf; Zigzag/GroupFlee/CoverFlee strategies |
| 2.4 | Defensive cooperation | ✅ | -- | 2.2 | DefenseSuf; mobbing counter-attack on kill |
| 2.5 | Cohesion dynamics | ✅ | -- | Phase 1 | CohesionDynSuf; threat/hunger/density modulation |
| 2.6 | Carrying capacity | ✅ | -- | Phase 1 | Food-availability hunger pressure in stepHunger |

---

## Phase 3: Ecological Expansion

| ID | Task | Status | Priority | Depends On | Notes |
|----|------|--------|----------|------------|-------|
| 3.1a | Nest infrastructure | ✅ | -- | Phase 0 | NestData.h SoA, rendering triangles, defense calculation |
| 3.1b | Nest function (regen + food storage + reproduction) | ✅ | -- | 3.1a | Health boost, food deposit, nest-based spawning |
| 3.1c | Nest contest + day/night cycle | ✅ | -- | 3.1b | Ownership transfer, ambientLight uniform |
| 3.2 | Memory system | ✅ | -- | 0.2 | 6 event types, ring buffer, exponential decay, 5 integration points |
| 3.4 | Mouse disturbance + disturbance sources | ✅ | -- | Phase 0 | DISTURB_REPEL/ATTRACT; 150px radius; linear decay 3s; cap 32 sources |
| 3.5 | Sanity system | ✅ | -- | Phase 2, 3.2 | 3-state (RATIONAL/UNEASY/PANICKED), 4 decay factors, wander+cohesion+memory boosted |
| 3.6 | Programmatic terrain + day/night tint | ✅ | -- | 3.1c | Simplex noise + FBO pre-bake + color temp tint; desaturated 60% at x0.75 brightness for boid visibility |

---

## Phase 4+: Future Milestones (not prioritized)

| ID | Task | Status | Notes |
|----|------|--------|-------|
| 4.1 | Map & geography system | 🔮 | Climate + biome + noise map generation |
| 4.2 | Weather & disaster system | 🔮 | Independent large subsystem |
| 4.3 | Global energy pool | 🔮 | Energy quantification + audit visualization |
| 4.4 | 3D spatial extension | 🔮 | Long-term vision |
| 4.5 | Pheromone / waste marking | 🔮 | Spatial markers + diffusion |
| 4.6 | Terrain / water / obstacles | 🔮 | Pathfinding + environment interaction |

---

## Deferred Items

| Original ID | Reason |
|-------------|--------|
| Inbreeding penalty | Requires lineage graph; cost/benefit ratio too high |
| Territory/turf guarding | Requires dynamic spatial territory computation |
| Food discoverer advantage | Requires per-plant ownership tracking |
| Climate/weather/biome (module 9) | Independent large sub-system; defer until core complete |

---

## Auto-Sequencing Rules

After each task completion:
1. This document is updated: completed tasks marked ✅ with notes
2. Next highest-priority ❌ task is selected automatically
3. Priority is dynamic: HIGH > Medium > Low, resolved by dependency chain
4. If multiple tasks share priority, follow numerical order
5. User may override priority at any time

---

## Current Next Task

Phases 0-3 are fully complete. Only deferred items remain:

| ID | Task | Phase |
|----|------|-------|
| **0.7** | Map size configurable UI (deferred) | Phase 0 |

---

*Last updated: 2026-08-11 after terrain visibility improvement*
