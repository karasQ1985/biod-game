#include "MainWindow.h"
#include "GLWidget.h"
#include "simulation/Simulation.h"
#include <QToolBar>
#include <QDockWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QStatusBar>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QScrollArea>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMenu>
#include <QColorDialog>
#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QKeySequence>
#include <QFileDialog>
#include <QApplication>

// ---- Parameter registration macros ----
#define HO(field) offsetof(FlockParams, field)
#define PO(field) offsetof(PlantParams, field)

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Biod - Flock Simulator");
    resize(1280, 800);

    m_glWidget = new GLWidget(this);
    setCentralWidget(m_glWidget);

    setupUI();
    setupToolbar();

    auto* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::refreshStatsPanel);
    statusTimer->start(250);

    m_logPath = QDir::tempPath() + "/biod_perf_log.csv";
    {
        QFile f(m_logPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << "Frame,dt_us,Boid_Count\n";
        }
    }
}

void MainWindow::setupToolbar()
{
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setStyleSheet(
        "QToolBar { background: #2b2b2b; border-bottom: 1px solid #4a4a4a; padding: 2px; spacing: 4px; }"
        "QPushButton { background: #3a3a3a; color: #ddd; border: 1px solid #555; "
        "  border-radius: 2px; padding: 2px 6px; font-size: 11px; }"
        "QPushButton:hover { background: #555; }"
        "QComboBox { background: #3a3a3a; color: #ddd; border: 1px solid #555; padding: 1px; font-size: 11px; }"
        "QSpinBox { background: #3a3a3a; color: #ddd; border: 1px solid #555; padding: 1px 3px; font-size: 11px; }");

    m_addFlockBtn = new QPushButton("[+]");
    m_addFlockBtn->setToolTip("Add a new flock");
    m_addFlockBtn->setShortcut(QKeySequence("Ctrl+N"));
    m_removeFlockBtn = new QPushButton("[-]");
    m_removeFlockBtn->setToolTip("Remove current flock");
    toolbar->addWidget(m_addFlockBtn);
    toolbar->addWidget(m_removeFlockBtn);
    connect(m_addFlockBtn, &QPushButton::clicked, this, &MainWindow::onAddFlock);
    connect(m_removeFlockBtn, &QPushButton::clicked, this, &MainWindow::onRemoveFlock);

    toolbar->addSeparator();

    // ---- Flock: dropdown selector ----
    auto* flockLabel = new QLabel("Flock:");
    flockLabel->setStyleSheet("color: #aaa; font-size: 11px; padding: 0 4px;");
    toolbar->addWidget(flockLabel);

    m_flockCombo = new QComboBox();
    m_flockCombo->setFixedWidth(120);
    m_flockCombo->setToolTip("Select flock to edit");
    toolbar->addWidget(m_flockCombo);

    connect(m_flockCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx < 0) return;
        if (m_flockCombo->signalsBlocked()) return;
        onFlockButton(idx);
    });

    updateFlockCombo();

    // ---- Inline controls: Rename + Color ----
    m_renameBtn = new QPushButton("Rename");
    m_renameBtn->setToolTip("Rename current flock");
    m_renameBtn->setMaximumWidth(65);
    m_renameBtn->setStyleSheet("font-size: 11px; padding: 1px 4px;");
    connect(m_renameBtn, &QPushButton::clicked, this, [this]() {
        int af = m_glWidget->simulation().activeFlock();
        onRenameFlock(af);
    });
    toolbar->addWidget(m_renameBtn);

    m_toolbarColorBtn = new QPushButton();
    m_toolbarColorBtn->setFixedSize(24, 22);
    m_toolbarColorBtn->setToolTip("Change flock color");
    connect(m_toolbarColorBtn, &QPushButton::clicked, this, [this]() {
        int af = m_glWidget->simulation().activeFlock();
        onChangeFlockColor(af);
    });
    toolbar->addWidget(m_toolbarColorBtn);

    toolbar->addSeparator();

    // ---- Boid count + spawn/remove ----
    m_countSpin = new QSpinBox();
    m_countSpin->setRange(1, 5000);
    m_countSpin->setValue(500);
    m_countSpin->setSuffix(" \u4e2a");
    m_countSpin->setToolTip("\u81ea\u5b9a\u4e49\u589e\u51cf\u6570\u91cf");
    toolbar->addWidget(m_countSpin);

    m_spawnBtn = new QPushButton("+ \u751f\u6210");
    m_removeBtn = new QPushButton("- \u79fb\u9664");
    toolbar->addWidget(m_spawnBtn);
    toolbar->addWidget(m_removeBtn);

    connect(m_spawnBtn, &QPushButton::clicked, this, &MainWindow::onSpawn);
    connect(m_removeBtn, &QPushButton::clicked, this, &MainWindow::onRemove);

    toolbar->addSeparator();

    m_clearBtn = new QPushButton("Clear");
    toolbar->addWidget(m_clearBtn);
    connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::onClearAll);

    updateFlockButtons();
}

void MainWindow::updateFlockCombo()
{
    if (!m_flockCombo) return;
    auto& sim = m_glWidget->simulation();
    int n = sim.flockCount();
    int active = sim.activeFlock();

    m_flockCombo->blockSignals(true);
    m_flockCombo->clear();
    for (int i = 0; i < n; ++i) {
        m_flockCombo->addItem(QString::fromStdString(sim.flockName(i)));
    }
    if (active >= 0 && active < n)
        m_flockCombo->setCurrentIndex(active);
    m_flockCombo->blockSignals(false);

    if (m_toolbarColorBtn) {
        const float* fc = sim.flockColor(active);
        m_toolbarColorBtn->setStyleSheet(
            QString("background-color: rgb(%1,%2,%3); border: 1px solid #666;")
                .arg(static_cast<int>(fc[0] * 255))
                .arg(static_cast<int>(fc[1] * 255))
                .arg(static_cast<int>(fc[2] * 255)));
    }
}

