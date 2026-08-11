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
// HO_N: nested offset for FlockParams sub-struct fields
// PO: direct offset for PlantParams fields (no sub-structs)
#define HO_N(sub, field) (offsetof(FlockParams, sub) + offsetof(decltype(FlockParams::sub), field))
#define PO(field) offsetof(PlantParams, field)
#define NO(field) offsetof(NestParams, field)

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

    toolbar->addSeparator();

    auto* saveBtn = new QPushButton("Save");
    saveBtn->setToolTip("Save configuration to file (.biodcfg)");
    toolbar->addWidget(saveBtn);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveConfig);

    auto* loadBtn = new QPushButton("Load");
    loadBtn->setToolTip("Load configuration from file (.biodcfg)");
    toolbar->addWidget(loadBtn);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::onLoadConfig);

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
    // Tab-based layout for scalable parameter organization.
    // Each tab hosts a subset of group boxes in a scrollable area.
    // Tabs are designed to accommodate future expansion:
    //   - Behavior:   Reynolds + Movement + Perception
    //   - Interaction: Predation + Inter-Flock + Foraging
    //   - Lifecycle:   Hunger + Reproduction (+ age/fatigue in Phase 1)
    //   - Appearance:  Colors + Sprite + Size
    //   - Relations:   Inter-flock relationship matrix
    //   - World:       Boundary + Plants + Global settings

    auto* tabWidget = new QTabWidget();
    tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #4a4a4a; } "
        "QTabBar::tab { background: #2b2b2b; color: #999; "
        "padding: 4px 10px; font-size: 11px; border: 1px solid #4a4a4a; "
        "border-bottom: none; min-width: 60px; } "
        "QTabBar::tab:selected { background: #3a3a3a; color: #ddd; } "
        "QTabBar::tab:hover { background: #404040; }");

    auto& sim = m_glWidget->simulation();
    auto& p = sim.params();
    auto& pp = sim.plantParams();

    // Common style for all group boxes
    const QString groupStyle =
        "QGroupBox { font-weight: bold; color: #000000; "
        "border: 1px solid #4a4a4a; border-radius: 3px; margin-top: 7px; "
        "padding-top: 10px; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }";

    // Helper: create a scrollable tab page and return its content layout
    auto makeTab = [&](const char* title) -> QVBoxLayout* {
        auto* page = new QWidget();
        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(page);
        auto* lay = new QVBoxLayout(page);
        lay->setContentsMargins(6, 4, 6, 4);
        lay->setSpacing(4);
        tabWidget->addTab(scroll, QString::fromUtf8(title));
        return lay;
    };

    auto& reg = m_params;

    // ================================================================
    // Register all parameters (same as before, unchanged)
    // ================================================================

    // Group: Hunger & Satiety
    reg.reg({"\u8870\u51cf\u901f\u7387 (Decay Rate)",       "hunger",   HO_N(hunger, hungerDecayRate),       0,   100, ScaleMode::Div1000,   static_cast<int>(p.hunger.hungerDecayRate * 1000),   false});
    reg.reg({"\u9971\u8179\u901f\u5ea6 (Speed Min)",         "hunger",   HO_N(hunger, hungerSpeedMin),        30,  100, ScaleMode::Div100,    static_cast<int>(p.hunger.hungerSpeedMin * 100),     false});
    reg.reg({"\u9965\u997f\u901f\u5ea6 (Speed Max)",         "hunger",   HO_N(hunger, hungerSpeedMax),        100, 250, ScaleMode::Div100,    static_cast<int>(p.hunger.hungerSpeedMax * 100),     false});
    reg.reg({"\u95ea\u70c1\u9608\u503c (Flash Threshold)",   "hunger",   HO_N(hunger, hungerFlashThreshold),  5,   80,  ScaleMode::Div100,    static_cast<int>(p.hunger.hungerFlashThreshold * 100), false});

    // Group: Predation & Escape
    reg.reg({"\u6355\u730e\u6210\u529f\u7387 (Chase Success)",          "predation", HO_N(predation, chaseSuccessBase),             0,   100, ScaleMode::Div100,    static_cast<int>(p.predation.chaseSuccessBase * 100),              false});
    reg.reg({"\u9003\u79bb\u6210\u529f\u7387 (Escape Success)",         "predation", HO_N(predation, escapeSuccessBase),            0,   100, ScaleMode::Div100,    static_cast<int>(p.predation.escapeSuccessBase * 100),             false});
    reg.reg({"\u6355\u730e\u8303\u56f4 (Chase Range)",                  "predation", HO_N(predation, chaseRange),                   5,   100, ScaleMode::OneToOne,  static_cast<int>(p.predation.chaseRange),                           false});
    reg.reg({"\u730e\u6740\u9965\u997f\u9608\u503c (Hunt Hunger)",       "predation", HO_N(predation, predationMinHunger),           5,   95,  ScaleMode::Div100,    static_cast<int>(p.predation.predationMinHunger * 100),            false});
    reg.reg({"\u51fb\u6740\u9965\u997f\u9608\u503c (Kill Hunger)",       "predation", HO_N(predation, predationKillHunger),          1,   50,  ScaleMode::Div100,    static_cast<int>(p.predation.predationKillHunger * 100),           false});
    reg.reg({"\u9971\u8179\u53c2\u4e0e\u7387% (Participation %)",      "predation", HO_N(predation, predationParticipationRate),   1,   100, ScaleMode::Div100,    static_cast<int>(p.predation.predationParticipationRate * 100),    false});
    reg.reg({"\u8fde\u6740\u589e\u91cd (Kill Weight Gain)",     "predation", HO_N(body, weightGainPerKill),          1,   200, ScaleMode::Div1000,   static_cast<int>(p.body.weightGainPerKill * 1000),            false});
    reg.reg({"\u95f2\u7f6e\u8870\u51cf/\u79d2 (Decay Rate/s)",   "predation", HO_N(body, weightDecayRate),            1,   20,  ScaleMode::Div1000,   static_cast<int>(p.body.weightDecayRate * 1000),              false});
    reg.reg({"\u8fde\u6740\u8d85\u65f6/\u79d2 (Streak Timeout)", "predation", HO_N(body, streakTimeout),              5,   100, ScaleMode::Div10,     static_cast<int>(p.body.streakTimeout * 10),                  false});
    reg.reg({"\u8870\u51cf\u5ef6\u8fdf/\u79d2 (Decay Delay)",    "predation", HO_N(body, decayDelay),                 1,   30,  ScaleMode::OneToOne,  static_cast<int>(p.body.decayDelay),                          false});
    reg.reg({"\u6700\u5927\u4f53\u91cd (Max Weight)",            "predation", HO_N(body, maxWeight),                 10,  50,  ScaleMode::Div10,     static_cast<int>(p.body.maxWeight * 10),                      false});
    reg.reg({"\u6700\u5c0f\u4f53\u91cd (Min Weight)",            "predation", HO_N(body, minWeight),                  1,   20,  ScaleMode::Div10,     static_cast<int>(p.body.minWeight * 10),                      false});

    // Group: Inter-Flock
    reg.reg({"\u7fa4\u95f4\u65a5\u529b (Inter-Flock Repulsion)", "interflock", HO_N(interFlock, interFlockRepulsionWeight), 0, 50, ScaleMode::Div10, static_cast<int>(p.interFlock.interFlockRepulsionWeight * 10), false});
    reg.reg({"\u6355\u98df\u5438\u5f15\u529b (Predator Attraction)","interflock", HO_N(interFlock, predatorAttractionWeight), 0, 50, ScaleMode::Div10, static_cast<int>(p.interFlock.predatorAttractionWeight * 10), false});
    reg.reg({"\u730e\u7269\u6050\u60e7 (Prey Fear)",            "interflock", HO_N(interFlock, preyFearWeight),             0, 50, ScaleMode::Div10, static_cast<int>(p.interFlock.preyFearWeight * 10),         false});

    // Group: Reynolds
    reg.reg({"\u5206\u79bb (Separation)", "reynolds", HO_N(perception, separationWeight), 0, 50, ScaleMode::Div10, static_cast<int>(p.perception.separationWeight * 10), false});
    reg.reg({"\u5bf9\u9f50 (Alignment)",  "reynolds", HO_N(perception, alignmentWeight),  0, 50, ScaleMode::Div10, static_cast<int>(p.perception.alignmentWeight * 10),  false});
    reg.reg({"\u805a\u96c6 (Cohesion)",   "reynolds", HO_N(perception, cohesionWeight),   0, 50, ScaleMode::Div10, static_cast<int>(p.perception.cohesionWeight * 10),   false});

    // Group: Movement & Collision
    reg.reg({"\u6700\u5927\u901f\u5ea6 (Max Speed)", "movement", HO_N(movement, maxSpeed), 50, 600, ScaleMode::OneToOne, static_cast<int>(p.movement.maxSpeed), false});
    reg.reg({"\u4f53\u91cd\u901f\u5ea6\u60e9\u7f5a (Weight Penalty)", "movement", HO_N(movement, weightSpeedPenalty), 0, 200, ScaleMode::Div100, static_cast<int>(p.movement.weightSpeedPenalty * 100), false});
    reg.reg({"\u786c\u78b0\u649e\u8ddd\u79bb (Hard Collision)", "movement", HO_N(movement, hardCollisionRadius), 0, 50, ScaleMode::Multiply2, static_cast<int>(p.movement.hardCollisionRadius / 2.0f), false});

    // Group: Perception
    reg.reg({"\u5206\u79bb\u534a\u5f84 (Sep Radius)", "perception", HO_N(perception, separationRadius), 5,  100, ScaleMode::OneToOne, static_cast<int>(p.perception.separationRadius), false});
    reg.reg({"\u5bf9\u9f50\u534a\u5f84 (Ali Radius)", "perception", HO_N(perception, alignmentRadius),  10, 200, ScaleMode::OneToOne, static_cast<int>(p.perception.alignmentRadius),  false});
    reg.reg({"\u805a\u96c6\u534a\u5f84 (Coh Radius)", "perception", HO_N(perception, cohesionRadius),   10, 200, ScaleMode::OneToOne, static_cast<int>(p.perception.cohesionRadius),   false,
             [this](float) { m_glWidget->simulation().updateGrid(); }});

    // Group: Boundary & Wander
    reg.reg({"\u8fb9\u754c\u89c4\u907f (Boundary Avoid)", "boundary", HO_N(boundary, boundaryWeight), 0,  50,  ScaleMode::Div10,    static_cast<int>(p.boundary.boundaryWeight * 10),  false});
    reg.reg({"\u8fb9\u754c\u8ddd\u79bb (Margin)",          "boundary", HO_N(boundary, boundaryMargin), 10, 300, ScaleMode::OneToOne,  static_cast<int>(p.boundary.boundaryMargin),        false});
    reg.reg({"\u968f\u673a\u6e38\u8361 (Wander)",           "boundary", HO_N(boundary, wanderWeight),   0,  30,  ScaleMode::Div10,    static_cast<int>(p.boundary.wanderWeight * 10),    false});

    // Group: Foraging
    reg.reg({"\u641c\u5bfb\u8303\u56f4 (Forage Range)",        "forage", HO_N(hunger, forageRange),            30,  400, ScaleMode::OneToOne,  static_cast<int>(p.hunger.forageRange),              false});
    reg.reg({"\u89c5\u98df\u529b\u5ea6 (Forage Weight)",       "forage", HO_N(hunger, forageWeight),           0,   50,  ScaleMode::Div10,    static_cast<int>(p.hunger.forageWeight * 10),        false});
    reg.reg({"\u9965\u997f\u9608\u503c (Hunger Threshold)",    "forage", HO_N(hunger, forageHungerThreshold),  10,  90,  ScaleMode::Div100,   static_cast<int>(p.hunger.forageHungerThreshold * 100), false});

    // Group: Reproduction
    reg.reg({"\u6700\u5c11\u540e\u4ee3 (Min Offspring)",  "repro", HO_N(reproduction, reproductionMinOffspring), 1,  5,    ScaleMode::OneToOne,  static_cast<int>(p.reproduction.reproductionMinOffspring),                  false});
    reg.reg({"\u6700\u591a\u540e\u4ee3 (Max Offspring)",  "repro", HO_N(reproduction, reproductionMaxOffspring), 1,  10,   ScaleMode::OneToOne,  static_cast<int>(p.reproduction.reproductionMaxOffspring),                  false});
    reg.reg({"\u7fa4\u4f53\u4e0a\u9650 (Max Flock Size)", "repro", HO_N(reproduction, maxFlockSize),             50, 5000, ScaleMode::OneToOne,  p.reproduction.maxFlockSize,                              false});
    reg.reg({"\u6700\u4f4e\u9971\u8179\u503c (Min Hunger)","repro",  HO_N(reproduction, reproductionMinHunger),  40, 95,   ScaleMode::Div100,   static_cast<int>(p.reproduction.reproductionMinHunger * 100), false});
    reg.reg({"\u7e41\u6b96\u95f4\u9694/\u79d2 (Interval)","repro",   HO_N(reproduction, reproductionInterval),   30, 300,  ScaleMode::OneToOne,  static_cast<int>(p.reproduction.reproductionInterval),       false});

    // Group: Age & Lifecycle (Phase 1.1)
    reg.reg({"\u6700\u5927\u5bff\u547d (Max Lifespan)",    "age", HO_N(age, maxLifespan),   100, 7200, ScaleMode::OneToOne, static_cast<int>(p.age.maxLifespan),  false});
    reg.reg({"\u5e7c\u5e74\u671f (Juvenile Age)",          "age", HO_N(age, juvenileAge),    0,   600, ScaleMode::OneToOne, static_cast<int>(p.age.juvenileAge),  false});
    reg.reg({"\u9752\u5e74\u671f (Young Age)",             "age", HO_N(age, youngAge),      10,  1200, ScaleMode::OneToOne, static_cast<int>(p.age.youngAge),    false});
    reg.reg({"\u8001\u5e74\u671f (Elder Age)",             "age", HO_N(age, elderAge),      60,  6000, ScaleMode::OneToOne, static_cast<int>(p.age.elderAge),    false});

    // Group: Body Size (Phase 1.2) -- age-stage visual size multipliers
    reg.reg({"\u5e7c\u5e74\u4f53\u578b (Juvenile Size)", "bodysize", HO_N(age, ageSizeJuvenile), 10, 200, ScaleMode::Div100, static_cast<int>(p.age.ageSizeJuvenile * 100), false});
    reg.reg({"\u9752\u5e74\u4f53\u578b (Young Size)",    "bodysize", HO_N(age, ageSizeYoung),    10, 200, ScaleMode::Div100, static_cast<int>(p.age.ageSizeYoung * 100),    false});
    reg.reg({"\u6210\u5e74\u4f53\u578b (Adult Size)",    "bodysize", HO_N(age, ageSizeAdult),    10, 200, ScaleMode::Div100, static_cast<int>(p.age.ageSizeAdult * 100),    false});
    reg.reg({"\u8001\u5e74\u4f53\u578b (Elder Size)",    "bodysize", HO_N(age, ageSizeElder),    10, 200, ScaleMode::Div100, static_cast<int>(p.age.ageSizeElder * 100),    false});

    // Group: Fatigue (Phase 1.3)
    reg.reg({"\u7d2f\u79ef\u901f\u7387 (Accum Rate)",    "fatigue", HO_N(fatigue, fatigueAccumRate),    1, 100, ScaleMode::Div1000, static_cast<int>(p.fatigue.fatigueAccumRate * 1000),    false});
    reg.reg({"\u6062\u590d\u901f\u7387 (Recovery Rate)",  "fatigue", HO_N(fatigue, fatigueRecoveryRate), 10, 500, ScaleMode::Div1000, static_cast<int>(p.fatigue.fatigueRecoveryRate * 1000),  false});
    reg.reg({"\u901f\u5ea6\u60e9\u7f5a (Speed Penalty)", "fatigue", HO_N(fatigue, fatigueSpeedPenalty),  0, 100, ScaleMode::Div100,  static_cast<int>(p.fatigue.fatigueSpeedPenalty * 100),   false});

    // Group: Gender Dimorphism (Phase 1.4)
    reg.reg({"\u96c4\u6027\u901f\u5ea6 (Male Speed)",  "gender", HO_N(gender, sexSpeedMale),   50, 200, ScaleMode::Div100, static_cast<int>(p.gender.sexSpeedMale * 100),   false});
    reg.reg({"\u96cc\u6027\u901f\u5ea6 (Female Speed)","gender", HO_N(gender, sexSpeedFemale), 50, 200, ScaleMode::Div100, static_cast<int>(p.gender.sexSpeedFemale * 100), false});
    reg.reg({"\u96c4\u6027\u4f53\u578b (Male Size)",   "gender", HO_N(gender, sexSizeMale),    50, 200, ScaleMode::Div100, static_cast<int>(p.gender.sexSizeMale * 100),    false});
    reg.reg({"\u96cc\u6027\u4f53\u578b (Female Size)", "gender", HO_N(gender, sexSizeFemale),  50, 200, ScaleMode::Div100, static_cast<int>(p.gender.sexSizeFemale * 100),  false});

    // Group: Pregnancy (Phase 1.5)
    reg.reg({"\u598a\u5a20\u65f6\u957f (Duration)",          "pregnancy", HO_N(pregnancy, pregnancyDuration),   5,  120, ScaleMode::OneToOne, static_cast<int>(p.pregnancy.pregnancyDuration),   false});
    reg.reg({"\u4ea7\u540e\u6062\u590d (Postpartum)",        "pregnancy", HO_N(pregnancy, postpartumRecovery), 10,  300, ScaleMode::OneToOne, static_cast<int>(p.pregnancy.postpartumRecovery), false});
    reg.reg({"\u54fa\u4e73\u9971\u8179 (Nursing Boost)",     "pregnancy", HO_N(pregnancy, offspringHungerBoost), 0, 80, ScaleMode::Div100, static_cast<int>(p.pregnancy.offspringHungerBoost * 100), false});

    // Group: Male Combat (Phase 2.1)
    reg.reg({"\u5185\u6597\u534a\u5f84 (Combat Radius)",  "combat", HO_N(combat, combatRadius),       5,  150, ScaleMode::OneToOne, static_cast<int>(p.combat.combatRadius),       false});
    reg.reg({"\u5185\u6597\u6982\u7387 (Probability)",    "combat", HO_N(combat, combatProbability),  5,  100, ScaleMode::Div100,  static_cast<int>(p.combat.combatProbability * 100), false});
    reg.reg({"\u75b2\u52b3\u589e\u52a0 (Fatigue Gain)",   "combat", HO_N(combat, combatFatigueGain),  1,  50,  ScaleMode::Div100,  static_cast<int>(p.combat.combatFatigueGain * 100),  false});
    reg.reg({"\u5185\u6597\u51b7\u5374 (Cooldown)",       "combat", HO_N(combat, combatCooldown),     1,  30,  ScaleMode::OneToOne, static_cast<int>(p.combat.combatCooldown),      false});

    // Group: Hatred / Enmity (Phase 2.2)
    reg.reg({"\u4ec7\u6068\u589e\u76ca (Gain per Kill)", "hatred", HO_N(hatred, hatredGainPerKill),       1,  80, ScaleMode::Div100,  static_cast<int>(p.hatred.hatredGainPerKill * 100),      false});
    reg.reg({"\u4ec7\u6068\u8870\u51cf (Decay Rate)",   "hatred", HO_N(hatred, hatredDecayRate),        1,  20,  ScaleMode::Div100,  static_cast<int>(p.hatred.hatredDecayRate * 100),        false});
    reg.reg({"\u9003\u8dd1\u8ddd\u79bb (Flee Radius)",    "hatred", HO_N(hatred, hatredFleeRadiusBoost), 1,  100, ScaleMode::Div10,   static_cast<int>(p.hatred.hatredFleeRadiusBoost * 10),   false});
    reg.reg({"\u9003\u8dd1\u529b\u5ea6 (Flee Weight)",    "hatred", HO_N(hatred, hatredFleeWeightBoost), 1,  50,  ScaleMode::Div10,   static_cast<int>(p.hatred.hatredFleeWeightBoost * 10),   false});

    // Group: Escape Strategy (Phase 2.3)
    reg.reg({"\u9003\u8dd1\u7b56\u7565 (Strategy)",        "escape", HO_N(escape, escapeStrategy),     0,  3,   ScaleMode::OneToOne,  static_cast<int>(p.escape.escapeStrategy),   false});
    reg.reg({"\u6df7\u5408\u5e45\u5ea6 (Mix Factor)",      "escape", HO_N(escape, escapeStrategyMix),  0,  100, ScaleMode::Div100,  static_cast<int>(p.escape.escapeStrategyMix * 100), false});
    reg.reg({"\u4e4b\u5b57\u632f\u5e45 (Zigzag Amp)",      "escape", HO_N(escape, escapeZigzagAmp),   10,  100, ScaleMode::Div100,  static_cast<int>(p.escape.escapeZigzagAmp * 100), false});

    // Group: Defensive Cooperation (Phase 2.4)
    reg.reg({"\u9632\u5fa1\u534a\u5f84 (Defense Radius)",   "defense", HO_N(defense, defenseRadius),         50,  500, ScaleMode::OneToOne, static_cast<int>(p.defense.defenseRadius),         false});
    reg.reg({"\u53cd\u51fb\u529b\u5ea6 (Response Weight)",   "defense", HO_N(defense, defenseResponseWeight), 5,   100, ScaleMode::Div100,  static_cast<int>(p.defense.defenseResponseWeight * 100),  false});
    reg.reg({"\u7fa4\u4f53\u9608\u503c (Group Threshold)",   "defense", HO_N(defense, defenseGroupThreshold), 1,   10,  ScaleMode::OneToOne, static_cast<int>(p.defense.defenseGroupThreshold),   false});

    // Group: Cohesion Dynamics (Phase 2.5)
    reg.reg({"\u57fa\u7840\u6743\u91cd (Base Weight)",       "cohesionDyn", HO_N(cohesionDyn, cohesionBaseWeight),   10,  200, ScaleMode::Div100,  static_cast<int>(p.cohesionDyn.cohesionBaseWeight * 100),   false});
    reg.reg({"\u5a01\u80c1\u589e\u76ca (Threat Boost)",      "cohesionDyn", HO_N(cohesionDyn, cohesionThreatBoost), 10,  400, ScaleMode::Div100,  static_cast<int>(p.cohesionDyn.cohesionThreatBoost * 100),  false});
    reg.reg({"\u9965\u997f\u8870\u51cf (Hunger Decay)",      "cohesionDyn", HO_N(cohesionDyn, cohesionHungerDecay), 0,   100, ScaleMode::Div100,  static_cast<int>(p.cohesionDyn.cohesionHungerDecay * 100),   false});
    reg.reg({"\u5bc6\u5ea6\u8870\u51cf (Density Decay)",     "cohesionDyn", HO_N(cohesionDyn, cohesionDensityDecay), 0,  100, ScaleMode::Div100,  static_cast<int>(p.cohesionDyn.cohesionDensityDecay * 100), false});

    // Group: Health / Dodge / Damage (Phase 1.7)
    reg.reg({"\u57fa\u7840\u95ea\u907f\u7387 (Dodge Base)",         "health", HO_N(health, dodgeChanceBase),  0,   80,  ScaleMode::Div100,  static_cast<int>(p.health.dodgeChanceBase * 100),  false});
    reg.reg({"\u4f24\u5bb3\u7cfb\u6570 (Damage Multiplier)",       "health", HO_N(health, damageToHealth),   10,  100, ScaleMode::Div100,  static_cast<int>(p.health.damageToHealth * 100),   false});
    reg.reg({"\u56de\u8840\u901f\u5ea6 (Health Regen)",            "health", HO_N(health, healthRegenRate),  0,   50,  ScaleMode::Div1000, static_cast<int>(p.health.healthRegenRate * 1000), false});
    reg.reg({"\u521d\u59cb\u751f\u547d (Initial Health)",         "health", HO_N(health, healthInitial),    50,  100, ScaleMode::Div100,  static_cast<int>(p.health.healthInitial * 100),    false});

    // Nest preference params (Phase 3.1)
    reg.reg({"\u5de2\u7a74\u56de\u5f52\u6743\u91cd (Nest Return)",      "nestPref", HO_N(nestPref, nestReturnWeight),      0,  100, ScaleMode::Div100,  static_cast<int>(p.nestPref.nestReturnWeight * 100),    true});
    reg.reg({"\u5de2\u7a74\u98df\u7269\u504f\u597d (Food Prefer)",      "nestPref", HO_N(nestPref, nestPreferFoodDensity), 0,  100, ScaleMode::Div100,  static_cast<int>(p.nestPref.nestPreferFoodDensity * 100), false});
    reg.reg({"\u5de2\u7a74\u5b89\u5168\u504f\u597d (Safety Prefer)",    "nestPref", HO_N(nestPref, nestPreferSafety),      0,  100, ScaleMode::Div100,  static_cast<int>(p.nestPref.nestPreferSafety * 100),      false});
    reg.reg({"\u5de2\u7a74\u9009\u5740\u8303\u56f4 (Selection Range)",  "nestPref", HO_N(nestPref, nestSelectionRange),    50, 500, ScaleMode::OneToOne, static_cast<int>(p.nestPref.nestSelectionRange),           false});

    // Plant params
    reg.reg({"\u6700\u5927\u6570\u91cf (Max Plants)",          "plants", PO(maxPlants),       10,  500, ScaleMode::OneToOne,  static_cast<int>(pp.maxPlants),  false});
    reg.reg({"\u8fdb\u98df\u8303\u56f4 (Eat Range)",           "plants", PO(eatRange),        5,   100, ScaleMode::OneToOne,  static_cast<int>(pp.eatRange),      true});
    reg.reg({"\u690d\u7269\u98df\u7269\u503c (Food Value)",     "plants", PO(plantFoodValue),  10,  100, ScaleMode::Div100,  static_cast<int>(pp.plantFoodValue * 100), true});
    reg.reg({"\u751f\u957f\u65f6\u95f4 (Growth Time)",         "plants", PO(growthTime),      10,  200, ScaleMode::OneToOne,  static_cast<int>(pp.growthTime),    true});
    reg.reg({"\u6269\u6563\u6982\u7387 (Spread Chance)",       "plants", PO(spreadChance),    0,   100, ScaleMode::Div1000,  static_cast<int>(pp.spreadChance * 1000), true});
    reg.reg({"\u6269\u6563\u8303\u56f4 (Spread Range)",        "plants", PO(spreadRange),     20,  300, ScaleMode::OneToOne,  static_cast<int>(pp.spreadRange),   true});
    reg.reg({"\u5b63\u8282\u957f\u5ea6 (Season Length)",       "plants", PO(seasonLength),    10,  300, ScaleMode::OneToOne,  static_cast<int>(pp.seasonLength),  true});
    reg.reg({"\u80a5\u6c83\u534a\u5f84 (Fertilize Radius)",    "plants", PO(fertilizeRadius), 20,  200, ScaleMode::OneToOne,  static_cast<int>(pp.fertilizeRadius), true});
    reg.reg({"\u80a5\u6c83\u589e\u5e45 (Fertilize Boost)",     "plants", PO(fertilizeBoost),  5,   80,  ScaleMode::Div100,   static_cast<int>(pp.fertilizeBoost * 100), true});
    reg.reg({"\u627f\u8f7d\u538b\u529b (Carrying Pressure)",   "plants", PO(carryingPressure), 0,  100, ScaleMode::Div100,   static_cast<int>(pp.carryingPressure * 100), true});

    // Global nest params (Phase 3.1)
    const NestParams& np = sim.nestParams();
    reg.reg({"\u521d\u59cb\u5de2\u7a74\u6570 (Initial Nests)",       "nest", NO(initialNests),      0,  50,  ScaleMode::OneToOne,  static_cast<int>(np.initialNests),    false});
    reg.reg({"\u5de2\u7a74\u9886\u5730\u534a\u5f84 (Nest Radius)",   "nest", NO(nestRadius),        50, 500, ScaleMode::OneToOne,  static_cast<int>(np.nestRadius),      true});
    reg.reg({"\u56de\u8840\u52a0\u6210 (Health Boost)",              "nest", NO(nestHealthBoost),   100,500, ScaleMode::Div100,   static_cast<int>(np.nestHealthBoost * 100), true});
    reg.reg({"\u98df\u7269\u5b58\u50a8\u901f\u7387 (Food Storage)",  "nest", NO(nestFoodStorageRate),0,  20,  ScaleMode::Div1000,  static_cast<int>(np.nestFoodStorageRate * 1000), true});
    reg.reg({"\u4e89\u593a\u65f6\u957f (Contest Duration)",          "nest", NO(contestDuration),   5,  60,  ScaleMode::OneToOne,  static_cast<int>(np.contestDuration),  false});
    reg.reg({"\u5b88\u536b\u95e8\u69db (Defense Threshold)",         "nest", NO(defenseThreshold),  1,  20,  ScaleMode::OneToOne,  static_cast<int>(np.defenseThreshold), false});

    // ================================================================
    // Build tabs
    // ================================================================
    // Tab 0: Behavior (Reynolds + Movement + Perception)
    {
        auto* lay = makeTab("Behavior");
        auto* box1 = reg.buildGroup("reynolds", groupStyle, &p, &pp, this);
        box1->setTitle("Reynolds \u7fa4\u4f53\u89c4\u5219");
        lay->addWidget(box1);

        auto* box2 = reg.buildGroup("movement", groupStyle, &p, &pp, this);
        box2->setTitle("\u79fb\u52a8\u4e0e\u78b0\u649e (Movement \u0026 Collision)");
        lay->addWidget(box2);

        auto* box3 = reg.buildGroup("perception", groupStyle, &p, &pp, this);
        box3->setTitle("\u611f\u77e5\u534a\u5f84 (Perception)");
        lay->addWidget(box3);

        // Invert hunger-speed checkbox (attached to movement group)
        m_invertHungerCheck = new QCheckBox("\u53cd\u8f6c\u9965\u997f\u901f\u5ea6\u66f2\u7ebf (Invert Hunger-Speed)");
        m_invertHungerCheck->setStyleSheet("color: #888; font-size: 11px;");
        connect(m_invertHungerCheck, &QCheckBox::toggled, this, &MainWindow::onToggleInvertHunger);
        lay->addWidget(m_invertHungerCheck);

        lay->addStretch();
    }

    // Tab 1: Interaction (Predation + Inter-Flock + Foraging)
    {
        auto* lay = makeTab("Interaction");
        auto* box1 = reg.buildGroup("predation", groupStyle, &p, &pp, this);
        box1->setTitle("\u6355\u730e\u4e0e\u9003\u79bb (Predation & Escape)");
        lay->addWidget(box1);

        auto* box2 = reg.buildGroup("interflock", groupStyle, &p, &pp, this);
        box2->setTitle("\u7fa4\u95f4\u884c\u4e3a (Inter-Flock)");
        lay->addWidget(box2);

        auto* box3 = reg.buildGroup("forage", groupStyle, &p, &pp, this);
        box3->setTitle("\u89c5\u98df\u884c\u4e3a (Foraging)");
        lay->addWidget(box3);

        lay->addStretch();
    }

    // Tab 2: Lifecycle (Hunger + Reproduction + Age + Body Size)
    // Phase 1 will add Fatigue, Gender here
    {
        auto* lay = makeTab("Lifecycle");
        auto* box1 = reg.buildGroup("hunger", groupStyle, &p, &pp, this);
        box1->setTitle("\u9965\u997f\u4e0e\u9971\u8179 (Hunger & Satiety)");
        lay->addWidget(box1);

        auto* box2 = reg.buildGroup("repro", groupStyle, &p, &pp, this);
        box2->setTitle("\u7fa4\u4f53\u7e41\u884d (Reproduction)");
        lay->addWidget(box2);

        auto* boxPreg = reg.buildGroup("pregnancy", groupStyle, &p, &pp, this);
        boxPreg->setTitle("\u598a\u5a20\u4e0e\u54fa\u4e73 (Pregnancy & Nursing)");
        lay->addWidget(boxPreg);

        auto* box3 = reg.buildGroup("age", groupStyle, &p, &pp, this);
        box3->setTitle("\u5e74\u9f84\u4e0e\u5bff\u547d (Age & Lifespan)");
        lay->addWidget(box3);

        auto* box4 = reg.buildGroup("bodysize", groupStyle, &p, &pp, this);
        box4->setTitle("\u4f53\u578b\u5927\u5c0f (Body Size)");
        lay->addWidget(box4);

        auto* box5 = reg.buildGroup("fatigue", groupStyle, &p, &pp, this);
        box5->setTitle("\u75b2\u52b3\u7cfb\u7edf (Fatigue)");
        lay->addWidget(box5);

        auto* box6 = reg.buildGroup("gender", groupStyle, &p, &pp, this);
        box6->setTitle("\u6027\u522b\u4e8c\u6001 (Gender Dimorphism)");
        lay->addWidget(box6);

        auto* boxCombat = reg.buildGroup("combat", groupStyle, &p, &pp, this);
        boxCombat->setTitle("\u96c4\u6027\u5185\u6597 (Male Combat)");
        lay->addWidget(boxCombat);

        auto* boxHatred = reg.buildGroup("hatred", groupStyle, &p, &pp, this);
        boxHatred->setTitle("\u4ec7\u6068\u7cfb\u7edf (Hatred / Enmity)");
        lay->addWidget(boxHatred);

        auto* boxEscape = reg.buildGroup("escape", groupStyle, &p, &pp, this);
        boxEscape->setTitle("\u9003\u8dd1\u7b56\u7565 (Escape Strategy)");
        lay->addWidget(boxEscape);

        auto* boxDefense = reg.buildGroup("defense", groupStyle, &p, &pp, this);
        boxDefense->setTitle("\u9632\u5fa1\u534f\u4f5c (Defensive Cooperation)");
        lay->addWidget(boxDefense);

        auto* boxCohDyn = reg.buildGroup("cohesionDyn", groupStyle, &p, &pp, this);
        boxCohDyn->setTitle("\u51dd\u805a\u529b\u52a8\u6001 (Cohesion Dynamics)");
        lay->addWidget(boxCohDyn);

        auto* boxHealth = reg.buildGroup("health", groupStyle, &p, &pp, this);
        boxHealth->setTitle("\u751f\u547d\u4e0e\u6218\u6597 (Health & Combat)");
        lay->addWidget(boxHealth);

        auto* boxNestPref = reg.buildGroup("nestPref", groupStyle, &p, &pp, this);
        boxNestPref->setTitle("\u5de2\u7a74\u504f\u597d (Nest Preference)");
        lay->addWidget(boxNestPref);

        lay->addStretch();
    }

    // Tab 3: Appearance (Colors + Sprite + Size)
    {
        auto* lay = makeTab("Appearance");
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

        // Male color
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

        // Female color
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

        // Import + Refresh
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

        m_uprightCheck = new QCheckBox("Upright sprite (mirror instead of 180-rotate)");
        m_uprightCheck->setToolTip(
            "When ON: sprite always stays upright, mirrors horizontally when moving left.\n"
            "When OFF: sprite rotates freely in full 360-degree direction.");
        connect(m_uprightCheck, &QCheckBox::toggled, this, &MainWindow::onToggleUpright);
        gLayout->addWidget(m_uprightCheck);

        lay->addWidget(m_appearanceGroup);

        // Sprite scale slider (global rendering setting)
        auto* spriteScaleLabel = new QLabel("\u7cbe\u7075\u56fe\u7f29\u653e (Sprite Scale)");
        spriteScaleLabel->setStyleSheet("color: #888; font-size: 11px;");
        lay->addWidget(spriteScaleLabel);

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
        lay->addLayout(spriteScaleRow);

        lay->addStretch();
    }

    // Tab 4: Relations (inter-flock relationship matrix)
    {
        auto* lay = makeTab("Relations");
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
        lay->addWidget(m_relGroup);
        lay->addStretch();
        rebuildRelationshipUI();
    }

    // Tab 5: World (Boundary + Plants + Global)
    {
        auto* lay = makeTab("World");

        auto* box1 = reg.buildGroup("boundary", groupStyle, &p, &pp, this);
        box1->setTitle("\u8fb9\u754c\u4e0e\u6e38\u8361 (Boundary & Wander)");
        lay->addWidget(box1);

        // Boundary mode selector (global, non-registry)
        {
            auto* bmRow = new QHBoxLayout();
            auto* bmLabel = new QLabel("Boundary Mode:");
            bmLabel->setStyleSheet("color: #888; font-size: 11px;");
            bmRow->addWidget(bmLabel);
            m_boundaryCombo = new QComboBox();
            m_boundaryCombo->addItem("Hybrid (Soft + Wrap)", static_cast<int>(BoundaryMode::Hybrid));
            m_boundaryCombo->addItem("Torus (Wrap only)",   static_cast<int>(BoundaryMode::Torus));
            m_boundaryCombo->addItem("SoftWall (Repel only)", static_cast<int>(BoundaryMode::SoftWall));
            m_boundaryCombo->addItem("HardWall (Bounce)",    static_cast<int>(BoundaryMode::HardWall));
            m_boundaryCombo->setCurrentIndex(static_cast<int>(sim.globalParams().boundaryMode));
            m_boundaryCombo->setToolTip("Boundary behavior for all boids");
            m_boundaryCombo->setStyleSheet("color: #aaa; font-size: 11px; background: #3a3a3a;");
            connect(m_boundaryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this](int idx) {
                auto& s = m_glWidget->simulation();
                s.globalParams().boundaryMode = static_cast<BoundaryMode>(idx);
            });
            bmRow->addWidget(m_boundaryCombo);
            lay->addLayout(bmRow);
        }

        // Hunger flash toggle
        m_globalHungerFlashCheck = new QCheckBox("\u9965\u997f\u95ea\u70c1 (Hunger Flash)");
        m_globalHungerFlashCheck->setStyleSheet("color: #888; font-size: 11px;");
        m_globalHungerFlashCheck->setToolTip("Global: boids flash red when starving");
        m_globalHungerFlashCheck->setChecked(sim.globalParams().hungerFlashEnabled);
        connect(m_globalHungerFlashCheck, &QCheckBox::toggled, this, &MainWindow::onToggleHungerFlash);
        lay->addWidget(m_globalHungerFlashCheck);

        lay->addSpacing(6);

        // Plants
        auto* box2 = reg.buildGroup("plants", groupStyle, &p, &pp, this);
        box2->setTitle("\u690d\u7269\u751f\u6001 (Plant Ecology)");
        lay->addWidget(box2);

        // Nest global params (Phase 3.1)
        auto* boxNest = reg.buildGroup("nest", groupStyle, &p, &pp, this);
        boxNest->setTitle("\u5de2\u7a74\u7cfb\u7edf (Nest System)");
        lay->addWidget(boxNest);

        // Season label
        m_seasonLabel = new QLabel("\u5b63\u8282: \u6625\u5b63 (Spring)");
        m_seasonLabel->setStyleSheet("color: #80c080; font-size: 12px; font-weight: bold; padding: 4px 0 0 0;");
        lay->addWidget(m_seasonLabel);

        // Global flock cap
        auto* capLabel = new QLabel("\u7fa4\u4f53\u4e0a\u9650 (Flock Cap)");
        capLabel->setStyleSheet("color: #888; font-size: 11px;");
        lay->addWidget(capLabel);

        auto* capRow = new QHBoxLayout();
        m_globalCapSlider = new QSlider(Qt::Horizontal);
        m_globalCapSlider->setRange(100, 10000);
        m_globalCapSlider->setValue(sim.maxBoids());
        m_globalCapSlider->setStyleSheet("QSlider::groove:horizontal { height: 4px; background: #555; border-radius: 2px; }"
            "QSlider::handle:horizontal { width: 10px; height: 14px; background: #888; border-radius: 3px; margin: -5px 0; }");
        connect(m_globalCapSlider, &QSlider::valueChanged, this, [this](int v) {
            m_glWidget->simulation().setGlobalFlockCap(v);
        });
        capRow->addWidget(m_globalCapSlider);
        m_globalCapValueLabel = new QLabel(QString::number(sim.maxBoids()));
        m_globalCapValueLabel->setStyleSheet("color: #aaa; font-size: 11px; font-weight: bold; min-width: 40px;");
        m_globalCapValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        connect(m_globalCapSlider, &QSlider::valueChanged, this, [this](int v) {
            m_globalCapValueLabel->setText(QString::number(v));
        });
        capRow->addWidget(m_globalCapValueLabel);
        lay->addLayout(capRow);

        // Boid count display
        auto* countLabel = new QLabel("Boids: 0");
        countLabel->setStyleSheet("color: #666; font-size: 10px;");
        countLabel->setObjectName("globalBoidCount");
        lay->addWidget(countLabel);

        lay->addStretch();
    }

    // ================================================================
    // Active flock indicator (above tabs, always visible)
    // ================================================================
    auto* mainWidget = new QWidget();
    auto* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);

    m_flockLabel = new QLabel("\u5f53\u524d\u7fa4\u4f53: Flock A");
    m_flockLabel->setStyleSheet("font-weight: bold; color: #aaa; padding: 4px;");
    mainLayout->addWidget(m_flockLabel);
    mainLayout->addWidget(tabWidget);

    // Dock widget wrapping everything
    auto* flockSettingsDock = new QDockWidget("\u7fa4\u4f53\u53c2\u6570 (Flock Settings)", this);
    flockSettingsDock->setWidget(mainWidget);
    flockSettingsDock->setObjectName("FlockSettingsDock");
    addDockWidget(Qt::RightDockWidgetArea, flockSettingsDock);

    // Stats dock only (global settings merged into World tab)
    setupStatsDock();

    // ================================================================
    // Connect all registry sliders
    // ================================================================
    reg.connectAll([this](const ParamDef* def, float val) {
        auto& sim = m_glWidget->simulation();
        void* base = def->isPlantParam
            ? static_cast<void*>(&sim.plantParams())
            : static_cast<void*>(&sim.params());
        def->writeTo(base, val);
        if (def->onChanged) def->onChanged(val);
    });

    // Defer sprite loading until GL context is ready
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
        int cap = sim.flockParams(f).reproduction.maxFlockSize;
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
    pickColor(m_flockColorBtn, &fp.appearance.maleColorR, &fp.appearance.maleColorG, &fp.appearance.maleColorB);
    // Sync flock rendering colors
    auto* fc = const_cast<float*>(sim.flockColor(id));
    if (fc) { fc[0] = fp.appearance.maleColorR; fc[1] = fp.appearance.maleColorG; fc[2] = fp.appearance.maleColorB; }
}

