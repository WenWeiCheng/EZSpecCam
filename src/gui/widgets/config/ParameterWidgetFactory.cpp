#include "ParameterWidgetFactory.h"

#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QWheelEvent>

namespace {
class WheelBlocker : public QObject
{
public:
    explicit WheelBlocker(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel) {
            return true;
        }
        return QObject::eventFilter(obj, event);
    }
};

void installWheelBlocker(QWidget *widget)
{
    if (!widget) {
        return;
    }
    WheelBlocker *blocker = new WheelBlocker();
    widget->installEventFilter(blocker);
    for (auto child : widget->findChildren<QWidget*>()) {
        child->installEventFilter(blocker);
    }
}
}

QWidget *ParameterWidgetFactory::createWidget(const ParameterDefinition &def)
{
    switch (def.type) {
    case ParameterType::FloatRange:
        return createFloatRangeWidget(def);
    case ParameterType::FloatCollection:
        return createFloatCollectionWidget(def);
    case ParameterType::IntRange:
        return createIntRangeWidget(def);
    case ParameterType::IntCollection:
        return createIntCollectionWidget(def);
    case ParameterType::String:
        return createStringWidget(def);
    case ParameterType::StringCollection:
        return createStringCollectionWidget(def);
    case ParameterType::Boolean:
        return createBooleanWidget(def);
    default:
        return nullptr;
    }
}

QVariant ParameterWidgetFactory::getWidgetValue(QWidget *widget, ParameterType type)
{
    switch (type) {
    case ParameterType::FloatRange: {
        bool hasUnitRange = widget->property("hasUnitRange").toBool();
        if (hasUnitRange) {
            QComboBox *unitCombo = widget->findChild<QComboBox *>("unitComboBox");
            QDoubleSpinBox *spinBox = widget->findChild<QDoubleSpinBox *>("spinBox");
            if (unitCombo && spinBox) {
                int unitIndex = unitCombo->currentIndex();
                QVector<double> unitRange = widget->property("unitRange").value<QVector<double>>();
                double displayValue = spinBox->value();
                double rawValue;
                if (unitIndex == 0 || unitRange.isEmpty()) {
                    rawValue = displayValue;
                } else if (unitIndex > 0 && unitIndex <= unitRange.size()) {
                    rawValue = displayValue * unitRange[unitIndex - 1];
                } else {
                    rawValue = displayValue;
                }
                return rawValue;
            }
        }
        QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox *>(widget);
        if (spinBox) {
            return spinBox->value();
        }
        QSlider *slider = qobject_cast<QSlider *>(widget);
        if (slider) {
            return static_cast<double>(slider->value());
        }
        // Handle container widget with layout
        if (widget->layout()) {
            for (int i = 0; i < widget->layout()->count(); ++i) {
                QWidget *itemWidget = widget->layout()->itemAt(i)->widget();
                if (itemWidget) {
                    QVariant result = getWidgetValue(itemWidget, type);
                    if (result.isValid()) {
                        return result;
                    }
                }
            }
        }
        break;
    }
    case ParameterType::IntRange: {
        bool hasUnitRange = widget->property("hasUnitRange").toBool();
        if (hasUnitRange) {
            QComboBox *unitCombo = widget->findChild<QComboBox *>("unitComboBox");
            QSpinBox *spinBox = widget->findChild<QSpinBox *>("spinBox");
            if (unitCombo && spinBox) {
                int unitIndex = unitCombo->currentIndex();
                QVector<double> unitRange = widget->property("unitRange").value<QVector<double>>();
                int displayValue = spinBox->value();
                double rawValue;
                if (unitIndex == 0 || unitRange.isEmpty()) {
                    rawValue = displayValue;
                } else if (unitIndex > 0 && unitIndex <= unitRange.size()) {
                    rawValue = displayValue * unitRange[unitIndex - 1];
                } else {
                    rawValue = displayValue;
                }
                return static_cast<int>(rawValue);
            }
        }
        QSpinBox *spinBox = qobject_cast<QSpinBox *>(widget);
        if (spinBox) {
            return spinBox->value();
        }
        break;
    }
    case ParameterType::IntCollection:
    case ParameterType::FloatCollection:
    case ParameterType::StringCollection: {
        QComboBox *comboBox = qobject_cast<QComboBox *>(widget);
        if (comboBox) {
            return comboBox->currentData();
        }
        break;
    }
    case ParameterType::String: {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(widget);
        if (lineEdit) {
            return lineEdit->text();
        }
        QLabel *label = qobject_cast<QLabel *>(widget);
        if (label) {
            return label->text();
        }
        break;
    }
    case ParameterType::Boolean: {
        QCheckBox *checkBox = qobject_cast<QCheckBox *>(widget);
        if (checkBox) {
            return checkBox->isChecked();
        }
        break;
    }
    default:
        break;
    }
    return QVariant();
}

