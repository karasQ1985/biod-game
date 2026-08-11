#pragma once

// ============================================================================
// WorldConstants  --  simulation physical quantity semantics (Phase 0.1)
//
// Purpose:
//   Centralize the physical meaning of all simulation parameters.
//   No existing computation logic is changed -- these constants document
//   the implicit unit system and provide anchored baselines for future
//   features (day/night cycle, temperature, energy audit).
//
// Design rule:
//   All internal computation uses PIXELS and REAL SECONDS directly.
//   Conversion functions are for display/interpretation only.
//
// Scale summary:
//   Space:  1 real meter  = 5 pixels  (world 1920x1080 px = 384x216 m)
//   Time:   simTime in real seconds; day/night cycle via SECONDS_PER_SIM_DAY
//   Mass:   1.0 weight unit ~ 10 kg (species average)
//   Energy: 1.0 hunger unit ~ 2000 kcal (full stomach)
//   Temp:   default 20 C (temperate baseline)
// ============================================================================

namespace WorldConst {

// ---- Space ----
constexpr float PX_PER_M = 5.0f;
constexpr float M_PER_PX = 1.0f / PX_PER_M;

// ---- Time ----
// Day/night cycle period in real seconds (Phase 3.3).
// One full day-night oscillation = SECONDS_PER_SIM_DAY seconds.
// Adjustable at runtime for 24h realism or fast-forward observation.
constexpr float SECONDS_PER_SIM_DAY = 30.0f;

// ---- Mass / Weight ----
// Body weight is normalized around 1.0 = species average mass.
// For interpretive display: 1.0 weight unit ~ 10 kg.
// Range [0.5, 2.0] maps roughly to [5 kg, 20 kg].
constexpr float KG_PER_WEIGHT = 10.0f;

// ---- Energy / Hunger ----
// Hunger [0.0, 1.0] is stomach fill fraction.
// 1.0 hunger level ~ 2000 kcal of metabolic energy.
constexpr float KCAL_PER_FULL_HUNGER = 2000.0f;

// ---- Environment ----
// Default ambient temperature (Celsius) for ecology calculations.
constexpr float DEFAULT_TEMP_C = 20.0f;

// ---- Health ----
// Health [0.0, 1.0] is vitality fraction. No real-world mapping needed.

// ---- Time acceleration (future) ----
// Global simulation speed multiplier. 1.0 = real-time.
constexpr float DEFAULT_TIME_SCALE = 1.0f;

// ---- Conversion helpers ----
constexpr float pxToMeters(float px) noexcept {
    return px * M_PER_PX;
}

constexpr float metersToPx(float m) noexcept {
    return m * PX_PER_M;
}

constexpr float pxPerSecToKmh(float pxPerSec) noexcept {
    return pxPerSec * M_PER_PX * 3.6f;
}

constexpr float weightToKg(float w) noexcept {
    return w * KG_PER_WEIGHT;
}

constexpr float kgToWeight(float kg) noexcept {
    return kg / KG_PER_WEIGHT;
}

constexpr float hungerToKcal(float h) noexcept {
    return h * KCAL_PER_FULL_HUNGER;
}

constexpr float kcalToHunger(float kcal) noexcept {
    return kcal / KCAL_PER_FULL_HUNGER;
}

} // namespace WorldConst
