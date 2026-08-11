#include "WorldSetupDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <cmath>
#include <algorithm>

// Climate zone data per zone
struct ClimateInfo {
    const char* nameCN;
    const char* nameEN;
    float tempMin;
    float tempMax;
    const char* precipLabel;
};

static const ClimateInfo CLIMATE_DATA[] = {
    { "\u6781\u5730 (Arctic)",         "Arctic",       -20.0f,  5.0f,  "\u4f4e\u964d\u6c34 (Low)" },
    { "\u4e9a\u5bd2\u5e26 (Subarctic)",  "Subarctic",    -5.0f, 15.0f,  "\u4e2d\u7b49 (Moderate)" },
    { "\u6e29\u5e26 (Temperate)",        "Temperate",     5.0f, 25.0f,  "\u4e2d\u7b49-\u5145\u6c9b (Moderate-High)" },
    { "\u4e9a\u70ed\u5e26 (Subtropical)","Subtropical",  15.0f, 35.0f,  "\u4f4e (Low)" },
    { "\u70ed\u5e26 (Tropical)",         "Tropical",     25.0f, 40.0f,  "\u9ad8 (High)" },
};

// Default terrain composition per climate zone (normalized ratios)
static const TerrainComposition ZONE_DEFAULTS[] = {
    // water, forest, grassland, desert, tundra, mountain, wetland
    { 0.40f, 0.00f, 0.00f, 0.00f, 0.50f, 0.10f, 0.00f },  // Arctic
    { 0.25f, 0.30f, 0.15f, 0.00f, 0.20f, 0.05f, 0.05f },  // Subarctic
    { 0.20f, 0.30f, 0.30f, 0.05f, 0.00f, 0.10f, 0.05f },  // Temperate
    { 0.15f, 0.20f, 0.35f, 0.15f, 0.00f, 0.10f, 0.05f },  // Subtropical
    { 0.10f, 0.40f, 0.20f, 0.05f, 0.00f, 0.05f, 0.20f },  // Tropical
};

WorldSetupDialog::WorldSetupDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("\u4e16\u754c\u914d\u7f6e (World Configuration)");
    setMinimumSize(520, 620);
    setModal(true);
    setupUI();

    // Initialize with default latitude (45N = temperate)
    m_latSlider->setValue(45);
    onLatitudeChanged(45);
}

void WorldSetupDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // ---- Map Size ----
    {
        auto* group = new QGroupBox("\u5730\u56fe\u5c3a\u5bf8 (Map Size)");
        auto* lay = new QVBoxLayout(group);
        lay->setSpacing(4);

        auto* presetRow = new QHBoxLayout();
        auto* presetLabel = new QLabel("\u9884\u8bbe (Preset):");
        presetLabel->setStyleSheet("color: #888; font-size: 11px;");
        presetRow->addWidget(presetLabel);
        m_presetCombo = new QComboBox();
        m_presetCombo->addItem("1920 x 1080 (HD)",       QSize(1920, 1080));
        m_presetCombo->addItem("2560 x 1440 (QHD)",      QSize(2560, 1440));
        m_presetCombo->addItem("3840 x 2160 (4K)",       QSize(3840, 2160));
        m_presetCombo->addItem("1280 x 720 (HD Ready)",  QSize(1280, 720));
        m_presetCombo->addItem("3000 x 3000 (Square Large)", QSize(3000, 3000));
        m_presetCombo->addItem("\u81ea\u5b9a\u4e49 (Custom)", QSize());
        presetRow->addWidget(m_presetCombo);
        lay->addLayout(presetRow);

        auto* dimRow = new QHBoxLayout();
        auto* wl = new QLabel("W:");
        wl->setStyleSheet("color: #888; font-size: 11px;");
        dimRow->addWidget(wl);
        m_mapWidth = new QSpinBox();
        m_mapWidth->setRange(640, 7680);
        m_mapWidth->setValue(1920);
        m_mapWidth->setSuffix(" px");
        m_mapWidth->setToolTip(QString("Valid range: %1 - %2 px").arg(m_mapWidth->minimum()).arg(m_mapWidth->maximum()));
        m_mapWidth->setKeyboardTracking(false);  // Only commit on Enter/focus loss
        dimRow->addWidget(m_mapWidth);
        auto* hl = new QLabel("H:");
        hl->setStyleSheet("color: #888; font-size: 11px;");
        dimRow->addWidget(hl);
        m_mapHeight = new QSpinBox();
        m_mapHeight->setRange(480, 4320);
        m_mapHeight->setValue(1080);
        m_mapHeight->setSuffix(" px");
        m_mapHeight->setToolTip(QString("Valid range: %1 - %2 px").arg(m_mapHeight->minimum()).arg(m_mapHeight->maximum()));
        m_mapHeight->setKeyboardTracking(false);  // Only commit on Enter/focus loss
        dimRow->addWidget(m_mapHeight);
        lay->addLayout(dimRow);

        connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &WorldSetupDialog::onPresetChanged);

        // When user manually edits, switch preset to Custom
        auto onManualEdit = [this]() {
            m_presetCombo->blockSignals(true);
            m_presetCombo->setCurrentIndex(m_presetCombo->count() - 1);  // "Custom" is last
            m_presetCombo->blockSignals(false);
        };
        connect(m_mapWidth,  QOverload<int>::of(&QSpinBox::valueChanged), this, onManualEdit);
        connect(m_mapHeight, QOverload<int>::of(&QSpinBox::valueChanged), this, onManualEdit);

        mainLayout->addWidget(group);
    }

    // ---- Geographic Location ----
    {
        auto* group = new QGroupBox("\u5730\u7406\u4f4d\u7f6e (Geographic Location)");
        auto* lay = new QVBoxLayout(group);
        lay->setSpacing(4);

        // Latitude slider (-90 to 90)
        auto* latRow = new QHBoxLayout();
        auto* latLabel = new QLabel("\u7eac\u5ea6 (Latitude):");
        latLabel->setStyleSheet("color: #888; font-size: 11px;");
        latRow->addWidget(latLabel);
        m_latSlider = new QSlider(Qt::Horizontal);
        m_latSlider->setRange(-90, 90);
        m_latSlider->setValue(45);
        m_latSlider->setTickPosition(QSlider::TicksBelow);
        m_latSlider->setTickInterval(15);
        latRow->addWidget(m_latSlider);
        m_latValueLabel = new QLabel("45\u00b0N");
        m_latValueLabel->setStyleSheet("color: #ccc; font-size: 11px; font-weight: bold; min-width: 36px;");
        latRow->addWidget(m_latValueLabel);
        lay->addLayout(latRow);

        // Climate info display
        auto* climateRow = new QHBoxLayout();
        auto* cLabel = new QLabel("\u6c14\u5019 (Climate):");
        cLabel->setStyleSheet("color: #888; font-size: 11px;");
        climateRow->addWidget(cLabel);
        m_climateLabel = new QLabel();
        m_climateLabel->setStyleSheet("color: #ccc; font-size: 11px; font-weight: bold;");
        climateRow->addWidget(m_climateLabel);
        climateRow->addStretch();
        auto* tLabel = new QLabel("\u6e29\u5ea6 (Temp):");
        tLabel->setStyleSheet("color: #888; font-size: 11px;");
        climateRow->addWidget(tLabel);
        m_tempLabel = new QLabel();
        m_tempLabel->setStyleSheet("color: #ccc; font-size: 11px;");
        climateRow->addWidget(m_tempLabel);
        lay->addLayout(climateRow);

        connect(m_latSlider, &QSlider::valueChanged, this, &WorldSetupDialog::onLatitudeChanged);

        mainLayout->addWidget(group);
    }

    // ---- Terrain Composition ----
    {
        auto* group = new QGroupBox("\u5730\u8c8c\u5143\u7d20\u5360\u6bd4 (Terrain Composition %)");
        auto* lay = new QVBoxLayout(group);
        lay->setSpacing(3);

        QLabel* dummy = nullptr;

        // Element name, key index, initial value
        struct ElemDef {
            const char* name;
            int keyIdx;   // index in TerrainComposition fields
            float init;
        };
        static const ElemDef elems[] = {
            { "\u6c34\u57df (Water)",         0, 0.20f },
            { "\u68ee\u6797 (Forest)",        1, 0.30f },
            { "\u8349\u539f (Grassland)",     2, 0.30f },
            { "\u6c99\u6f20 (Desert)",        3, 0.05f },
            { "\u51bb\u571f (Tundra)",        4, 0.00f },
            { "\u5c71\u5730 (Mountain)",      5, 0.10f },
            { "\u6e7f\u5730 (Wetland)",       6, 0.05f },
        };

        for (int i = 0; i < 7; ++i) {
            QLabel* valLabel = nullptr;
            QSlider* sl = addElementRow(group, lay, elems[i].name, elems[i].init, valLabel);
            m_elemRows[i].slider = sl;
            m_elemRows[i].label = valLabel;

            // When slider changes, update composition struct
            connect(sl, &QSlider::valueChanged, this, [this, i](int v) {
                float val = v / 100.0f;
                switch (i) {
                case 0: m_composition.water     = val; break;
                case 1: m_composition.forest    = val; break;
                case 2: m_composition.grassland = val; break;
                case 3: m_composition.desert    = val; break;
                case 4: m_composition.tundra    = val; break;
                case 5: m_composition.mountain  = val; break;
                case 6: m_composition.wetland   = val; break;
                }
            });
        }

        mainLayout->addWidget(group);
    }

    // ---- Boundary Mode ----
    {
        auto* group = new QGroupBox("\u8fb9\u754c\u6a21\u5f0f (Boundary Mode)");
        auto* lay = new QHBoxLayout(group);
        auto* bl = new QLabel("\u6a21\u5f0f (Mode):");
        bl->setStyleSheet("color: #888; font-size: 11px;");
        lay->addWidget(bl);
        m_boundaryCombo = new QComboBox();
        m_boundaryCombo->addItem("\u6df7\u5408 (Hybrid - Soft + Wrap)",  3);  // BoundaryMode::Hybrid = 3
        m_boundaryCombo->addItem("\u73af\u8fb9 (Torus - Wrap only)",      0);  // BoundaryMode::Torus = 0
        m_boundaryCombo->addItem("\u8f6f\u5899 (SoftWall - Repel only)",  1);  // BoundaryMode::SoftWall = 1
        m_boundaryCombo->addItem("\u786c\u5899 (HardWall - Bounce)",      2);  // BoundaryMode::HardWall = 2
        m_boundaryCombo->setCurrentIndex(0);
        lay->addWidget(m_boundaryCombo);
        mainLayout->addWidget(group);
    }

    mainLayout->addStretch();

    // ---- Separator + OK / Cancel ----
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #4a4a4a;");
    mainLayout->addWidget(sep);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnBox->button(QDialogButtonBox::Ok)->setText("\u786e\u5b9a (OK)");
    btnBox->button(QDialogButtonBox::Cancel)->setText("\u53d6\u6d88 (Cancel)");
    btnBox->setStyleSheet(
        "QPushButton { padding: 4px 16px; font-size: 12px; }"
        "QPushButton:first-child { background: #4a6a4a; color: #ddd; }");
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);
}

