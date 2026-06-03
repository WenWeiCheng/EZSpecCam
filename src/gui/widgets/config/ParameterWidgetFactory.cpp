#include "ParameterWidgetFactory.h"
#include "CameraTypes.h"

#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QWheelEvent>
#include <QtWidgets>
#include <qslider.h>

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
    WheelBlocker *blocker = new WheelBlocker(widget);
    widget->installEventFilter(blocker);
    for (auto child : widget->findChildren<QWidget*>()) {
        child->installEventFilter(blocker);
    }
}

double correctStep(double value, double min, double step)
{
    return min + qRound((value - min) / step) * step;
}
}

QWidget *ParameterWidgetFactory::createWidget(const ParameterDefinition &def)
{
    if (def.isReadOnly) {
        QLabel *label = new QLabel(def.defaultValue.toString());
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (!def.description.isEmpty()) {
            label->setToolTip(def.description);
        }
        return label;
    }

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
        if (QDoubleSpinBox *spinBox = widget->findChild<QDoubleSpinBox *>("spinBox")) {
            return spinBox->value();
        }
        if (QSlider *slider = widget->findChild<QSlider *>("slider")) {
            return static_cast<double>(slider->value());
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
        if (QSpinBox *spinBox = widget->findChild<QSpinBox *>("spinBox")) {
            return spinBox->value();
        }
        if (QSlider *slider = widget->findChild<QSlider *>("slider")) {
            return static_cast<int>(slider->value());
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

void ParameterWidgetFactory::setWidgetValue(QWidget *widget, const QVariant &value, ParameterDefinition def)
{
    switch (def.type) {
    case ParameterType::FloatRange: {
        if (widget->property("hasUnitRange").toBool()
            && widget->findChild<QComboBox *>("unitComboBox")
            && widget->findChild<QDoubleSpinBox *>("spinBox")) {
            setFloatRangeWidgetWithUnitValue(widget, def, value.toDouble());
            return;
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
                    setWidgetValue(itemWidget, value, def);
                }
            }
        }
        break;
    }
    case ParameterType::IntRange: {
        if (widget->property("hasUnitRange").toBool()
            && widget->findChild<QComboBox *>("unitComboBox")
            && widget->findChild<QSpinBox *>("spinBox")) {
            setIntRangeWidgetWithUnitValue(widget, def, value.toDouble());
            return;
        }
        QSpinBox *spinBox = qobject_cast<QSpinBox *>(widget);
        if (spinBox) {
            spinBox->setValue(value.toInt());
        }
        QSlider *slider = qobject_cast<QSlider *>(widget);
        if(slider) {
            slider->setValue(value.toInt());
            return;
        }
        QHBoxLayout *layout = qobject_cast<QHBoxLayout *>(widget->layout());
        if(layout) {
            for(int i=0; i<layout->count(); ++i){
                QWidget *itemWidget = layout->itemAt(i)->widget();
                if(itemWidget){
                    setWidgetValue(itemWidget, value, def);
                }
            }
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
    spinBox->setSingleStep(def.constraint.step > 0 ? def.constraint.step : 1.0);
    spinBox->setDecimals(3);
    spinBox->setRange(def.constraint.minValue, def.constraint.maxValue);
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

        QObject::connect(spinBox, &QDoubleSpinBox::editingFinished,
                        [=]() {
                            double minVal = def.constraint.minValue;
                            double maxVal = def.constraint.maxValue;
                            double step = def.constraint.step > 0 ? def.constraint.step : 1.0;
                            double clamped = qBound(minVal, spinBox->value(), maxVal);
                            double corrected = correctStep(clamped, minVal, step);
                            if (qAbs(corrected - spinBox->value()) > 1e-9) {
                                spinBox->setValue(corrected);
                            }
                            slider->setValue(static_cast<int>(corrected));
                        });
        QObject::connect(slider, &QSlider::valueChanged,
                        [=](int value) {
                            double minVal = def.constraint.minValue;
                            double step = def.constraint.step > 0 ? def.constraint.step : 1.0;
                            double corrected = correctStep(value, minVal, step);
                            spinBox->setValue(corrected);
                        });
        if (def.defaultValue.isValid()) {
            slider->setValue(static_cast<int>(def.defaultValue.toDouble()));
        }
    } else {
        QObject::connect(spinBox, &QDoubleSpinBox::editingFinished,
                        [=]() {
                            double minVal = def.constraint.minValue;
                            double maxVal = def.constraint.maxValue;
                            double step = def.constraint.step > 0 ? def.constraint.step : 1.0;
                            double corrected = correctStep(qBound(minVal, spinBox->value(), maxVal),
                                                            minVal, step);
                            if (qAbs(corrected - spinBox->value()) > 1e-9) {
                                spinBox->setValue(corrected);
                            }
                        });
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
        int decimals;
        if (unitIndex == 0) {
            rangeMin = def.constraint.minValue;
            rangeMax = def.constraint.maxValue;
            decimals = 0;
        } else {
            rangeMin = 1;
            rangeMax = def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1];
            decimals = std::log10(static_cast<double>(def.constraint.unitRange[unitIndex - 1]));
        }
        double displayStep = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                 && unitIndex <= def.constraint.unitRange.size())
                                 ? def.constraint.step / def.constraint.unitRange[unitIndex - 1]
                                 : def.constraint.step;
        spinBox->setSingleStep(displayStep);
        spinBox->setDecimals(decimals);
        spinBox->setRange(rangeMin, rangeMax);
        slider->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
        slider->setSingleStep(displayStep);
    };

    container->setProperty("currentUnitIndex",
                            def.constraint.getUnitIndex(def.defaultValue.isValid()
                                                             ? def.defaultValue.toDouble()
                                                             : 0.0));

    QObject::connect(unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    [=](int unitIndex) {
                        int oldIndex = container->property("currentUnitIndex").toInt();
                        double rawValue = def.constraint.toRawValue(spinBox->value(), oldIndex);
                        container->setProperty("currentUnitIndex", unitIndex);
                        updateRanges(unitIndex);
                        double displayValue = def.constraint.toDisplayValue(rawValue, unitIndex);
                        spinBox->setValue(displayValue);
                        slider->blockSignals(true);
                        slider->setValue(static_cast<int>(displayValue));
                        slider->blockSignals(false);
                    });

    // Correct and sync only when user finishes editing (click out or press Enter)
    QObject::connect(spinBox, &QDoubleSpinBox::editingFinished,
                    [=]() {
                        int unitIndex = unitCombo->currentIndex();
                        double displayStep = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                                 && unitIndex <= def.constraint.unitRange.size())
                                                 ? def.constraint.step / def.constraint.unitRange[unitIndex - 1]
                                                 : def.constraint.step;
                        double rangeMin = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                              && unitIndex <= def.constraint.unitRange.size())
                                              ? def.constraint.minValue / def.constraint.unitRange[unitIndex - 1]
                                              : def.constraint.minValue;
                        double rangeMax = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                              && unitIndex <= def.constraint.unitRange.size())
                                              ? def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1]
                                              : def.constraint.maxValue;
                        double clamped = qBound(rangeMin, spinBox->value(), rangeMax);
                        double corrected = rangeMin + qRound((clamped - rangeMin) / displayStep) * displayStep;
                        if (qAbs(corrected - spinBox->value()) > 1e-9) {
                            spinBox->setValue(corrected);
                        }
                        // int rawValue = static_cast<int>(def.constraint.toRawValue(corrected, unitIndex));
                        slider->blockSignals(true);
                        slider->setValue(corrected);
                        slider->blockSignals(false);
                    });

    QObject::connect(slider, &QSlider::valueChanged,
                    [=](int displayValue) {
                        int unitIndex = unitCombo->currentIndex();
                        double displayStep = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                                 && unitIndex <= def.constraint.unitRange.size())
                                                 ? def.constraint.step / def.constraint.unitRange[unitIndex - 1]
                                                 : def.constraint.step;
                        double rangeMin = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                              && unitIndex <= def.constraint.unitRange.size())
                                              ? def.constraint.minValue / def.constraint.unitRange[unitIndex - 1]
                                              : def.constraint.minValue;
                        double corrected = rangeMin + qRound((displayValue - rangeMin) / displayStep) * displayStep;
                        spinBox->setValue(corrected);
                                            });

    if (def.defaultValue.isValid()) {
        setFloatRangeWidgetWithUnitValue(container, def, def.defaultValue.toDouble());
    } else {
        updateRanges(0);
    }

    return container;
}

