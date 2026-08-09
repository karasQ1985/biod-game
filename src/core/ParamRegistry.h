#pragma once

#include "ParamDef.h"

#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <QObject>

#include <map>
#include <string>
#include <vector>

// UI binding state per registered parameter
struct ParamBinding {
    const ParamDef* def = nullptr;
    QSlider* slider = nullptr;
    QLabel*  label = nullptr;       // parameter name label
    QLabel*  valueLabel = nullptr;  // current value display (persistent, right of name)
};

// Central registry that owns all parameter slider widgets and bindings.
// Adding a new parameter = one reg() call. No more per-slider members or slots.
class ParamRegistry {
public:
    // Explicit construction required for parent-child ownership chain
    explicit ParamRegistry() = default;

    // Register a parameter definition. Returns index for optional chaining.
    int reg(const ParamDef& def);

    // Build a QGroupBox containing all sliders registered under groupKey.
    // The returned QGroupBox* is owned by parent (caller stores if needed for title updates).
    // initialBaseFlock/initialBasePlant point to the source struct for initial slider values.
    QGroupBox* buildGroup(const char* groupKey,
                          const QString& groupStyle,
                          const void* initialBaseFlock,
                          const void* initialBasePlant,
                          QWidget* parent);

    // Connect all registered sliders to a callback lambda.
    // The lambda receives (const ParamDef* def, const void* flockBase, const void* plantBase, float val).
    // flockBase and plantBase are captured externally (mainly for write-back).
    template<typename Func>
    void connectAll(Func callback);

    // Refresh all slider values from current FlockParams and PlantParams.
    void refresh(const void* flockBase, const void* plantBase);

    // Get the GroupBox for a given groupKey (used for title updates in updateFlockButtons).
    // May return nullptr if the group has no sliders (e.g. appearance, relations).
    QGroupBox* groupBox(const char* groupKey) const;

    // Convenience: update group box title with new zh/en text, plus style refresh.
    static void setGroupTitle(QGroupBox* box, const QString& title);

private:
    std::vector<ParamDef> m_defs;
    std::vector<ParamBinding> m_bindings;          // indexed same as m_defs
    std::map<std::string, QGroupBox*> m_groups;    // keyed by groupKey
};

// ---- Template implementation (must be in header) ----

template<typename Func>
void ParamRegistry::connectAll(Func callback) {
    for (auto& b : m_bindings) {
        // BUG FIX: capture def* by value (not reference) so it stays valid after this loop
        // The ParamDef* points into m_defs which outlives all callbacks.
        const ParamDef* def = b.def;
        QObject::connect(b.slider, &QSlider::valueChanged, [callback, def](int val) {
            callback(def, def->sliderToFloat(val));
        });
    }
}