void MainWindow::setupUI()
{
    auto* scroll = new QScrollArea(this);
    auto* dock = new QWidget(scroll);
    auto* mainLayout = new QVBoxLayout(dock);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);

    auto& sim = m_glWidget->simulation();
    auto& p = sim.params();
    auto& pp = sim.plantParams();

    // Active flock indicator
    m_flockLabel = new QLabel("\u5f53\u524d\u7fa4\u4f53: Flock A");
    m_flockLabel->setStyleSheet("font-weight: bold; color: #aaa; padding: 4px;");
    mainLayout->addWidget(m_flockLabel);

    // Common Win10-native group box style
    const QString groupStyle =
        "QGroupBox { font-weight: bold; color: #000000; "
        "border: 1px solid #4a4a4a; border-radius: 3px; margin-top: 7px; "
        "padding-top: 10px; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }";

    auto& reg = m_params;

    // ================================================================
    // Register all FlockParams parameters
    // ================================================================

    // Group: Hunger & Satiety
    reg.reg({"\u8870\u51cf\u901f\u7387 (Decay Rate)",       "hunger",   HO(hungerDecayRate),       0,   100, ScaleMode::Div1000,   static_cast<int>(p.hungerDecayRate * 1000),   false});
    reg.reg({"\u9971\u8179\u901f\u5ea6 (Speed Min)",         "hunger",   HO(hungerSpeedMin),        30,  100, ScaleMode::Div100,    static_cast<int>(p.hungerSpeedMin * 100),     false});
    reg.reg({"\u9965\u997f\u901f\u5ea6 (Speed Max)",         "hunger",   HO(hungerSpeedMax),        100, 250, ScaleMode::Div100,    static_cast<int>(p.hungerSpeedMax * 100),     false});
    reg.reg({"\u95ea\u70c1\u9608\u503c (Flash Threshold)",   "hunger",   HO(hungerFlashThreshold),  5,   80,  ScaleMode::Div100,    static_cast<int>(p.hungerFlashThreshold * 100), false});

    // Group: Predation & Escape
    reg.reg({"\u6355\u730e\u6210\u529f\u7387 (Chase Success)",          "predation", HO(chaseSuccessBase),             0,   100, ScaleMode::Div100,    static_cast<int>(p.chaseSuccessBase * 100),              false});
    reg.reg({"\u9003\u79bb\u6210\u529f\u7387 (Escape Success)",         "predation", HO(escapeSuccessBase),            0,   100, ScaleMode::Div100,    static_cast<int>(p.escapeSuccessBase * 100),             false});
    reg.reg({"\u6355\u730e\u8303\u56f4 (Chase Range)",                  "predation", HO(chaseRange),                   5,   100, ScaleMode::OneToOne,  static_cast<int>(p.chaseRange),                           false});
    reg.reg({"\u730e\u6740\u9965\u997f\u9608\u503c (Hunt Hunger)",       "predation", HO(predationMinHunger),           5,   95,  ScaleMode::Div100,    static_cast<int>(p.predationMinHunger * 100),            false});
    reg.reg({"\u51fb\u6740\u9965\u997f\u9608\u503c (Kill Hunger)",       "predation", HO(predationKillHunger),          1,   50,  ScaleMode::Div100,    static_cast<int>(p.predationKillHunger * 100),           false});
    reg.reg({"\u9971\u8179\u53c2\u4e0e\u7387% (Participation %)",      "predation", HO(predationParticipationRate),   1,   100, ScaleMode::Div100,    static_cast<int>(p.predationParticipationRate * 100),    false});
    reg.reg({"\u8fde\u6740\u589e\u91cd (Kill Weight Gain)",     "predation", HO(weightGainPerKill),          1,   200, ScaleMode::Div1000,   static_cast<int>(p.weightGainPerKill * 1000),            false});
    reg.reg({"\u95f2\u7f6e\u8870\u51cf/\u79d2 (Decay Rate/s)",   "predation", HO(weightDecayRate),            1,   20,  ScaleMode::Div1000,   static_cast<int>(p.weightDecayRate * 1000),              false});
    reg.reg({"\u8fde\u6740\u8d85\u65f6/\u79d2 (Streak Timeout)", "predation", HO(streakTimeout),              5,   100, ScaleMode::Div10,     static_cast<int>(p.streakTimeout * 10),                  false});
    reg.reg({"\u8870\u51cf\u5ef6\u8fdf/\u79d2 (Decay Delay)",    "predation", HO(decayDelay),                 1,   30,  ScaleMode::OneToOne,  static_cast<int>(p.decayDelay),                          false});
    reg.reg({"\u6700\u5927\u4f53\u91cd (Max Weight)",            "predation", HO(maxWeight),                 10,  50,  ScaleMode::Div10,     static_cast<int>(p.maxWeight * 10),                      false});
    reg.reg({"\u6700\u5c0f\u4f53\u91cd (Min Weight)",            "predation", HO(minWeight),                  1,   20,  ScaleMode::Div10,     static_cast<int>(p.minWeight * 10),                      false});

    // Group: Inter-Flock
    reg.reg({"\u7fa4\u95f4\u65a5\u529b (Inter-Flock Repulsion)", "interflock", HO(interFlockRepulsionWeight), 0, 50, ScaleMode::Div10, static_cast<int>(p.interFlockRepulsionWeight * 10), false});
    reg.reg({"\u6355\u98df\u5438\u5f15\u529b (Predator Attraction)","interflock", HO(predatorAttractionWeight), 0, 50, ScaleMode::Div10, static_cast<int>(p.predatorAttractionWeight * 10), false});
    reg.reg({"\u730e\u7269\u6050\u60e7 (Prey Fear)",            "interflock", HO(preyFearWeight),             0, 50, ScaleMode::Div10, static_cast<int>(p.preyFearWeight * 10),         false});

    // Group: Reynolds
    reg.reg({"\u5206\u79bb (Separation)", "reynolds", HO(separationWeight), 0, 50, ScaleMode::Div10, static_cast<int>(p.separationWeight * 10), false});
    reg.reg({"\u5bf9\u9f50 (Alignment)",  "reynolds", HO(alignmentWeight),  0, 50, ScaleMode::Div10, static_cast<int>(p.alignmentWeight * 10),  false});
    reg.reg({"\u805a\u96c6 (Cohesion)",   "reynolds", HO(cohesionWeight),   0, 50, ScaleMode::Div10, static_cast<int>(p.cohesionWeight * 10),   false});

    // ---- Group: Movement & Collision (Max Speed, Weight, Hard Collision) ----
    reg.reg({"\u6700\u5927\u901f\u5ea6 (Max Speed)", "movement", HO(maxSpeed), 50, 600, ScaleMode::OneToOne, static_cast<int>(p.maxSpeed), false});
    reg.reg({"\u4f53\u91cd\u901f\u5ea6\u60e9\u7f5a (Weight Penalty)", "movement", HO(weightSpeedPenalty), 0, 200, ScaleMode::Div100, static_cast<int>(p.weightSpeedPenalty * 100), false});
    reg.reg({"\u786c\u78b0\u649e\u8ddd\u79bb (Hard Collision)", "movement", HO(hardCollisionRadius), 0, 50, ScaleMode::Multiply2, static_cast<int>(p.hardCollisionRadius / 2.0f), false});

    // Group: Perception
    reg.reg({"\u5206\u79bb\u534a\u5f84 (Sep Radius)", "perception", HO(separationRadius), 5,  100, ScaleMode::OneToOne, static_cast<int>(p.separationRadius), false});
    reg.reg({"\u5bf9\u9f50\u534a\u5f84 (Ali Radius)", "perception", HO(alignmentRadius),  10, 200, ScaleMode::OneToOne, static_cast<int>(p.alignmentRadius),  false});
    reg.reg({"\u805a\u96c6\u534a\u5f84 (Coh Radius)", "perception", HO(cohesionRadius),   10, 200, ScaleMode::OneToOne, static_cast<int>(p.cohesionRadius),   false,
             [this](float) { m_glWidget->simulation().updateGrid(); }});  // cohRadius triggers grid rebuild

    // Group: Boundary & Wander
    reg.reg({"\u8fb9\u754c\u89c4\u907f (Boundary Avoid)", "boundary", HO(boundaryWeight), 0,  50,  ScaleMode::Div10,    static_cast<int>(p.boundaryWeight * 10),  false});
    reg.reg({"\u8fb9\u754c\u8ddd\u79bb (Margin)",          "boundary", HO(boundaryMargin), 10, 300, ScaleMode::OneToOne,  static_cast<int>(p.boundaryMargin),        false});
    reg.reg({"\u968f\u673a\u6e38\u8361 (Wander)",           "boundary", HO(wanderWeight),   0,  30,  ScaleMode::Div10,    static_cast<int>(p.wanderWeight * 10),    false});

    // Group: Foraging
    reg.reg({"\u641c\u5bfb\u8303\u56f4 (Forage Range)",        "forage", HO(forageRange),            30,  400, ScaleMode::OneToOne,  static_cast<int>(p.forageRange),              false});
    reg.reg({"\u89c5\u98df\u529b\u5ea6 (Forage Weight)",       "forage", HO(forageWeight),           0,   50,  ScaleMode::Div10,    static_cast<int>(p.forageWeight * 10),        false});
    reg.reg({"\u9965\u997f\u9608\u503c (Hunger Threshold)",    "forage", HO(forageHungerThreshold),  10,  90,  ScaleMode::Div100,   static_cast<int>(p.forageHungerThreshold * 100), false});

    // Group: Reproduction
    reg.reg({"\u6700\u5c11\u540e\u4ee3 (Min Offspring)",  "repro", HO(reproductionMinOffspring), 1,  5,    ScaleMode::OneToOne,  static_cast<int>(p.reproductionMinOffspring),                  false});
    reg.reg({"\u6700\u591a\u540e\u4ee3 (Max Offspring)",  "repro", HO(reproductionMaxOffspring), 1,  10,   ScaleMode::OneToOne,  static_cast<int>(p.reproductionMaxOffspring),                  false});
    reg.reg({"\u7fa4\u4f53\u4e0a\u9650 (Max Flock Size)", "repro", HO(maxFlockSize),             50, 5000, ScaleMode::OneToOne,  p.maxFlockSize,                              false});
    reg.reg({"\u6700\u4f4e\u9971\u8179\u503c (Min Hunger)","repro",  HO(reproductionMinHunger),  40, 95,   ScaleMode::Div100,   static_cast<int>(p.reproductionMinHunger * 100), false});
    reg.reg({"\u7e41\u6b96\u95f4\u9694/\u79d2 (Interval)","repro",   HO(reproductionInterval),   30, 300,  ScaleMode::OneToOne,  static_cast<int>(p.reproductionInterval),       false});

    // ================================================================
    // Register all PlantParams parameters
    // ================================================================
    reg.reg({"\u6700\u5927\u6570\u91cf (Max Plants)",          "plants", PO(maxPlants),       10,  500, ScaleMode::OneToOne,  static_cast<int>(pp.maxPlants),  false});
    reg.reg({"\u8fdb\u98df\u8303\u56f4 (Eat Range)",           "plants", PO(eatRange),        5,   100, ScaleMode::OneToOne,  static_cast<int>(pp.eatRange),      true});
    reg.reg({"\u751f\u957f\u65f6\u95f4 (Growth Time)",         "plants", PO(growthTime),      10,  200, ScaleMode::OneToOne,  static_cast<int>(pp.growthTime),    true});
    reg.reg({"\u6269\u6563\u6982\u7387 (Spread Chance)",       "plants", PO(spreadChance),    0,   100, ScaleMode::Div1000,  static_cast<int>(pp.spreadChance * 1000), true});
    reg.reg({"\u6269\u6563\u8303\u56f4 (Spread Range)",        "plants", PO(spreadRange),     20,  300, ScaleMode::OneToOne,  static_cast<int>(pp.spreadRange),   true});
    reg.reg({"\u5b63\u8282\u957f\u5ea6 (Season Length)",       "plants", PO(seasonLength),    10,  300, ScaleMode::OneToOne,  static_cast<int>(pp.seasonLength),  true});
    reg.reg({"\u80a5\u6c83\u534a\u5f84 (Fertilize Radius)",    "plants", PO(fertilizeRadius), 20,  200, ScaleMode::OneToOne,  static_cast<int>(pp.fertilizeRadius), true});
    reg.reg({"\u80a5\u6c83\u589e\u5e45 (Fertilize Boost)",     "plants", PO(fertilizeBoost),  5,   80,  ScaleMode::Div100,   static_cast<int>(pp.fertilizeBoost * 100), true});

    // ================================================================
    // Build GroupBoxes via registry
    // ================================================================

    struct GroupInfo {
        const char* key;
        const char* title;
    };
    const GroupInfo groups[] = {
        {"hunger",      "\u9965\u997f\u4e0e\u9971\u8179 (Hunger & Satiety)"},
        {"predation",   "\u6355\u730e\u4e0e\u9003\u79bb (Predation & Escape)"},
        {"interflock",  "\u7fa4\u95f4\u884c\u4e3a (Inter-Flock)"},
        {"reynolds",    "Reynolds \u7fa4\u4f53\u89c4\u5219"},
        {"movement",    "\u79fb\u52a8\u4e0e\u78b0\u649e (Movement \u0026 Collision)"},
        {"perception",  "\u611f\u77e5\u534a\u5f84 (Perception)"},
        {"boundary",    "\u8fb9\u754c\u4e0e\u6e38\u8361 (Boundary & Wander)"},
    };

    for (auto& g : groups) {
        auto* box = reg.buildGroup(g.key, groupStyle, &p, &pp, this);
        box->setTitle(QString::fromUtf8(g.title));
        mainLayout->addWidget(box);
    }

    // ---- Invert hunger-speed checkbox (Movement group, non-registry: bool) ----
    {
        auto* moveBox = reg.groupBox("movement");
        if (moveBox) {
            m_invertHungerCheck = new QCheckBox("\u53cd\u8f6c\u9965\u997f\u901f\u5ea6\u66f2\u7ebf (Invert Hunger-Speed)");
            m_invertHungerCheck->setStyleSheet("color: #888; font-size: 11px;");
            connect(m_invertHungerCheck, &QCheckBox::toggled, this, &MainWindow::onToggleInvertHunger);
            qobject_cast<QVBoxLayout*>(moveBox->layout())->addWidget(m_invertHungerCheck);
        }
    }

    // ================================================================
    // Group: Flock Relationships (dynamic, non-registry)
    // ================================================================
    {
        m_relGroup = new QGroupBox("\u7fa4\u4f53\u5173\u7cfb (Flock Relations)");
        m_relGroup->setStyleSheet(groupStyle);
        m_relGroup->setCheckable(true);
        m_relGroup->setChecked(true);
        m_relContainer = new QWidget();
        m_relVLayout = new QVBoxLayout(m_relContainer);
        m_relVLayout->setContentsMargins(6, 2, 6, 6);
        m_relVLayout->setSpacing(1);
        m_relGroup->setLayout(new QVBoxLayout());
        m_relGroup->layout()->setContentsMargins(0, 0, 0, 0);
        m_relGroup->layout()->addWidget(m_relContainer);
        mainLayout->addWidget(m_relGroup);
        rebuildRelationshipUI();
    }

    // ================================================================
    // Group: Plant Ecology (from registry + season label)
    // ================================================================
    {
        auto* box = reg.buildGroup("plants", groupStyle, &p, &pp, this);
        box->setTitle("\u690d\u7269\u751f\u6001 (Plant Ecology)");

        // Season label (handled outside registry: dynamic, non-param)
        m_seasonLabel = new QLabel("\u5b63\u8282: \u6625\u5b63 (Spring)");
        m_seasonLabel->setStyleSheet("color: #80c080; font-size: 12px; font-weight: bold; padding: 4px 0 0 0;");
        qobject_cast<QVBoxLayout*>(box->layout())->addWidget(m_seasonLabel);

        mainLayout->addWidget(box);
    }

    // ================================================================
    // Group: Foraging (from registry)
    // ================================================================
    {
        auto* box = reg.buildGroup("forage", groupStyle, &p, &pp, this);
        box->setTitle("\u89c5\u98df\u884c\u4e3a (Foraging)");
        mainLayout->addWidget(box);
    }

    // ================================================================
    // Group: Reproduction (from registry)
    // ================================================================
    {
        auto* box = reg.buildGroup("repro", groupStyle, &p, &pp, this);
        box->setTitle("\u7fa4\u4f53\u7e41\u884d (Reproduction)");
        mainLayout->addWidget(box);
    }

    // ================================================================
    // Group: Appearance (per-flock, non-registry: color buttons + checkbox)
    // ================================================================
    {
        m_appearanceGroup = new QGroupBox("\u5916\u89c2 (Appearance)");
        m_appearanceGroup->setStyleSheet(groupStyle);
        m_appearanceGroup->setCheckable(true);
        m_appearanceGroup->setChecked(true);
        auto* gLayout = new QVBoxLayout(m_appearanceGroup);
        gLayout->setContentsMargins(6, 2, 6, 6);
        gLayout->setSpacing(4);

        // Flock color button
        {
            auto* row = new QHBoxLayout();
            auto* lbl = new QLabel("Flock Color:");
            lbl->setStyleSheet("color: #888; font-size: 11px;");
            row->addWidget(lbl);
            m_flockColorBtn = new QPushButton();
            m_flockColorBtn->setFixedSize(28, 20);
            m_flockColorBtn->setToolTip("Change flock color");
            connect(m_flockColorBtn, &QPushButton::clicked, this, [this]() {
                int af = m_glWidget->simulation().activeFlock();
                onChangeFlockColor(af);
            });
            row->addWidget(m_flockColorBtn);
            row->addStretch();
            gLayout->addLayout(row);
        }

        // Male color button
        {
            auto* row = new QHBoxLayout();
            auto* lbl = new QLabel("Male Color:");
            lbl->setStyleSheet("color: #888; font-size: 11px;");
            row->addWidget(lbl);
            m_maleColorBtn = new QPushButton();
            m_maleColorBtn->setFixedSize(28, 20);
            m_maleColorBtn->setToolTip("Change male boid color");
            connect(m_maleColorBtn, &QPushButton::clicked, this, [this]() {
                int af = m_glWidget->simulation().activeFlock();
                onChangeMaleColor(af);
            });
            row->addWidget(m_maleColorBtn);
            row->addStretch();
            gLayout->addLayout(row);
        }

        // Female color button
        {
            auto* row = new QHBoxLayout();
            auto* lbl = new QLabel("Female Color:");
            lbl->setStyleSheet("color: #888; font-size: 11px;");
            row->addWidget(lbl);
            m_femaleColorBtn = new QPushButton();
            m_femaleColorBtn->setFixedSize(28, 20);
            m_femaleColorBtn->setToolTip("Change female boid color");
            connect(m_femaleColorBtn, &QPushButton::clicked, this, [this]() {
                int af = m_glWidget->simulation().activeFlock();
                onChangeFemaleColor(af);
            });
            row->addWidget(m_femaleColorBtn);
            row->addStretch();
            gLayout->addLayout(row);
        }

        // Sex colors checkbox
        m_sexColorsCheck = new QCheckBox("Enable Sex Colors");
        m_sexColorsCheck->setStyleSheet("color: #888; font-size: 11px;");
        connect(m_sexColorsCheck, &QCheckBox::toggled, this, &MainWindow::onToggleSexColors);
        gLayout->addWidget(m_sexColorsCheck);

        // Sprite selector
        {
            auto* row = new QHBoxLayout();
            auto* lbl = new QLabel("Sprite:");
            lbl->setStyleSheet("color: #888; font-size: 11px;");
            row->addWidget(lbl);
            m_spriteCombo = new QComboBox();
            m_spriteCombo->setStyleSheet(
                "QComboBox { background: #3a3a3a; color: #ddd; border: 1px solid #555; "
                "font-size: 11px; padding: 1px 3px; }"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView { background: #333; color: #ddd; "
                "selection-background-color: #555; font-size: 11px; }");
            m_spriteCombo->setToolTip("Select sprite image for this flock\nEmpty = solid color");
            connect(m_spriteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MainWindow::onSpriteChanged);
            row->addWidget(m_spriteCombo, 1);
            gLayout->addLayout(row);
        }

        // Import + Refresh buttons
        {
            auto* row = new QHBoxLayout();
            row->addStretch();
            m_spriteImportBtn = new QPushButton("Import...");
            m_spriteImportBtn->setStyleSheet(
                "QPushButton { background: #3a3a3a; color: #aaa; border: 1px solid #555; "
                "border-radius: 2px; padding: 2px 8px; font-size: 10px; }"
                "QPushButton:hover { background: #555; color: #ddd; }");
            m_spriteImportBtn->setToolTip("Import a PNG sprite image (will be copied to image/ folder)");
            connect(m_spriteImportBtn, &QPushButton::clicked, this, &MainWindow::onImportSprite);
            row->addWidget(m_spriteImportBtn);

            m_spriteRefreshBtn = new QPushButton("Refresh");
            m_spriteRefreshBtn->setStyleSheet(
                "QPushButton { background: #3a3a3a; color: #aaa; border: 1px solid #555; "
                "border-radius: 2px; padding: 2px 8px; font-size: 10px; }"
                "QPushButton:hover { background: #555; color: #ddd; }");
            m_spriteRefreshBtn->setToolTip("Refresh sprites from image/ folder (detects changes via hash)");
            connect(m_spriteRefreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshSprites);
            row->addWidget(m_spriteRefreshBtn);
            gLayout->addLayout(row);
        }

        // Upright sprite toggle
        m_uprightCheck = new QCheckBox("Upright sprite (mirror instead of 180-rotate)");
        m_uprightCheck->setToolTip(
            "When ON: sprite always stays upright, mirrors horizontally when moving left.\n"
            "When OFF: sprite rotates freely in full 360-degree direction.");
        connect(m_uprightCheck, &QCheckBox::toggled, this, &MainWindow::onToggleUpright);
        gLayout->addWidget(m_uprightCheck);

        mainLayout->addWidget(m_appearanceGroup);
    }

    mainLayout->addStretch();

    scroll->setWidget(dock);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* flockSettingsDock = new QDockWidget("\u7fa4\u4f53\u53c2\u6570 (Flock Settings)", this);
    flockSettingsDock->setWidget(scroll);
    flockSettingsDock->setObjectName("FlockSettingsDock");
    addDockWidget(Qt::RightDockWidgetArea, flockSettingsDock);

    // ---- Independent docks: global settings + statistics ----
    setupGlobalDock();
    setupStatsDock();

    // ================================================================
    // Connect all registry sliders: write param on change
    // ================================================================
    reg.connectAll([this](const ParamDef* def, float val) {
        auto& sim = m_glWidget->simulation();
        void* base = def->isPlantParam
            ? static_cast<void*>(&sim.plantParams())
            : static_cast<void*>(&sim.params());
        def->writeTo(base, val);
        if (def->onChanged) def->onChanged(val);
    });

    // Defer sprite loading until GL context is ready (after first paint)
    QTimer::singleShot(100, this, [this]() { initSprites(); });
  }