void MainWindow::onChangeMaleColor(int id)
{
    auto& sim = m_glWidget->simulation();
    if (id < 0 || id >= sim.flockCount()) return;
    auto& fp = sim.params();
    pickColor(m_maleColorBtn, &fp.appearance.maleColorR, &fp.appearance.maleColorG, &fp.appearance.maleColorB);
}

void MainWindow::onChangeFemaleColor(int id)
{
    auto& sim = m_glWidget->simulation();
    if (id < 0 || id >= sim.flockCount()) return;
    auto& fp = sim.params();
    pickColor(m_femaleColorBtn, &fp.appearance.femaleColorR, &fp.appearance.femaleColorG, &fp.appearance.femaleColorB);
}

void MainWindow::onToggleSexColors(bool checked)
{
    auto& sim = m_glWidget->simulation();
    sim.params().appearance.useSexColors = checked;
    sim.saveCurrentParams();
}

void MainWindow::onToggleInvertHunger(bool checked)
{
    auto& sim = m_glWidget->simulation();
    sim.params().hunger.invertHungerSpeed = checked;
    sim.saveCurrentParams();
}

void MainWindow::onToggleHungerFlash(bool checked)
{
    auto& sim = m_glWidget->simulation();
    sim.globalParams().hungerFlashEnabled = checked;
}

