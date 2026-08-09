#pragma once

#include <cstddef>
#include <functional>
#include <string>

enum class ScaleMode {
    Div10,      // floatVal = sliderVal / 10.0f
    Div100,     // floatVal = sliderVal / 100.0f
    Div1000,    // floatVal = sliderVal / 1000.0f
    OneToOne,   // floatVal = static_cast<float>(sliderVal)
    Multiply2,  // floatVal = sliderVal * 2.0f   (e.g. hard collision: slider 7 → 14 px)
};

struct ParamDef {
    const char* nameCN;
    const char* groupKey;
    size_t      fieldOffset;       // offsetof(FlockParams, field) or offsetof(PlantParams, field)
    int         sliderMin;
    int         sliderMax;
    ScaleMode   scale;
    int         initialSliderVal;
    bool        isPlantParam;      // true = write to PlantParams, false = write to FlockParams

    // Optional extra action when value changes (e.g. cohRadius -> updateGrid)
    std::function<void(float)> onChanged;

    // ---- Helpers ----

    // Number of decimal places for display (based on ScaleMode)
    int displayDecimals() const {
        switch (scale) {
        case ScaleMode::Div10:   return 1;
        case ScaleMode::Div100:  return 2;
        case ScaleMode::Div1000: return 3;
        default:                 return 0;  // OneToOne, Multiply2
        }
    }

    // Auto-derived single step in slider units (keyboard / arrow key granularity)
    int autoSingleStep() const {
        int range = sliderMax - sliderMin;
        if (range > 3000) return 50;
        if (range > 500)  return 10;
        if (range > 150)  return 5;
        return 1;
    }

    // Auto-derived page step (PgUp/PgDn granularity, ~1/10 of range)
    int autoPageStep() const {
        int range = sliderMax - sliderMin;
        int step = range / 10;
        return std::max(step, 1);
    }

    float sliderToFloat(int v) const {
        switch (scale) {
        case ScaleMode::Div10:   return static_cast<float>(v) / 10.0f;
        case ScaleMode::Div100:  return static_cast<float>(v) / 100.0f;
        case ScaleMode::Div1000: return static_cast<float>(v) / 1000.0f;
        case ScaleMode::Multiply2: return static_cast<float>(v) * 2.0f;
        case ScaleMode::OneToOne: return static_cast<float>(v);
        }
        return static_cast<float>(v);
    }

    int floatToSlider(float f) const {
        switch (scale) {
        case ScaleMode::Div10:   return static_cast<int>(f * 10.0f);
        case ScaleMode::Div100:  return static_cast<int>(f * 100.0f);
        case ScaleMode::Div1000: return static_cast<int>(f * 1000.0f);
        case ScaleMode::Multiply2: return static_cast<int>(f / 2.0f);
        case ScaleMode::OneToOne: return static_cast<int>(f);
        }
        return static_cast<int>(f);
    }

    // Read float value from a base struct via fieldOffset
    float readFrom(const void* base) const {
        const float* field = reinterpret_cast<const float*>(
            reinterpret_cast<const char*>(base) + fieldOffset);
        return *field;
    }

    // Write float value to a base struct via fieldOffset
    void writeTo(void* base, float val) const {
        float* field = reinterpret_cast<float*>(
            reinterpret_cast<char*>(base) + fieldOffset);
        *field = val;
    }
};