void MainWindow::refreshSliders()
{
    auto& sim = m_glWidget->simulation();
    m_params.refresh(&sim.params(), &sim.plantParams());

    // Season label
    int season = sim.currentSeason();
    const char* seasonNames[] = {
        "\u6625\u5b63 (Spring)", "\u590f\u5b63 (Summer)",
        "\u79cb\u5b63 (Autumn)", "\u51ac\u5b63 (Winter)"};
    const char* seasonColors[] = {"#80c080", "#c0c040", "#c08040", "#80a0c0"};
    if (m_seasonLabel) {
        m_seasonLabel->setText(QString("\u5b63\u8282: %1").arg(seasonNames[season]));
        m_seasonLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: bold; padding: 4px 0 0 0;")
                .arg(seasonColors[season]));
    }

    // Refresh relationship combos dynamically
    int nf = sim.flockCount();
    for (int i = 0; i < nf; ++i) {
        for (int j = 0; j < nf; ++j) {
            if (i < static_cast<int>(m_relCombos.size()) &&
                j < static_cast<int>(m_relCombos[i].size()) &&
                m_relCombos[i][j]) {
                // Only update if not the same combo cell that user is interacting with
                m_relCombos[i][j]->blockSignals(true);
                int rel = static_cast<int>(sim.relationship(i, j));
                m_relCombos[i][j]->setCurrentIndex(rel);
                m_relCombos[i][j]->blockSignals(false);
            }
        }
    }
}

