#include "ParamRegistry.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QStyle>

int ParamRegistry::reg(const ParamDef& def) {
    int idx = static_cast<int>(m_defs.size());
    m_defs.push_back(def);
    m_bindings.push_back({});
    return idx;
}

QGroupBox* ParamRegistry::buildGroup(const char* groupKey,
                                      const QString& groupStyle,
                                      const void* initialBaseFlock,
                                      const void* initialBasePlant,
                                      QWidget* parent) {
    // Collect all defs for this group
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(m_defs.size()); ++i) {
        if (std::string(m_defs[i].groupKey) == groupKey) {
            indices.push_back(i);
        }
    }
    if (indices.empty()) return nullptr;

    auto* box = new QGroupBox(parent);
    box->setStyleSheet(groupStyle);
    box->setCheckable(true);
    box->setChecked(true);

    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(6, 2, 6, 6);
    layout->setSpacing(1);

    for (int i : indices) {
        ParamDef& def = m_defs[i];
        ParamBinding& b = m_bindings[i];
        b.def = &def;

        // Choose initial base
        const void* base = def.isPlantParam ? initialBasePlant : initialBaseFlock;
        int initVal = (base != nullptr) ? def.floatToSlider(def.readFrom(base))
                                        : def.initialSliderVal;
        float initFloat = def.sliderToFloat(initVal);
        int decimals = def.displayDecimals();

        // ---- Row: [name label | stretch | value label] ----
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);

        b.label = new QLabel(QString::fromUtf8(def.nameCN));
        b.label->setStyleSheet("color: #888; font-size: 11px;");
        row->addWidget(b.label);

        row->addStretch();

        b.valueLabel = new QLabel();
        b.valueLabel->setStyleSheet("color: #aaa; font-size: 11px; font-weight: bold; min-width: 40px;");
        b.valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (decimals == 0)
            b.valueLabel->setText(QString::number(static_cast<int>(initFloat)));
        else
            b.valueLabel->setText(QString::number(initFloat, 'f', decimals));
        row->addWidget(b.valueLabel);

        layout->addLayout(row);

        // ---- Slider with auto step sizes ----
        b.slider = new QSlider(Qt::Horizontal);
        b.slider->setRange(def.sliderMin, def.sliderMax);
        b.slider->setValue(initVal);
        b.slider->setFixedHeight(18);
        b.slider->setSingleStep(def.autoSingleStep());
        b.slider->setPageStep(def.autoPageStep());
        layout->addWidget(b.slider);

        // Capture pointers by value for connection lambdas
        ParamDef* defPtr = &def;
        QSlider* sl = b.slider;
        QLabel* valLbl = b.valueLabel;

        // ---- Right-click: manual value input ----
        sl->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(sl, &QSlider::customContextMenuRequested,
            [sl, defPtr, box](const QPoint& /*pos*/) {
                float curFloat = defPtr->sliderToFloat(sl->value());
                float minFloat = defPtr->sliderToFloat(defPtr->sliderMin);
                float maxFloat = defPtr->sliderToFloat(defPtr->sliderMax);
                int dec = defPtr->displayDecimals();
                bool ok = false;
                double input = QInputDialog::getDouble(box,
                    QString::fromUtf8(defPtr->nameCN),
                    QString("Value (%1 - %2):")
                        .arg(minFloat, 0, 'f', dec)
                        .arg(maxFloat, 0, 'f', dec),
                    curFloat, minFloat, maxFloat, dec, &ok);
                if (ok) {
                    int newVal = defPtr->floatToSlider(static_cast<float>(input));
                    newVal = std::max(defPtr->sliderMin,
                                      std::min(defPtr->sliderMax, newVal));
                    sl->setValue(newVal);
                }
            });

        // ---- Update value label + tooltip on slider change ----
        QObject::connect(sl, &QSlider::valueChanged, [sl, defPtr, valLbl, decimals](int v) {
            float fv = defPtr->sliderToFloat(v);
            // Update persistent value label
            if (decimals == 0)
                valLbl->setText(QString::number(static_cast<int>(fv)));
            else
                valLbl->setText(QString::number(fv, 'f', decimals));
            // Update tooltip
            QString name = QString::fromUtf8(defPtr->nameCN);
            int colon = name.indexOf(':');
            if (colon > 0) name = name.left(colon);
            if (decimals == 0)
                name += QString(" = %1").arg(static_cast<int>(fv));
            else
                name += QString(" = %1").arg(fv, 0, 'f', decimals);
            sl->setToolTip(name);
        });

        // Set initial tooltip
        {
            QString initName = QString::fromUtf8(def.nameCN);
            int colon = initName.indexOf(':');
            if (colon > 0) initName = initName.left(colon);
            if (decimals == 0)
                initName += QString(" = %1").arg(static_cast<int>(initFloat));
            else
                initName += QString(" = %1").arg(initFloat, 0, 'f', decimals);
            sl->setToolTip(initName);
        }
    }

    m_groups[groupKey] = box;
    return box;
}

void ParamRegistry::refresh(const void* flockBase, const void* plantBase) {
    for (auto& b : m_bindings) {
        if (!b.slider || !b.def) continue;
        const void* base = b.def->isPlantParam ? plantBase : flockBase;
        int newVal = b.def->floatToSlider(b.def->readFrom(base));
        b.slider->blockSignals(true);
        b.slider->setValue(newVal);
        b.slider->blockSignals(false);

        // Also update value label (signals were blocked so valueChanged didn't fire)
        if (b.valueLabel) {
            float fv = b.def->sliderToFloat(newVal);
            int dec = b.def->displayDecimals();
            if (dec == 0)
                b.valueLabel->setText(QString::number(static_cast<int>(fv)));
            else
                b.valueLabel->setText(QString::number(fv, 'f', dec));
        }
    }
}

QGroupBox* ParamRegistry::groupBox(const char* groupKey) const {
    auto it = m_groups.find(groupKey);
    return (it != m_groups.end()) ? it->second : nullptr;
}

void ParamRegistry::setGroupTitle(QGroupBox* box, const QString& title) {
    if (!box) return;
    box->setTitle(title);
    box->style()->unpolish(box);
    box->style()->polish(box);
}