void ParameterWidgetFactory::setWidgetValue(QWidget *widget, const QVariant &value, ParameterType type)
{
    switch (type) {
    case ParameterType::FloatRange: {
        bool hasUnitRange = widget->property("hasUnitRange").toBool();
        if (hasUnitRange) {
            QComboBox *unitCombo = widget->findChild<QComboBox *>("unitComboBox");
            QDoubleSpinBox *spinBox = widget->findChild<QDoubleSpinBox *>("spinBox");
            if (unitCombo && spinBox) {
                double rawValue = value.toDouble();
                QVector<double> unitRange = widget->property("unitRange").value<QVector<double>>();
                int unitIndex;
                if (unitRange.isEmpty()) {
                    unitIndex = 0;
                } else {
                    for (int i = 0; i < unitRange.size(); ++i) {
                        if (rawValue < unitRange[i]) {
                            unitIndex = i;
                            break;
                        }
                        unitIndex = i + 1;
                    }
                }
                unitCombo->blockSignals(true);
                unitCombo->setCurrentIndex(unitIndex);
                unitCombo->blockSignals(false);
                double displayValue;
                if (unitIndex == 0 || unitRange.isEmpty()) {
                    displayValue = rawValue;
                } else if (unitIndex > 0 && unitIndex <= unitRange.size()) {
                    displayValue = rawValue / unitRange[unitIndex - 1];
                } else {
                    displayValue = rawValue;
                }
                spinBox->setValue(displayValue);
                QSlider *slider = widget->findChild<QSlider *>("slider");
                if (slider) {
                    slider->setValue(static_cast<int>(displayValue));
                }
                return;
            }
        }
        QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox *>(widget);
        if (spinBox) {
            spinBox->setValue(value.toDouble());
            return;
        }
        QSlider *slider = qobject_cast<QSlider *>(widget);
        if (slider) {
            slider->setValue(value.toInt());
            return;
        }
        QHBoxLayout *layout = qobject_cast<QHBoxLayout *>(widget->layout());
        if (layout) {
            for (int i = 0; i < layout->count(); ++i) {
                QWidget *itemWidget = layout->itemAt(i)->widget();
                if (itemWidget) {
                    setWidgetValue(itemWidget, value, type);
                }
            }
        }
        break;
    }
    case ParameterType::IntRange: {
        bool hasUnitRange = widget->property("hasUnitRange").toBool();
        if (hasUnitRange) {
            QComboBox *unitCombo = widget->findChild<QComboBox *>("unitComboBox");
            QSpinBox *spinBox = widget->findChild<QSpinBox *>("spinBox");
            if (unitCombo && spinBox) {
                double rawValue = value.toDouble();
                QVector<double> unitRange = widget->property("unitRange").value<QVector<double>>();
                int unitIndex;
                if (unitRange.isEmpty()) {
                    unitIndex = 0;
                } else {
                    for (int i = 0; i < unitRange.size(); ++i) {
                        if (rawValue < unitRange[i]) {
                            unitIndex = i;
                            break;
                        }
                        unitIndex = i + 1;
                    }
                }
                unitCombo->blockSignals(true);
                unitCombo->setCurrentIndex(unitIndex);
                unitCombo->blockSignals(false);
                double displayValue;
                if (unitIndex == 0 || unitRange.isEmpty()) {
                    displayValue = rawValue;
                } else if (unitIndex > 0 && unitIndex <= unitRange.size()) {
                    displayValue = rawValue / unitRange[unitIndex - 1];
                } else {
                    displayValue = rawValue;
                }
                spinBox->setValue(static_cast<int>(displayValue));
                QSlider *slider = widget->findChild<QSlider *>("slider");
                if (slider) {
                    slider->setValue(static_cast<int>(displayValue));
                }
                return;
            }
        }
        QSpinBox *spinBox = qobject_cast<QSpinBox *>(widget);
        if (spinBox) {
            spinBox->setValue(value.toInt());
        }
        break;
    }
    case ParameterType::IntCollection:
    case ParameterType::FloatCollection:
    case ParameterType::StringCollection: {
        QComboBox *comboBox = qobject_cast<QComboBox *>(widget);
        if (comboBox) {
            int index = comboBox->findData(value);
            if (index >= 0) {
                comboBox->setCurrentIndex(index);
            }
        }
        break;
    }
    case ParameterType::String: {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(widget);
        if (lineEdit) {
            lineEdit->setText(value.toString());
        }
        break;
    }
    case ParameterType::Boolean: {
        QCheckBox *checkBox = qobject_cast<QCheckBox *>(widget);
        if (checkBox) {
            checkBox->setChecked(value.toBool());
        }
        break;
    }
    default:
        break;
    }
}