// ---- Global Settings Dock (independent, draggable) ----
void MainWindow::setupGlobalDock()
{
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(6, 4, 6, 6);
    layout->setSpacing(4);

    auto& sim = m_glWidget->simulation();

    m_wrapBoundaryCheck = new QCheckBox("\u5faa\u73af\u8fb9\u754c (Wrap Boundary)");
    m_wrapBoundaryCheck->setStyleSheet("color: #888; font-size: 11px;");
    m_wrapBoundaryCheck->setToolTip("ON = \u74b0\u9762\u62d3\u64b2 (\u5f9e\u5de6\u908a\u51fa\u53bb\u5f9e\u53f3\u908a\u56de\u4f86)\nOFF = \u786c\u78b0\u649e\u7246\u58c1 (\u53cd\u5f48\u56de\u4f86)");
    m_wrapBoundaryCheck->setChecked(sim.globalParams().wrapBoundary);
    connect(m_wrapBoundaryCheck, &QCheckBox::toggled, this, &MainWindow::onToggleWrapBoundary);
    layout->addWidget(m_wrapBoundaryCheck);

    m_globalHungerFlashCheck = new QCheckBox("\u9965\u997f\u95ea\u70c1 (Hunger Flash)");
    m_globalHungerFlashCheck->setStyleSheet("color: #888; font-size: 11px;");
    m_globalHungerFlashCheck->setToolTip("\u5168\u5c40\u63a7\u5236: \u9965\u997f\u65f6 boid \u662f\u5426\u7ea2\u8272\u95ea\u70c1");
    m_globalHungerFlashCheck->setChecked(sim.globalParams().hungerFlashEnabled);
    connect(m_globalHungerFlashCheck, &QCheckBox::toggled, this, &MainWindow::onToggleHungerFlash);
    layout->addWidget(m_globalHungerFlashCheck);

    layout->addSpacing(8);

    // Global flock cap slider (batch-set maxFlockSize for all flocks)
    auto* capLabel = new QLabel("\u7fa4\u4f53\u4e0a\u9650 (Flock Cap)");
    capLabel->setStyleSheet("color: #888; font-size: 11px;");
    layout->addWidget(capLabel);

    auto* capRow = new QHBoxLayout();
    m_globalFlockCapSlider = new QSlider(Qt::Horizontal);
    m_globalFlockCapSlider->setRange(10, 2000);
    m_globalFlockCapSlider->setSingleStep(10);
    m_globalFlockCapSlider->setPageStep(100);
    m_globalFlockCapSlider->setStyleSheet(
        "QSlider::groove:horizontal { background: #3a3a3a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #666; width: 10px; margin: -3px 0; "
        "  border-radius: 3px; }"
        "QSlider::handle:horizontal:hover { background: #888; }");
    int initCap = sim.flockParams(sim.activeFlock()).maxFlockSize;
    m_globalFlockCapSlider->setValue(initCap);
    m_globalFlockCapSlider->setToolTip(QString("%1 \u4e2a").arg(initCap));
    connect(m_globalFlockCapSlider, &QSlider::valueChanged, this, &MainWindow::onGlobalFlockCapChanged);
    capRow->addWidget(m_globalFlockCapSlider);

    m_globalFlockCapLabel = new QLabel(QString("%1").arg(initCap));
    m_globalFlockCapLabel->setStyleSheet("color: #aaa; font-size: 11px; min-width: 30px;");
    m_globalFlockCapLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    capRow->addWidget(m_globalFlockCapLabel);

    layout->addLayout(capRow);

    layout->addSpacing(8);

    // Sprite scale slider (2x - 100x, multiplies rendered size of sprited boids)
    auto* spriteScaleLabel = new QLabel("\u7cbe\u7075\u56fe\u7f29\u653e (Sprite Scale)");
    spriteScaleLabel->setStyleSheet("color: #888; font-size: 11px;");
    layout->addWidget(spriteScaleLabel);

    auto* spriteScaleRow = new QHBoxLayout();
    m_spriteScaleSlider = new QSlider(Qt::Horizontal);
    m_spriteScaleSlider->setRange(2, 100);
    m_spriteScaleSlider->setSingleStep(1);
    m_spriteScaleSlider->setPageStep(10);
    m_spriteScaleSlider->setValue(2);
    m_spriteScaleSlider->setStyleSheet(
        "QSlider::groove:horizontal { background: #3a3a3a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #666; width: 10px; margin: -3px 0; "
        "  border-radius: 3px; }"
        "QSlider::handle:horizontal:hover { background: #888; }");
    m_spriteScaleSlider->setToolTip("2x");
    connect(m_spriteScaleSlider, &QSlider::valueChanged, this, &MainWindow::onSpriteScaleChanged);
    spriteScaleRow->addWidget(m_spriteScaleSlider);

    m_spriteScaleLabel = new QLabel("2");
    m_spriteScaleLabel->setStyleSheet("color: #aaa; font-size: 11px; min-width: 30px;");
    m_spriteScaleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    spriteScaleRow->addWidget(m_spriteScaleLabel);

    layout->addLayout(spriteScaleRow);

    layout->addStretch();

    auto* dock = new QDockWidget("\u5168\u5c40\u63a7\u5236 (Global Settings)", this);
    dock->setWidget(content);
    dock->setObjectName("GlobalSettingsDock");
    dock->setMinimumWidth(180);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

// ---- Statistics Dock (independent, draggable) ----
void MainWindow::setupStatsDock()
{
    m_statsContent = new QWidget();
    m_statsLayout = new QVBoxLayout(m_statsContent);
    m_statsLayout->setContentsMargins(8, 4, 8, 6);
    m_statsLayout->setSpacing(3);

    // Per-flock labels will be added dynamically in refreshStatsPanel

    m_statsTotalLabel = new QLabel();
    m_statsTotalLabel->setStyleSheet("color: #ccc; font-size: 11px;");
    m_statsLayout->addWidget(m_statsTotalLabel);

    m_statsPlantsLabel = new QLabel();
    m_statsPlantsLabel->setStyleSheet("color: #aa8; font-size: 11px;");
    m_statsLayout->addWidget(m_statsPlantsLabel);

    m_statsTimeLabel = new QLabel();
    m_statsTimeLabel->setStyleSheet("color: #8ac; font-size: 11px;");
    m_statsLayout->addWidget(m_statsTimeLabel);

    m_statsLayout->addStretch();

    auto* dock = new QDockWidget("\u7edf\u8ba1\u4fe1\u606f (Statistics)", this);
    dock->setWidget(m_statsContent);
    dock->setObjectName("StatisticsDock");
    dock->setMinimumWidth(180);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::refreshStatsPanel()
{
    if (!m_statsLayout) return;

    auto& sim = m_glWidget->simulation();
    int nf = sim.flockCount();

    // Collect per-flock stats (same logic as former HUD)
    struct PerFlockStat { int count; float avgHunger; int males; int females; };
    std::vector<PerFlockStat> stats(nf);
    int maxFlock = -1;
    for (int i = 0; i < sim.data().count; ++i) {
        int fid = sim.data().flockId[i];
        if (fid < 0 || fid >= nf) continue;
        stats[fid].count++;
        stats[fid].avgHunger += sim.data().hunger[i];
        if (sim.data().sex[i] == 0) ++stats[fid].males;
        else                        ++stats[fid].females;
        if (fid > maxFlock) maxFlock = fid;
    }
    for (int f = 0; f <= maxFlock && f < nf; ++f) {
        if (stats[f].count > 0)
            stats[f].avgHunger /= static_cast<float>(stats[f].count);
    }

    // Rebuild per-flock labels if flock count changed
    if (nf != m_lastStatsFlockCount) {
        for (auto* lbl : m_statsFlockLabels) {
            m_statsLayout->removeWidget(lbl);
            delete lbl;
        }
        m_statsFlockLabels.clear();

        int insertIdx = 0;
        for (int f = 0; f < nf; ++f) {
            auto* lbl = new QLabel();
            lbl->setStyleSheet("font-size: 11px; padding: 1px 0;");
            m_statsFlockLabels.push_back(lbl);
            m_statsLayout->insertWidget(insertIdx++, lbl);
        }
        m_lastStatsFlockCount = nf;
    }

    // Update per-flock labels with full info: count, sex, hunger
    int total = 0;
    for (int f = 0; f < nf && f < static_cast<int>(m_statsFlockLabels.size()); ++f) {
        int cnt = stats[f].count;
        int cap = sim.flockParams(f).maxFlockSize;
        total += cnt;
        const float* fc = sim.flockColor(f);
        QString color = QString("rgb(%1,%2,%3)")
            .arg(static_cast<int>(fc[0] * 255))
            .arg(static_cast<int>(fc[1] * 255))
            .arg(static_cast<int>(fc[2] * 255));

        // Count: normal "X /Y" or red ">Y" when overflowing
        QString cntStr = (cnt > cap)
            ? QString("<span style='color:#ff4444;'>%1 &gt;%2</span>").arg(cnt).arg(cap)
            : QString("<span style='color:#ccc;'>%1</span> <span style='color:#666;'>/%2</span>").arg(cnt).arg(cap);

        // Sex ratio: "M:40 F:40"
        QString sexStr = QString("<span style='color:#aaa;'>\u2642%1 \u2640%2</span>")
            .arg(stats[f].males).arg(stats[f].females);

        // Hunger average with color coding (same thresholds as former HUD)
        float hungerPct = stats[f].avgHunger * 100.0f;
        QString hungerColor;
        if      (hungerPct > 60.0f) hungerColor = "#64c864";  // green
        else if (hungerPct > 25.0f) hungerColor = "#dcb432";  // yellow
        else                         hungerColor = "#dc5050";  // red
        QString hungerStr = QString("<span style='color:%1;'>\u9971: %2%</span>")
            .arg(hungerColor).arg(hungerPct, 0, 'f', 0);

        m_statsFlockLabels[f]->setText(
            QString("<span style='color:%1;'>\u25cf</span> <span style='color:#ccc;'>%2</span> %3  %4")
                .arg(color)
                .arg(QString::fromStdString(sim.flockName(f)).left(12))
                .arg(sexStr)
                .arg(hungerStr));
    }

    // Total / max
    int maxB = sim.maxBoids();
    m_statsTotalLabel->setText(
        QString("\u603b\u6570 (Total): %1 / %2").arg(total).arg(maxB));

    // Plants (with max cap)
    int plantCount = sim.plants().aliveCount();
    int plantMax = sim.plantParams().maxPlants;
    m_statsPlantsLabel->setText(
        QString("\u690d\u7269 (Plants): %1 / %2").arg(plantCount).arg(plantMax));

    // Time + season + pseudo-year (same as former HUD)
    float totalSecs = sim.simTime();
    float seasonLen = sim.plantParams().seasonLength;
    float yearLen = seasonLen * 4.0f;
    int pseudoYear = 1 + static_cast<int>(totalSecs / yearLen);
    float yearProgress = std::fmod(totalSecs, yearLen) / yearLen * 100.0f;
    int season = sim.currentSeason();
    const char* seasonNames[] = {"\u6625", "\u590f", "\u79cb", "\u51ac"};
    m_statsTimeLabel->setText(
        QString("\u65f6\u95f4 (Time): Year %1, %2  (%3%)")
            .arg(pseudoYear)
            .arg(seasonNames[season])
            .arg(yearProgress, 0, 'f', 1));
}

// ---- Flock management ----
void MainWindow::onAddFlock()
{
    auto& sim = m_glWidget->simulation();
    int result = sim.addFlock();
    if (result >= 0) {
        updateFlockCombo();
        updateFlockButtons();
    } else if (result == -1) {
        statusBar()->showMessage(
            QString("\u7fa4\u4f53\u6570\u91cf\u5df2\u8fbe\u4e0a\u9650 (%1)").arg(MAX_FLOCKS), 3000);
    } else {
        statusBar()->showMessage(
            QString("\u5168\u5c40\u4e2a\u4f53\u5df2\u8fbe\u4e0a\u9650 (%1), \u65e0\u6cd5\u65b0\u5efa\u7fa4\u4f53")
                .arg(sim.maxBoids()), 3000);
    }
}

void MainWindow::onRemoveFlock()
{
    auto& sim = m_glWidget->simulation();
    bool ok = sim.removeFlock(sim.activeFlock());
    if (ok) {
        updateFlockCombo();
        updateFlockButtons();
    }
}

void MainWindow::onFlockButton(int id)
{
    auto& sim = m_glWidget->simulation();
    if (id >= 0 && id < sim.flockCount()) {
        sim.setActiveFlock(id);
        updateFlockButtons();
        refreshSliders();
    }
}

void MainWindow::onRenameFlock(int id)
{
    auto& sim = m_glWidget->simulation();
    if (id < 0 || id >= sim.flockCount()) return;

    bool ok = false;
    QString newName = QInputDialog::getText(this,
        "Rename Flock",
        "New name:",
        QLineEdit::Normal,
        QString::fromStdString(sim.flockName(id)),
        &ok);

    if (ok && !newName.isEmpty()) {
        sim.setFlockName(id, newName.toStdString());
        updateFlockCombo();
        updateFlockButtons();
    }
}

void MainWindow::pickColor(QPushButton* btn, float* r, float* g, float* b)
{
    QColor current(
        static_cast<int>((*r) * 255),
        static_cast<int>((*g) * 255),
        static_cast<int>((*b) * 255));
    QColor c = QColorDialog::getColor(current, this, "Pick Color");
    if (c.isValid()) {
        *r = c.redF();
        *g = c.greenF();
        *b = c.blueF();
        btn->setStyleSheet(
            QString("background-color: rgb(%1,%2,%3); border: 1px solid #666;")
                .arg(c.red()).arg(c.green()).arg(c.blue()));
        // Save current params so per-flock colors persist
        m_glWidget->simulation().saveCurrentParams();
        updateFlockButtons();
    }
}

void MainWindow::onChangeFlockColor(int id)
{
    auto& sim = m_glWidget->simulation();
    if (id < 0 || id >= sim.flockCount()) return;
    auto& fp = sim.params();
    pickColor(m_flockColorBtn, &fp.maleColorR, &fp.maleColorG, &fp.maleColorB);
    // Sync flock rendering colors
    auto* fc = const_cast<float*>(sim.flockColor(id));
    if (fc) { fc[0] = fp.maleColorR; fc[1] = fp.maleColorG; fc[2] = fp.maleColorB; }
}

void MainWindow::onChangeMaleColor(int id)
{
    auto& sim = m_glWidget->simulation();
    if (id < 0 || id >= sim.flockCount()) return;
    auto& fp = sim.params();
    pickColor(m_maleColorBtn, &fp.maleColorR, &fp.maleColorG, &fp.maleColorB);
}

void MainWindow::onChangeFemaleColor(int id)
{
    auto& sim = m_glWidget->simulation();
    if (id < 0 || id >= sim.flockCount()) return;
    auto& fp = sim.params();
    pickColor(m_femaleColorBtn, &fp.femaleColorR, &fp.femaleColorG, &fp.femaleColorB);
}

void MainWindow::onToggleSexColors(bool checked)
{
    auto& sim = m_glWidget->simulation();
    sim.params().useSexColors = checked;
    sim.saveCurrentParams();
}

void MainWindow::onToggleInvertHunger(bool checked)
{
    auto& sim = m_glWidget->simulation();
    sim.params().invertHungerSpeed = checked;
    sim.saveCurrentParams();
}

void MainWindow::onToggleWrapBoundary(bool checked)
{
    auto& sim = m_glWidget->simulation();
    sim.globalParams().wrapBoundary = checked;
}

void MainWindow::onToggleHungerFlash(bool checked)
{
    auto& sim = m_glWidget->simulation();
    sim.globalParams().hungerFlashEnabled = checked;
}

void MainWindow::onGlobalFlockCapChanged(int value)
{
    auto& sim = m_glWidget->simulation();
    sim.setGlobalFlockCap(value);
    m_globalFlockCapSlider->setToolTip(QString("%1 \u4e2a").arg(value));
    m_globalFlockCapLabel->setText(QString("%1").arg(value));
    // Sync the per-flock ParameterRegistry slider too (if active flock params changed)
    refreshSliders();
}

void MainWindow::onSpriteScaleChanged(int value)
{
    m_spriteScaleSlider->setToolTip(QString("%1x").arg(value));
    m_spriteScaleLabel->setText(QString("%1").arg(value));
    m_glWidget->renderer().setSpriteScale(static_cast<float>(value));
}

// ---- Sprite management ----
QStringList MainWindow::scanImageDirectory() const
{
    QDir dir(QApplication::applicationDirPath() + QDir::separator() + "image");
    if (!dir.exists()) return {};
    QStringList filters;
    filters << "*.png";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
    QStringList paths;
    for (const auto& f : files)
        paths.append(dir.absoluteFilePath(f));
    return paths;
}

void MainWindow::initSprites()
{
    QStringList paths = scanImageDirectory();
    auto& renderer = m_glWidget->renderer();

    // GL operations require an active context (not guaranteed from timer/button slots)
    m_glWidget->makeCurrent();
    renderer.loadSprites(paths);
    m_glWidget->doneCurrent();

    refreshSpriteCombo();
}

void MainWindow::refreshSpriteCombo()
{
    if (!m_spriteCombo) return;
    auto& sim = m_glWidget->simulation();
    int af = sim.activeFlock();

    m_spriteCombo->blockSignals(true);
    m_spriteCombo->clear();
    m_spriteCombo->addItem("None", QString());

    QStringList files = scanImageDirectory();
    for (const auto& path : files) {
        QFileInfo fi(path);
        m_spriteCombo->addItem(fi.fileName(), fi.fileName());
    }

    // Restore selection for current flock
    if (af >= 0 && af < sim.flockCount()) {
        QString currentSprite = QString::fromStdString(sim.flockParams(af).spriteName);
        int idx = m_spriteCombo->findData(currentSprite);
        if (idx >= 0)
            m_spriteCombo->setCurrentIndex(idx);
        else
            m_spriteCombo->setCurrentIndex(0);
    }

    m_spriteCombo->blockSignals(false);
}

void MainWindow::onSpriteChanged(int index)
{
    if (!m_spriteCombo || index < 0) return;
    auto& sim = m_glWidget->simulation();
    QString name = m_spriteCombo->itemData(index).toString();
    sim.params().spriteName = name.toStdString();
    sim.saveCurrentParams();
}

void MainWindow::onImportSprite()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "Import Sprite Image",
        QString(), "PNG Images (*.png)");
    if (filePath.isEmpty()) return;

    // Ensure image/ directory exists next to the executable
    QDir imageDir(QApplication::applicationDirPath() + QDir::separator() + "image");
    if (!imageDir.exists())
        imageDir.mkpath(".");

    // Copy file to image/ directory (skip if source is already in image/)
    QFileInfo srcFi(filePath);
    QString destPath = imageDir.absoluteFilePath(srcFi.fileName());

    bool sameFile = (srcFi.canonicalFilePath() == QFileInfo(destPath).canonicalFilePath());
    if (!sameFile) {
        if (QFile::exists(destPath))
            QFile::remove(destPath);
        if (!QFile::copy(filePath, destPath)) {
            statusBar()->showMessage("Failed to import sprite image", 3000);
            return;
        }
    }

    // Reload sprites (makeCurrent/doneCurrent handled inside initSprites)
    initSprites();
    statusBar()->showMessage(
        QString(sameFile ? "Refreshed: %1" : "Imported: %1").arg(srcFi.fileName()), 3000);
}