QWidget *ParameterWidgetFactory::createIntRangeWidget(const ParameterDefinition &def)
{
    if (def.constraint.hasUnitRange()) {
        return createIntRangeWidgetWithUnit(def);
    }

    int minVal = static_cast<int>(def.constraint.minValue);
    int maxVal = static_cast<int>(def.constraint.maxValue);
    int span = maxVal - minVal;

    if (span > 50) {
        QWidget *container = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        QSpinBox *spinBox = new QSpinBox(container);
        spinBox->setObjectName("spinBox");
        spinBox->setSingleStep(def.constraint.step > 0 ? static_cast<int>(def.constraint.step) : 1);
        spinBox->setRange(minVal, maxVal);
        if (def.defaultValue.isValid()) {
            spinBox->setValue(def.defaultValue.toInt());
        }
        if (!def.description.isEmpty()) {
            spinBox->setToolTip(def.description);
        }
        installWheelBlocker(spinBox);
        layout->addWidget(spinBox, 1);

        QSlider *slider = new QSlider(Qt::Horizontal, container);
        slider->setObjectName("slider");
        slider->setRange(minVal, maxVal);
        slider->setSingleStep(def.constraint.step > 0 ? static_cast<int>(def.constraint.step) : 1);
        if (!def.description.isEmpty()) {
            slider->setToolTip(def.description);
        }
        installWheelBlocker(slider);
        layout->addWidget(slider, 2);

        container->setProperty("isIntRange", true);

        QObject::connect(slider, &QSlider::valueChanged,
            [=](int value) {
                double step_d = def.constraint.step > 0 ? def.constraint.step : 1.0;
                double corrected_d = correctStep(value, def.constraint.minValue, step_d);
                spinBox->setValue(static_cast<int>(corrected_d));
                            });

        QObject::connect(spinBox, &QSpinBox::editingFinished,
            [=]() {
                double step_d = def.constraint.step > 0 ? def.constraint.step : 1.0;
                double clamped = qBound(static_cast<double>(minVal), static_cast<double>(spinBox->value()), static_cast<double>(maxVal));
                double corrected_d = correctStep(clamped, def.constraint.minValue, step_d);
                int corrected = static_cast<int>(corrected_d);
                if (corrected != spinBox->value()) {
                    spinBox->setValue(corrected);
                }
                slider->setValue(corrected);
            });

        return container;
    }

    QSpinBox *spinBox = new QSpinBox();
    spinBox->setSingleStep(def.constraint.step > 0 ? static_cast<int>(def.constraint.step) : 1);
    if (def.defaultValue.isValid()) {
        spinBox->setValue(def.defaultValue.toInt());
    }
    if (!def.description.isEmpty()) {
        spinBox->setToolTip(def.description);
    }
    installWheelBlocker(spinBox);
    QObject::connect(spinBox, &QSpinBox::editingFinished,
        [=]() {
            double minVal = def.constraint.minValue;
            double maxVal = def.constraint.maxValue;
            double step = def.constraint.step > 0 ? def.constraint.step : 1.0;
            int corrected = static_cast<int>(
                correctStep(qBound(minVal, static_cast<double>(spinBox->value()), maxVal), minVal, step));
            if (corrected != spinBox->value()) {
                spinBox->setValue(corrected);
            }
        });
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
            rangeMax = def.constraint.maxValue;
        } else {
            rangeMin = 1;
            rangeMax = def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1];
        }
        spinBox->setSingleStep(def.constraint.step > 0 ? static_cast<int>(def.constraint.step) : 1);
        spinBox->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
        slider->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
    };

    container->setProperty("currentUnitIndex",
                            def.constraint.getUnitIndex(def.defaultValue.isValid()
                                                             ? def.defaultValue.toDouble()
                                                             : 0.0));

    QObject::connect(unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    [=](int unitIndex) {
                        int oldIndex = container->property("currentUnitIndex").toInt();
                        double rawValue = def.constraint.toRawValue(spinBox->value(), oldIndex);
                        container->setProperty("currentUnitIndex", unitIndex);
                        updateRanges(unitIndex);
                        double displayValue = def.constraint.toDisplayValue(rawValue, unitIndex);
                        spinBox->setValue(static_cast<int>(displayValue));
                        slider->blockSignals(true);
                        slider->setValue(static_cast<int>(displayValue));
                        slider->blockSignals(false);
                    });

    QObject::connect(spinBox, &QSpinBox::editingFinished,
                    [=]() {
                        int unitIndex = unitCombo->currentIndex();
                        double displayStep = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                                 && unitIndex <= def.constraint.unitRange.size())
                                                 ? def.constraint.step / def.constraint.unitRange[unitIndex - 1]
                                                 : def.constraint.step;
                        double rangeMin = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                              && unitIndex <= def.constraint.unitRange.size())
                                              ? def.constraint.minValue / def.constraint.unitRange[unitIndex - 1]
                                              : def.constraint.minValue;
                        double rangeMax = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                              && unitIndex <= def.constraint.unitRange.size())
                                              ? def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1]
                                              : def.constraint.maxValue;
                        double clamped = qBound(rangeMin, static_cast<double>(spinBox->value()), rangeMax);
                        double corrected_d = rangeMin + qRound((clamped - rangeMin) / displayStep) * displayStep;
                        int corrected = static_cast<int>(corrected_d);
                        if (corrected != spinBox->value()) {
                            spinBox->setValue(corrected);
                        }
                        // double rawValue = def.constraint.toRawValue(corrected, unitIndex);
                        slider->blockSignals(true);
                        slider->setValue(corrected);
                        slider->blockSignals(false);
                    });

    QObject::connect(slider, &QSlider::valueChanged,
                    [=](int displayValue) {
                        int unitIndex = unitCombo->currentIndex();
                        double displayStep = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                                 && unitIndex <= def.constraint.unitRange.size())
                                                 ? def.constraint.step / def.constraint.unitRange[unitIndex - 1]
                                                 : def.constraint.step;
                        double rangeMin = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                                              && unitIndex <= def.constraint.unitRange.size())
                                              ? def.constraint.minValue / def.constraint.unitRange[unitIndex - 1]
                                              : def.constraint.minValue;
                        double corrected_d = rangeMin + qRound((displayValue - rangeMin) / displayStep) * displayStep;
                        spinBox->setValue(static_cast<int>(corrected_d));
                                            });

    if (def.defaultValue.isValid()) {
        setIntRangeWidgetWithUnitValue(container, def, def.defaultValue.toDouble());
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

void ParameterWidgetFactory::setFloatRangeWidgetWithUnitValue(QWidget *widget,
                                                               const ParameterDefinition &def,
                                                               double rawValue)
{
    QComboBox *unitCombo = widget->findChild<QComboBox *>("unitComboBox");
    QDoubleSpinBox *spinBox = widget->findChild<QDoubleSpinBox *>("spinBox");
    QSlider *slider = widget->findChild<QSlider *>("slider");
    if (!unitCombo || !spinBox) {
        return;
    }

    int unitIndex = def.constraint.getUnitIndex(rawValue);
    widget->setProperty("currentUnitIndex", unitIndex);

    unitCombo->blockSignals(true);
    unitCombo->setCurrentIndex(unitIndex);
    unitCombo->blockSignals(false);

    double rangeMin;
    double rangeMax;
    int decimals;
    if (unitIndex == 0) {
        rangeMin = def.constraint.minValue;
        rangeMax = def.constraint.maxValue;
        decimals = 0;
    } else {
        rangeMin = 1;
        rangeMax = def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1];
        decimals = static_cast<int>(std::log10(static_cast<double>(def.constraint.unitRange[unitIndex - 1])));
    }
    double displayStep = (unitIndex > 0 && !def.constraint.unitRange.isEmpty()
                             && unitIndex <= def.constraint.unitRange.size())
                             ? def.constraint.step / def.constraint.unitRange[unitIndex - 1]
                             : def.constraint.step;

    spinBox->setSingleStep(displayStep);
    spinBox->setDecimals(decimals);
    spinBox->setRange(rangeMin, rangeMax);
    if (slider) {
        slider->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
        slider->setSingleStep(displayStep);
    }

    double displayValue = def.constraint.toDisplayValue(rawValue, unitIndex);
    spinBox->setValue(displayValue);
    if (slider) {
        slider->setValue(static_cast<int>(displayValue));
    }
}