bool ParameterWidgetFactory::isModified(QWidget *widget, const QVariant &originalValue, ParameterType type)
{
    QVariant currentValue = getWidgetValue(widget, type);
    if (type == ParameterType::FloatRange) {
        return qAbs(currentValue.toDouble() - originalValue.toDouble()) > 0.0001;
    }
    return currentValue != originalValue;
}

QWidget *ParameterWidgetFactory::createFloatRangeWidget(const ParameterDefinition &def)
{
    if (def.constraint.hasUnitRange()) {
        return createFloatRangeWidgetWithUnit(def);
    }

    QWidget *container = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QDoubleSpinBox *spinBox = new QDoubleSpinBox(container);
    spinBox->setObjectName("spinBox");
    spinBox->setRange(def.constraint.minValue, def.constraint.maxValue);
    spinBox->setSingleStep(def.constraint.step > 0 ? def.constraint.step : 1.0);
    spinBox->setDecimals(3);
    if (def.defaultValue.isValid()) {
        spinBox->setValue(def.defaultValue.toDouble());
    }
    if (!def.description.isEmpty()) {
        spinBox->setToolTip(def.description);
    }
    installWheelBlocker(spinBox);
    layout->addWidget(spinBox, 1);

    if (def.constraint.maxValue - def.constraint.minValue > def.constraint.step * 10) {
        QSlider *slider = new QSlider(Qt::Horizontal, container);
        slider->setObjectName("slider");
        slider->setRange(static_cast<int>(def.constraint.minValue),
                        static_cast<int>(def.constraint.maxValue));
        if (!def.description.isEmpty()) {
            slider->setToolTip(def.description);
        }
        installWheelBlocker(slider);
        layout->addWidget(slider, 2);

        QObject::connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                        slider, &QSlider::setValue);
        QObject::connect(slider, &QSlider::valueChanged,
                        spinBox, &QDoubleSpinBox::setValue);
        if (def.defaultValue.isValid()) {
            slider->setValue(static_cast<int>(def.defaultValue.toDouble()));
        }
    }

    return container;
}

QWidget *ParameterWidgetFactory::createFloatRangeWidgetWithUnit(const ParameterDefinition &def)
{
    QWidget *container = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QComboBox *unitCombo = new QComboBox(container);
    unitCombo->setObjectName("unitComboBox");
    for (const QString &u : def.constraint.unit) {
        unitCombo->addItem(u);
    }
    installWheelBlocker(unitCombo);
    layout->addWidget(unitCombo);

    QDoubleSpinBox *spinBox = new QDoubleSpinBox(container);
    spinBox->setObjectName("spinBox");
    if (!def.description.isEmpty()) {
        spinBox->setToolTip(def.description);
    }
    installWheelBlocker(spinBox);
    layout->addWidget(spinBox, 1);

    QSlider *slider = new QSlider(Qt::Horizontal, container);
    slider->setObjectName("slider");
    if (!def.description.isEmpty()) {
        slider->setToolTip(def.description);
    }
    installWheelBlocker(slider);
    layout->addWidget(slider, 2);

    container->setProperty("hasUnitRange", true);
    container->setProperty("unitRange", QVariant::fromValue(def.constraint.unitRange));
    container->setProperty("minValue", def.constraint.minValue);
    container->setProperty("maxValue", def.constraint.maxValue);

    auto updateRanges = [=](int unitIndex) {
        double rangeMin, rangeMax;
        if (unitIndex == 0) {
            rangeMin = def.constraint.minValue;
            rangeMax = def.constraint.unitRange[0] - 1;
        } else if (unitIndex < def.constraint.unitRange.size()) {
            rangeMin = 1;
            rangeMax = (def.constraint.unitRange[unitIndex] / def.constraint.unitRange[unitIndex - 1]) - 1;
            if (rangeMax > def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1]) {
                rangeMax = def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1];
            }
        } else {
            rangeMin = 1;
            rangeMax = def.constraint.maxValue / def.constraint.unitRange.last();
        }
        spinBox->setRange(rangeMin, rangeMax);
        spinBox->setSingleStep(def.constraint.step > 0 ? def.constraint.step : 1.0);
        spinBox->setDecimals(3);
        slider->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
    };

    auto updateFromRawValue = [=](double rawValue) {
        int unitIndex = def.constraint.getUnitIndex(rawValue);
        unitCombo->blockSignals(true);
        unitCombo->setCurrentIndex(unitIndex);
        unitCombo->blockSignals(false);
        updateRanges(unitIndex);
        double displayValue = def.constraint.toDisplayValue(rawValue, unitIndex);
        spinBox->setValue(displayValue);
        slider->setValue(static_cast<int>(displayValue));
    };

    QObject::connect(unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    [=](int unitIndex) {
                        updateRanges(unitIndex);
                        double rawValue = def.constraint.toRawValue(spinBox->value(), unitIndex);
                        slider->blockSignals(true);
                        slider->setValue(static_cast<int>(spinBox->value()));
                        slider->blockSignals(false);
                    });

    QObject::connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    [=](double displayValue) {
                        int unitIndex = unitCombo->currentIndex();
                        double rawValue = def.constraint.toRawValue(displayValue, unitIndex);
                        slider->blockSignals(true);
                        slider->setValue(static_cast<int>(displayValue));
                        slider->blockSignals(false);
                    });

    QObject::connect(slider, &QSlider::valueChanged,
                    [=](int value) {
                        spinBox->setValue(value);
                    });

    if (def.defaultValue.isValid()) {
        updateFromRawValue(def.defaultValue.toDouble());
    } else {
        updateRanges(0);
    }

    return container;
}