void MainWindow::onRefreshSprites()
{
    QStringList paths = scanImageDirectory();
    auto& renderer = m_glWidget->renderer();

    if (renderer.spritesChanged(paths)) {
        m_glWidget->makeCurrent();
        int loaded = renderer.loadSprites(paths);
        m_glWidget->doneCurrent();
        statusBar()->showMessage(
            QString("Sprites reloaded: %1 image(s)").arg(loaded), 2000);
    } else {
        statusBar()->showMessage("Sprites unchanged", 2000);
    }
    refreshSpriteCombo();
}

void MainWindow::onToggleUpright(bool checked)
{
    auto& sim = m_glWidget->simulation();
    sim.params().uprightSprite = checked;
    sim.saveCurrentParams();
}

// ---- Boid spawn/remove ----
void MainWindow::onSpawn()
{
    auto& sim = m_glWidget->simulation();
    int requested = m_countSpin->value();
    int spawned = sim.spawnRandom(requested);
    if (spawned == 0 && requested > 0) {
        // ---- Determine which cap blocked the spawn ----
        int flockCap = sim.flockParams(sim.activeFlock()).maxFlockSize;
        int flockCnt = sim.countInFlock(sim.activeFlock());
        if (sim.data().count >= sim.maxBoids()) {
            statusBar()->showMessage(
                QString("\u5168\u5c40\u4e2a\u4f53\u5df2\u8fbe\u4e0a\u9650 (%1), \u65e0\u6cd5\u751f\u6210")
                    .arg(sim.maxBoids()), 3000);
        } else if (flockCnt >= flockCap) {
            statusBar()->showMessage(
                QString("\"%1\" \u7fa4\u5df2\u8fbe\u4e0a\u9650 (%2)")
                    .arg(QString::fromStdString(sim.flockName(sim.activeFlock())))
                    .arg(flockCap), 3000);
        }
    } else if (spawned < requested) {
        statusBar()->showMessage(
            QString("\u4ec5\u751f\u6210 %1/%2 (\u5df2\u8fbe\u4e0a\u9650)").arg(spawned).arg(requested), 3000);
    }
}

