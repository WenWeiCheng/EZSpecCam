#ifndef PARAMETERWIDGETFACTORY_H
#define PARAMETERWIDGETFACTORY_H

#include <QWidget>
#include <QVariant>

#include "../../../core/CameraTypes.h"

/**
 * @brief Factory class for creating Qt widgets from parameter definitions
 *
 * Creates appropriate Qt widgets based on parameter type and constraints:
 * FloatRange, IntRange, IntCollection, FloatCollection, Enum, String, StringCollection, Boolean.
 *
 * @see ParameterDefinition
 * @see ParameterType
 */
class ParameterWidgetFactory
{
public:
    static QWidget *createWidget(const ParameterDefinition &def);
    static QVariant getWidgetValue(QWidget *widget, ParameterType type);
    static void setWidgetValue(QWidget *widget, const QVariant &value, ParameterDefinition def);
    static bool isModified(QWidget *widget, const QVariant &originalValue, ParameterType type);

private:
    static QWidget *createFloatRangeWidget(const ParameterDefinition &def);
    static QWidget *createFloatRangeWidgetWithUnit(const ParameterDefinition &def);
    static QWidget *createIntRangeWidget(const ParameterDefinition &def);
    static QWidget *createIntRangeWidgetWithUnit(const ParameterDefinition &def);
    static QWidget *createIntCollectionWidget(const ParameterDefinition &def);
    static QWidget *createEnumWidget(const ParameterDefinition &def);
    static QWidget *createFloatCollectionWidget(const ParameterDefinition &def);
    static QWidget *createStringWidget(const ParameterDefinition &def);
    static QWidget *createStringCollectionWidget(const ParameterDefinition &def);
    static QWidget *createBooleanWidget(const ParameterDefinition &def);

    // The chosen unit index is stored on the container widget as the "currentUnitIndex"
    // property so the QComboBox::currentIndexChanged slot can read the previous index.
    static void setFloatRangeWidgetWithUnitValue(QWidget *widget, const ParameterDefinition &def,
                                                 double rawValue);
    static void setIntRangeWidgetWithUnitValue(QWidget *widget, const ParameterDefinition &def,
                                               double rawValue);
};

#endif // PARAMETERWIDGETFACTORY_H