// TODO: ParameterWidgetFactory::createIntRangeWidget need a slider as well
QWidget *ParameterWidgetFactory::createIntRangeWidget(const ParameterDefinition &def)
{
    if (def.constraint.hasUnitRange()) {
        return createIntRangeWidgetWithUnit(def);
    }

    QSpinBox *spinBox = new QSpinBox();
    spinBox->setRange(static_cast<int>(def.constraint.minValue),
                     static_cast<int>(def.constraint.maxValue));
    spinBox->setSingleStep(def.constraint.step > 0 ? static_cast<int>(def.constraint.step) : 1);
    if (def.defaultValue.isValid()) {
        spinBox->setValue(def.defaultValue.toInt());
    }
    if (!def.description.isEmpty()) {
        spinBox->setToolTip(def.description);
    }
    installWheelBlocker(spinBox);
    return spinBox;
}

QWidget *ParameterWidgetFactory::createIntRangeWidgetWithUnit(const ParameterDefinition &def)
{
    QWidget *container = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QComboBox *unitCombo = new QComboBox(container);
    unitCombo->setObjectName("unitComboBox");
    for (const QString &u : def.constraint.unit) {
        unitCombo->addItem(u);
    }
    installWheelBlocker(unitCombo);
    layout->addWidget(unitCombo);

    QSpinBox *spinBox = new QSpinBox(container);
    spinBox->setObjectName("spinBox");
    if (!def.description.isEmpty()) {
        spinBox->setToolTip(def.description);
    }
    installWheelBlocker(spinBox);
    layout->addWidget(spinBox, 1);

    QSlider *slider = new QSlider(Qt::Horizontal, container);
    slider->setObjectName("slider");
    if (!def.description.isEmpty()) {
        slider->setToolTip(def.description);
    }
    installWheelBlocker(slider);
    layout->addWidget(slider, 2);

    container->setProperty("hasUnitRange", true);
    container->setProperty("unitRange", QVariant::fromValue(def.constraint.unitRange));
    container->setProperty("minValue", def.constraint.minValue);
    container->setProperty("maxValue", def.constraint.maxValue);
    container->setProperty("isIntRange", true);

    auto updateRanges = [=](int unitIndex) {
        double rangeMin, rangeMax;
        if (unitIndex == 0) {
            rangeMin = def.constraint.minValue;
            rangeMax = def.constraint.unitRange[0] - 1;
        } else if (unitIndex < def.constraint.unitRange.size()) {
            rangeMin = 1;
            rangeMax = (def.constraint.unitRange[unitIndex] / def.constraint.unitRange[unitIndex - 1]) - 1;
            if (rangeMax > def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1]) {
                rangeMax = def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1];
            }
        } else {
            rangeMin = 1;
            rangeMax = def.constraint.maxValue / def.constraint.unitRange.last();
        }
        spinBox->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
        spinBox->setSingleStep(def.constraint.step > 0 ? static_cast<int>(def.constraint.step) : 1);
        slider->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
    };

    auto updateFromRawValue = [=](double rawValue) {
        int unitIndex = def.constraint.getUnitIndex(rawValue);
        unitCombo->blockSignals(true);
        unitCombo->setCurrentIndex(unitIndex);
        unitCombo->blockSignals(false);
        updateRanges(unitIndex);
        double displayValue = def.constraint.toDisplayValue(rawValue, unitIndex);
        spinBox->setValue(static_cast<int>(displayValue));
        slider->setValue(static_cast<int>(displayValue));
    };

    QObject::connect(unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    [=](int unitIndex) {
                        updateRanges(unitIndex);
                        double rawValue = def.constraint.toRawValue(spinBox->value(), unitIndex);
                        slider->blockSignals(true);
                        slider->setValue(spinBox->value());
                        slider->blockSignals(false);
                    });

    QObject::connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                    [=](int displayValue) {
                        int unitIndex = unitCombo->currentIndex();
                        double rawValue = def.constraint.toRawValue(displayValue, unitIndex);
                        slider->blockSignals(true);
                        slider->setValue(displayValue);
                        slider->blockSignals(false);
                    });

    QObject::connect(slider, &QSlider::valueChanged,
                    [=](int value) {
                        spinBox->setValue(value);
                    });

    if (def.defaultValue.isValid()) {
        updateFromRawValue(def.defaultValue.toDouble());
    } else {
        updateRanges(0);
    }

    return container;
}