void MainWindow::onRemove()
{
    int count = m_countSpin->value();
    int flockId = m_glWidget->simulation().activeFlock();
    m_glWidget->simulation().removeBoidsFromFlock(flockId, count);
}

void MainWindow::onClearAll() { m_glWidget->simulation().data().clear(); }

// ---- UI helpers ----
void MainWindow::updateFlockButtons()
{
    auto& sim = m_glWidget->simulation();
    int active = sim.activeFlock();
    int n = sim.flockCount();

    // Guard: flocks not yet initialized
    if (n == 0) return;

    // Update combo box item texts
    if (m_flockCombo) {
        m_flockCombo->blockSignals(true);
        for (int i = 0; i < n && i < m_flockCombo->count(); ++i) {
            m_flockCombo->setItemText(i, QString::fromStdString(sim.flockName(i)));
        }
        if (active >= 0 && active < n && m_flockCombo->currentIndex() != active)
            m_flockCombo->setCurrentIndex(active);
        m_flockCombo->blockSignals(false);
    }

    // Update toolbar color button
    if (m_toolbarColorBtn) {
        const float* fc = sim.flockColor(active);
        m_toolbarColorBtn->setStyleSheet(
            QString("background-color: rgb(%1,%2,%3); border: 1px solid #666;")
                .arg(static_cast<int>(fc[0] * 255))
                .arg(static_cast<int>(fc[1] * 255))
                .arg(static_cast<int>(fc[2] * 255)));
    }

    if (m_flockLabel) {
        const float* fc = sim.flockColor(active);
        m_flockLabel->setText(QString("\u5f53\u524d\u7fa4\u4f53: %1").arg(sim.flockName(active).c_str()));
        m_flockLabel->setStyleSheet(
            QString("font-weight: bold; color: rgb(%1,%2,%3); padding: 4px;")
                .arg(static_cast<int>(fc[0] * 255))
                .arg(static_cast<int>(fc[1] * 255))
                .arg(static_cast<int>(fc[2] * 255)));
    }

    // Update per-flock group box titles via registry
    QString flockName = QString::fromStdString(sim.flockName(active));
    QString defaultName = QString::fromStdString(sim.defaultFlockName(active));
    bool isCustom = (flockName != defaultName);

    auto makeTitle = [&](const char* zh, const char* en) -> QString {
        if (isCustom)
            return QString("%1 - %2").arg(flockName).arg(QString::fromUtf8(zh));
        else
            return QString("%1 (%2)").arg(QString::fromUtf8(zh)).arg(QString::fromUtf8(en));
    };

    // Per-flock groups (titles change per selected flock)
    struct FlockGroupTitle { const char* key; const char* zh; const char* en; };
    const FlockGroupTitle flockGroups[] = {
        {"hunger",     "\u9965\u997f\u4e0e\u9971\u8179", "Hunger & Satiety"},
        {"predation",  "\u6355\u730e\u4e0e\u9003\u79bb", "Predation & Escape"},
        {"interflock", "\u7fa4\u95f4\u884c\u4e3a",       "Inter-Flock"},
        {"reynolds",   "Reynolds",                       "Reynolds Rules"},
        {"movement",   "\u79fb\u52a8\u4e0e\u78b0\u649e",       "Movement \u0026 Collision"},
        {"perception", "\u611f\u77e5\u534a\u5f84",       "Perception"},
        {"boundary",   "\u8fb9\u754c\u4e0e\u6e38\u8361", "Boundary & Wander"},
        {"forage",     "\u89c5\u98df\u884c\u4e3a",       "Foraging"},
        {"repro",      "\u7fa4\u4f53\u7e41\u884d",       "Reproduction"},
    };
    for (auto& g : flockGroups) {
        ParamRegistry::setGroupTitle(m_params.groupBox(g.key), makeTitle(g.zh, g.en));
    }

    // Appearance group (same pattern, but non-registry)
    if (m_appearanceGroup) {
        m_appearanceGroup->setTitle(makeTitle("\u5916\u89c2", "Appearance"));
        m_appearanceGroup->style()->unpolish(m_appearanceGroup);
        m_appearanceGroup->style()->polish(m_appearanceGroup);
    }

    updateColorButtons();

    // Update relationship labels and rebuild if count changed
    rebuildRelationshipUI();
}