QSlider* WorldSetupDialog::addElementRow(QGroupBox* /*group*/, QVBoxLayout* layout,
                                         const char* name, float initialPct,
                                         QLabel*& outLabel)
{
    auto* row = new QHBoxLayout();
    auto* nameLabel = new QLabel(QString::fromUtf8(name));
    nameLabel->setStyleSheet("color: #888; font-size: 11px; min-width: 90px;");
    row->addWidget(nameLabel);

    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 100);
    slider->setValue(static_cast<int>(initialPct * 100));
    slider->setFixedHeight(18);
    row->addWidget(slider);

    outLabel = new QLabel(QString("%1%").arg(static_cast<int>(initialPct * 100)));
    outLabel->setStyleSheet("color: #ccc; font-size: 11px; font-weight: bold; min-width: 36px;");
    row->addWidget(outLabel);

    connect(slider, &QSlider::valueChanged, this, [outLabel](int v) {
        outLabel->setText(QString("%1%").arg(v));
    });

    layout->addLayout(row);
    return slider;
}

void WorldSetupDialog::onPresetChanged(int /*idx*/)
{
    QSize sz = m_presetCombo->currentData().toSize();
    if (sz.isValid()) {
        m_mapWidth->setValue(sz.width());
        m_mapHeight->setValue(sz.height());
    }
}

void WorldSetupDialog::onLatitudeChanged(int latDeg)
{
    float lat = static_cast<float>(latDeg);
    const char* northSouth = (lat >= 0) ? "N" : "S";
    m_latValueLabel->setText(QString("%1\u00b0%2").arg(std::abs(latDeg)).arg(northSouth));

    updateClimateDisplay();
    applyLatitudeDefaults(lat);
}

void WorldSetupDialog::updateClimateDisplay()
{
    ClimateZone zone = climateZone();
    const auto& info = CLIMATE_DATA[static_cast<int>(zone)];
    m_climateLabel->setText(QString("%1 (%2)").arg(info.nameCN).arg(info.nameEN));
    m_tempLabel->setText(QString("%1 ~ %2\u00b0C").arg(info.tempMin, 0, 'f', 0).arg(info.tempMax, 0, 'f', 0));
}

void WorldSetupDialog::applyLatitudeDefaults(float lat)
{
    ClimateZone zone = climateZone();
    const auto& def = ZONE_DEFAULTS[static_cast<int>(zone)];
    m_composition = def;

    // Update sliders (block signals to avoid re-triggering)
    auto updateSlider = [](QSlider* sl, QLabel* lbl, float val) {
        sl->blockSignals(true);
        sl->setValue(static_cast<int>(val * 100));
        sl->blockSignals(false);
        lbl->setText(QString("%1%").arg(static_cast<int>(val * 100)));
    };

    updateSlider(m_elemRows[0].slider, m_elemRows[0].label, def.water);
    updateSlider(m_elemRows[1].slider, m_elemRows[1].label, def.forest);
    updateSlider(m_elemRows[2].slider, m_elemRows[2].label, def.grassland);
    updateSlider(m_elemRows[3].slider, m_elemRows[3].label, def.desert);
    updateSlider(m_elemRows[4].slider, m_elemRows[4].label, def.tundra);
    updateSlider(m_elemRows[5].slider, m_elemRows[5].label, def.mountain);
    updateSlider(m_elemRows[6].slider, m_elemRows[6].label, def.wetland);
}

ClimateZone WorldSetupDialog::climateZone() const
{
    float absLat = std::abs(latitude());
    if (absLat >= 60.0f)  return ClimateZone::Arctic;
    if (absLat >= 45.0f)  return ClimateZone::Subarctic;
    if (absLat >= 30.0f)  return ClimateZone::Temperate;
    if (absLat >= 15.0f)  return ClimateZone::Subtropical;
    return ClimateZone::Tropical;
}

int WorldSetupDialog::boundaryMode() const
{
    return m_boundaryCombo->currentData().toInt();
}