void ParameterWidgetFactory::setIntRangeWidgetWithUnitValue(QWidget *widget,
                                                             const ParameterDefinition &def,
                                                             double rawValue)
{
    QComboBox *unitCombo = widget->findChild<QComboBox *>("unitComboBox");
    QSpinBox *spinBox = widget->findChild<QSpinBox *>("spinBox");
    QSlider *slider = widget->findChild<QSlider *>("slider");
    if (!unitCombo || !spinBox) {
        return;
    }

    int unitIndex = def.constraint.getUnitIndex(rawValue);
    widget->setProperty("currentUnitIndex", unitIndex);

    unitCombo->blockSignals(true);
    unitCombo->setCurrentIndex(unitIndex);
    unitCombo->blockSignals(false);

    double rangeMin;
    double rangeMax;
    if (unitIndex == 0) {
        rangeMin = def.constraint.minValue;
        rangeMax = def.constraint.maxValue;
    } else {
        rangeMin = 1;
        rangeMax = def.constraint.maxValue / def.constraint.unitRange[unitIndex - 1];
    }
    int singleStep = def.constraint.step > 0 ? static_cast<int>(def.constraint.step) : 1;

    spinBox->setSingleStep(singleStep);
    spinBox->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
    if (slider) {
        slider->setRange(static_cast<int>(rangeMin), static_cast<int>(rangeMax));
        slider->setSingleStep(singleStep);
    }

    double displayValue = def.constraint.toDisplayValue(rawValue, unitIndex);
    spinBox->setValue(static_cast<int>(displayValue));
    if (slider) {
        slider->setValue(static_cast<int>(displayValue));
    }
}