void MainWindow::onGlobalFlockCapChanged(int value)
{
    // NOTE: maxFlockSize is now per-flock (in Reproduction group via ParamRegistry).
    // This slot retained as no-op for backward compatibility.
    (void)value;
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
        QString currentSprite = QString::fromStdString(sim.flockParams(af).appearance.spriteName);
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
    sim.params().appearance.spriteName = name.toStdString();
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
    sim.params().appearance.uprightSprite = checked;
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
        int flockCap = sim.flockParams(sim.activeFlock()).reproduction.maxFlockSize;
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

void MainWindow::onSaveConfig()
{
    QString path = QFileDialog::getSaveFileName(this,
        "Save Configuration", QString(), "Biod Config (*.biodcfg)");
    if (path.isEmpty()) return;
    // Ensure .biodcfg extension
    if (!path.endsWith(".biodcfg", Qt::CaseInsensitive))
        path += ".biodcfg";

    auto& sim = m_glWidget->simulation();
    sim.saveCurrentParams(); // Flush active params before saving
    if (sim.saveConfig(path.toUtf8().constData()))
        statusBar()->showMessage("Config saved: " + path, 3000);
    else
        statusBar()->showMessage("Failed to save config", 3000);
}

void MainWindow::onLoadConfig()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Load Configuration", QString(), "Biod Config (*.biodcfg)");
    if (path.isEmpty()) return;

    auto& sim = m_glWidget->simulation();
    if (!sim.loadConfig(path.toUtf8().constData())) {
        statusBar()->showMessage("Failed to load config: " + path, 3000);
        return;
    }

    // Rebuild UI for the new flock set
    updateFlockCombo();
    updateFlockButtons();
    refreshSliders();
    rebuildRelationshipUI();

    statusBar()->showMessage("Config loaded: " + path, 3000);
}

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
                .arg(static_cast<int>(fp.appearance.maleColorR * 255))
                .arg(static_cast<int>(fp.appearance.maleColorG * 255))
                .arg(static_cast<int>(fp.appearance.maleColorB * 255)));
    }

    // Female color button
    if (m_femaleColorBtn) {
        const auto& fp = sim.flockParams(af);
        m_femaleColorBtn->setStyleSheet(
            QString("background-color: rgb(%1,%2,%3); border: 1px solid #666;")
                .arg(static_cast<int>(fp.appearance.femaleColorR * 255))
                .arg(static_cast<int>(fp.appearance.femaleColorG * 255))
                .arg(static_cast<int>(fp.appearance.femaleColorB * 255)));
    }

    // Sex colors checkbox
    if (m_sexColorsCheck) {
        const auto& fp = sim.flockParams(af);
        m_sexColorsCheck->blockSignals(true);
        m_sexColorsCheck->setChecked(fp.appearance.useSexColors);
        m_sexColorsCheck->blockSignals(false);
    }

    // Invert hunger-speed checkbox
    if (m_invertHungerCheck) {
        const auto& fp = sim.flockParams(af);
        m_invertHungerCheck->blockSignals(true);
        m_invertHungerCheck->setChecked(fp.hunger.invertHungerSpeed);
        m_invertHungerCheck->blockSignals(false);
    }

    // NOTE: wrapBoundary and hungerFlash are global -- NOT refreshed per flock

    // Upright sprite checkbox
    if (m_uprightCheck) {
        const auto& fp = sim.flockParams(af);
        m_uprightCheck->blockSignals(true);
        m_uprightCheck->setChecked(fp.appearance.uprightSprite);
        m_uprightCheck->blockSignals(false);
    }

    // Refresh sprite combo for current flock
    refreshSpriteCombo();
}
