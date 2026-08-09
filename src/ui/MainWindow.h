#pragma once

#include <QMainWindow>
#include <QColorDialog>

#include "core/ParamRegistry.h"

class GLWidget;
class QLabel;
class QPushButton;
class QComboBox;
class QSpinBox;
class QGroupBox;
class QCheckBox;
class QVBoxLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onSpawn();
    void onRemove();
    void onClearAll();
    void onAddFlock();
    void onRemoveFlock();
    void onFlockButton(int id);
    void onRenameFlock(int id);
    void onChangeFlockColor(int id);
    void onChangeMaleColor(int id);
    void onChangeFemaleColor(int id);
    void onToggleSexColors(bool checked);
    void onToggleInvertHunger(bool checked);
    void onToggleWrapBoundary(bool checked);
    void onToggleHungerFlash(bool checked);
    void onGlobalFlockCapChanged(int value);
    void onSpriteChanged(int index);
    void onImportSprite();
    void onRefreshSprites();
    void onSpriteScaleChanged(int value);
    void onToggleUpright(bool checked);

private:
    void setupUI();
    void setupToolbar();
    void setupGlobalDock();
    void setupStatsDock();
    void refreshStatsPanel();
    void updateFlockCombo();
    void updateFlockButtons();
    void refreshSliders();
    void rebuildRelationshipUI();

    GLWidget* m_glWidget = nullptr;
    QLabel* m_flockLabel = nullptr;

    // Flock selection dropdown + inline controls
    QComboBox* m_flockCombo = nullptr;
    QPushButton* m_renameBtn = nullptr;
    QPushButton* m_toolbarColorBtn = nullptr;

    // Add / remove flock buttons
    QPushButton* m_addFlockBtn = nullptr;
    QPushButton* m_removeFlockBtn = nullptr;

    // Parameter registry (replaces all per-slider members and slots)
    ParamRegistry m_params;

    // Dynamic relationship matrix (non-registry: NxN dynamic UI)
    QWidget* m_relContainer = nullptr;
    QVBoxLayout* m_relVLayout = nullptr;
    std::vector<std::vector<QComboBox*>> m_relCombos;
    std::vector<std::vector<QLabel*>> m_relLabels;
    QGroupBox* m_relGroup = nullptr;

    QSpinBox* m_countSpin = nullptr;

    // Season label (non-registry: dynamic)
    QLabel* m_seasonLabel = nullptr;

    // Appearance module (per-flock, non-registry: color buttons + checkbox)
    QGroupBox* m_appearanceGroup = nullptr;
    QPushButton* m_flockColorBtn = nullptr;
    QPushButton* m_maleColorBtn = nullptr;
    QPushButton* m_femaleColorBtn = nullptr;
    QCheckBox* m_sexColorsCheck = nullptr;
    QCheckBox* m_invertHungerCheck = nullptr;
    QComboBox* m_spriteCombo = nullptr;
    QPushButton* m_spriteImportBtn = nullptr;
    QPushButton* m_spriteRefreshBtn = nullptr;
    QCheckBox* m_uprightCheck = nullptr;

    // Global settings (independent dock, non-registry)
    QCheckBox* m_wrapBoundaryCheck = nullptr;
    QCheckBox* m_globalHungerFlashCheck = nullptr;
    QSlider*   m_globalFlockCapSlider = nullptr;
    QLabel*    m_globalFlockCapLabel = nullptr;
    QSlider*   m_spriteScaleSlider = nullptr;
    QLabel*    m_spriteScaleLabel = nullptr;

    // Statistics panel (independent dock)
    QWidget* m_statsContent = nullptr;
    QVBoxLayout* m_statsLayout = nullptr;
    std::vector<QLabel*> m_statsFlockLabels;   // per-flock count labels
    QLabel* m_statsTotalLabel = nullptr;
    QLabel* m_statsPlantsLabel = nullptr;
    QLabel* m_statsTimeLabel = nullptr;
    int m_lastStatsFlockCount = 0;              // track rebuild need

    QPushButton* m_spawnBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;

    QString m_logPath;

    // Helper: show color picker and update color button display
    void pickColor(QPushButton* btn, float* r, float* g, float* b);
    void updateColorButtons();
    void initSprites();                     // scan image/ dir and load into renderer
    QStringList scanImageDirectory() const;  // list PNGs in image/ subdirectory
    void refreshSpriteCombo();               // rebuild combo from image/ dir files
};
