#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <vector>

// Per-element terrain composition ratio (normalized to 0.0 - 1.0)
struct TerrainComposition {
    float water    = 0.20f;
    float forest   = 0.30f;
    float grassland = 0.30f;
    float desert   = 0.05f;
    float tundra   = 0.00f;
    float mountain = 0.10f;
    float wetland  = 0.05f;
};

// Climate zone derived from latitude
enum class ClimateZone : int {
    Arctic     = 0,
    Subarctic  = 1,
    Temperate  = 2,
    Subtropical = 3,
    Tropical   = 4
};

// Boundary mode selection (maps directly to BoundaryMode in Simulation.h)

class WorldSetupDialog : public QDialog {
    Q_OBJECT

public:
    explicit WorldSetupDialog(QWidget* parent = nullptr);

    // Results after user accepts
    int mapWidth()  const { return m_mapWidth->value(); }
    int mapHeight() const { return m_mapHeight->value(); }
    float latitude() const { return static_cast<float>(m_latSlider->value()); }
    ClimateZone climateZone() const;
    const TerrainComposition& composition() const { return m_composition; }
    int boundaryMode() const;

private slots:
    void onPresetChanged(int idx);
    void onLatitudeChanged(int latDeg);

private:
    void setupUI();
    void applyLatitudeDefaults(float lat);
    void updateClimateDisplay();
    void syncSliderToLabel(QSlider* slider, QLabel* label, const char* suffix);

    // Helper to create a terrain element slider row
    QSlider* addElementRow(QGroupBox* group, QVBoxLayout* layout,
                           const char* name, float initialPct,
                           QLabel*& outLabel);

    // Map size
    QComboBox* m_presetCombo = nullptr;
    QSpinBox*  m_mapWidth = nullptr;
    QSpinBox*  m_mapHeight = nullptr;

    // Latitude
    QSlider* m_latSlider = nullptr;
    QLabel*  m_latValueLabel = nullptr;
    QLabel*  m_climateLabel = nullptr;
    QLabel*  m_tempLabel = nullptr;

    // Terrain composition
    TerrainComposition m_composition;

    // Element sliders + value labels (7 elements)
    struct ElemRow {
        QSlider* slider;
        QLabel*  label;
    };
    ElemRow m_elemRows[7];

    // Boundary mode
    QComboBox* m_boundaryCombo = nullptr;
};