void MainWindow::rebuildRelationshipUI()
{
    if (!m_relVLayout) return;

    auto& sim = m_glWidget->simulation();
    int n = sim.flockCount();

    // Check if combo vectors already match the required size
    bool needsRebuild = (static_cast<int>(m_relCombos.size()) != n);
    if (!needsRebuild) {
        for (int i = 0; i < n; ++i) {
            if (static_cast<int>(m_relCombos[i].size()) != n) {
                needsRebuild = true;
                break;
            }
        }
    }

    if (!needsRebuild) {
        // Just update labels (names may have changed) and rel values
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (m_relLabels[i][j]) {
                    m_relLabels[i][j]->setText(QString("%1 -> %2")
                        .arg(QString::fromStdString(sim.flockName(i)))
                        .arg(QString::fromStdString(sim.flockName(j))));
                }
                if (m_relCombos[i][j]) {
                    m_relCombos[i][j]->blockSignals(true);
                    m_relCombos[i][j]->setCurrentIndex(static_cast<int>(sim.relationship(i, j)));
                    m_relCombos[i][j]->blockSignals(false);
                }
            }
        }
        return;
    }

    // Full rebuild needed: clear old widgets
    if (m_relContainer) {
        m_relGroup->layout()->removeWidget(m_relContainer);
        m_relContainer->deleteLater();
        m_relContainer = nullptr;
        m_relVLayout = nullptr;
    }
    m_relCombos.clear();
    m_relLabels.clear();

    m_relContainer = new QWidget();
    m_relVLayout = new QVBoxLayout(m_relContainer);
    m_relVLayout->setContentsMargins(6, 2, 6, 6);
    m_relVLayout->setSpacing(1);
    m_relGroup->layout()->addWidget(m_relContainer);

    m_relCombos.resize(n);
    m_relLabels.resize(n);
    for (int i = 0; i < n; ++i) {
        m_relCombos[i].resize(n);
        m_relLabels[i].resize(n);
        for (int j = 0; j < n; ++j) {
            // Build a row: label + combo box
            auto* row = new QWidget();
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 1, 0, 1);
            rowLayout->setSpacing(4);

            auto* lbl = new QLabel(QString("%1 -> %2")
                .arg(QString::fromStdString(sim.flockName(i)))
                .arg(QString::fromStdString(sim.flockName(j))));
            lbl->setStyleSheet("color: #888; font-size: 10px;");
            lbl->setFixedWidth(160);
            rowLayout->addWidget(lbl);
            m_relLabels[i][j] = lbl;

            auto* combo = new QComboBox();
            combo->setStyleSheet(
                "QComboBox { background: #3a3a3a; color: #ddd; border: 1px solid #555;"
                "  padding: 1px 4px; font-size: 11px; }"
                "QComboBox QAbstractItemView { background: #3a3a3a; color: #ddd; selection-background-color: #555; }");
            combo->addItems({"\u65e0 (None)", "\u730e\u7269 (Prey)", "\u6355\u98df\u8005 (Predator)"});
            combo->setCurrentIndex(static_cast<int>(sim.relationship(i, j)));
            int vi = i, vj = j;
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    [this, vi, vj](int idx) {
                m_glWidget->simulation().setRelationship(vi, vj, static_cast<FlockRelation>(idx));
            });
            rowLayout->addWidget(combo);
            m_relCombos[i][j] = combo;

            m_relVLayout->addWidget(row);
        }
    }
}