QWidget *ParameterWidgetFactory::createIntCollectionWidget(const ParameterDefinition &def)
{
    QComboBox *comboBox = new QComboBox();
    for (const QVariant &val : def.constraint.validValues) {
        comboBox->addItem(QString::number(val.toInt()), val);
    }
    if (def.defaultValue.isValid()) {
        int index = comboBox->findData(def.defaultValue);
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
        }
    }
    if (!def.description.isEmpty()) {
        comboBox->setToolTip(def.description);
    }
    installWheelBlocker(comboBox);
    return comboBox;
}

QWidget *ParameterWidgetFactory::createEnumWidget(const ParameterDefinition &def)
{
    QComboBox *comboBox = new QComboBox();
    for (const QVariant &val : def.constraint.validValues) {
        QString displayText = val.toString();
        comboBox->addItem(displayText, val);
    }
    if (def.defaultValue.isValid()) {
        int index = comboBox->findData(def.defaultValue);
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
        }
    }
    if (!def.description.isEmpty()) {
        comboBox->setToolTip(def.description);
    }
    installWheelBlocker(comboBox);
    return comboBox;
}

QWidget *ParameterWidgetFactory::createFloatCollectionWidget(const ParameterDefinition &def)
{
    QComboBox *comboBox = new QComboBox();
    for (const QVariant &val : def.constraint.validValues) {
        comboBox->addItem(QString::number(val.toDouble(), 'f', 3), val);
    }
    if (def.defaultValue.isValid()) {
        int index = comboBox->findData(def.defaultValue);
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
        }
    }
    if (!def.description.isEmpty()) {
        comboBox->setToolTip(def.description);
    }
    installWheelBlocker(comboBox);
    return comboBox;
}

QWidget *ParameterWidgetFactory::createStringWidget(const ParameterDefinition &def)
{
    if (def.isReadOnly) {
        QLabel *label = new QLabel(def.defaultValue.toString());
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (!def.description.isEmpty()) {
            label->setToolTip(def.description);
        }
        return label;
    } else {
        QLineEdit *lineEdit = new QLineEdit();
        if (def.defaultValue.isValid()) {
            lineEdit->setText(def.defaultValue.toString());
        }
        if (!def.description.isEmpty()) {
            lineEdit->setToolTip(def.description);
        }
        return lineEdit;
    }
}

QWidget *ParameterWidgetFactory::createStringCollectionWidget(const ParameterDefinition &def)
{
    QWidget *widget = createEnumWidget(def);
    if (!def.description.isEmpty()) {
        widget->setToolTip(def.description);
    }
    return widget;
}

QWidget *ParameterWidgetFactory::createBooleanWidget(const ParameterDefinition &def)
{
    QCheckBox *checkBox = new QCheckBox(def.displayName);
    if (def.defaultValue.isValid()) {
        checkBox->setChecked(def.defaultValue.toBool());
    }
    if (!def.description.isEmpty()) {
        checkBox->setToolTip(def.description);
    }
    return checkBox;
}