void MainWindow::updateColorButtons()
{
    auto& sim = m_glWidget->simulation();
    int af = sim.activeFlock();
    if (af < 0 || af >= sim.flockCount()) return;

    // Flock color button
    if (m_flockColorBtn) {
        const float* fc = sim.flockColor(af);
        m_flockColorBtn->setStyleSheet(
            QString("background-color: rgb(%1,%2,%3); border: 1px solid #666;")
                .arg(static_cast<int>(fc[0] * 255))
                .arg(static_cast<int>(fc[1] * 255))
                .arg(static_cast<int>(fc[2] * 255)));
    }

    // Male color button
    if (m_maleColorBtn) {
        const auto& fp = sim.flockParams(af);
        m_maleColorBtn->setStyleSheet(
            QString("background-color: rgb(%1,%2,%3); border: 1px solid #666;")
                .arg(static_cast<int>(fp.maleColorR * 255))
                .arg(static_cast<int>(fp.maleColorG * 255))
                .arg(static_cast<int>(fp.maleColorB * 255)));
    }

    // Female color button
    if (m_femaleColorBtn) {
        const auto& fp = sim.flockParams(af);
        m_femaleColorBtn->setStyleSheet(
            QString("background-color: rgb(%1,%2,%3); border: 1px solid #666;")
                .arg(static_cast<int>(fp.femaleColorR * 255))
                .arg(static_cast<int>(fp.femaleColorG * 255))
                .arg(static_cast<int>(fp.femaleColorB * 255)));
    }

    // Sex colors checkbox
    if (m_sexColorsCheck) {
        const auto& fp = sim.flockParams(af);
        m_sexColorsCheck->blockSignals(true);
        m_sexColorsCheck->setChecked(fp.useSexColors);
        m_sexColorsCheck->blockSignals(false);
    }

    // Invert hunger-speed checkbox
    if (m_invertHungerCheck) {
        const auto& fp = sim.flockParams(af);
        m_invertHungerCheck->blockSignals(true);
        m_invertHungerCheck->setChecked(fp.invertHungerSpeed);
        m_invertHungerCheck->blockSignals(false);
    }

    // NOTE: wrapBoundary and hungerFlash are global -- NOT refreshed per flock

    // Upright sprite checkbox
    if (m_uprightCheck) {
        const auto& fp = sim.flockParams(af);
        m_uprightCheck->blockSignals(true);
        m_uprightCheck->setChecked(fp.uprightSprite);
        m_uprightCheck->blockSignals(false);
    }

    // Refresh sprite combo for current flock
    refreshSpriteCombo();
}
