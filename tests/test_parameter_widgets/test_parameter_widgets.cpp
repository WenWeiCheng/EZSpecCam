#include <QtTest/QtTest>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLocale>
#include <QSlider>

#include "ParameterWidgetFactory.h"

/**
 * Widget-level coverage for the unit-scaled parameter editor.
 *
 * The exposure parameter of the hk16011 camera is the reference case: 1 us of
 * raw resolution is 1e-6 in the "s" unit, so the editor has to be a float range.
 * When it was built as an integer range, committing a typed value cast the
 * scaled display value down (10 s -> 9.999999999999998 -> 9 s). These cases pin
 * the float presentation and the raw-value round-trip the driver now relies on.
 */
class TestParameterWidgets : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void test_unit_scaled_parameter_uses_float_widget();
    void test_typed_whole_seconds_survive();
    void test_typed_fractional_seconds_survive();
    void test_unit_switch_keeps_raw_value();

private:
    static ParameterDefinition exposureDefinition(double defaultValueUs);
    static QWidget *createExposure(double defaultValueUs);
    static void typeAndCommit(QDoubleSpinBox *spinBox, const QString &text);
};

void TestParameterWidgets::initTestCase()
{
    // QDoubleSpinBox parses through the locale; pin it so "1.5" is a valid input
    // regardless of the machine's LANG.
    QLocale::setDefault(QLocale::c());
}

ParameterDefinition TestParameterWidgets::exposureDefinition(double defaultValueUs)
{
    // The shape Hk16011Driver::buildParameterTable() + applyParameterMetadata()
    // produce for exposure_time_us: the firmware type is Int with a 1 us step,
    // presented as a float range because the metadata adds a unit selector.
    ParameterDefinition def;
    def.name = QStringLiteral("exposure_time_us");
    def.displayName = QStringLiteral("exposure_time_us");
    def.description = QStringLiteral("Integration time per frame, in microseconds.");
    def.category = ParameterCategory::Core;
    def.type = ParameterType::FloatRange;
    def.isReadOnly = false;
    def.constraint.minValue = 1.0;
    def.constraint.maxValue = 2147483647.0;
    def.constraint.step = 1.0;
    def.constraint.unit = QStringList{ QStringLiteral("us"), QStringLiteral("ms"),
                                       QStringLiteral("s") };
    def.constraint.unitRange = QVector<double>{ 1000.0, 1000000.0 };
    def.defaultValue = defaultValueUs;
    return def;
}

QWidget *TestParameterWidgets::createExposure(double defaultValueUs)
{
    return ParameterWidgetFactory::createWidget(exposureDefinition(defaultValueUs));
}

void TestParameterWidgets::typeAndCommit(QDoubleSpinBox *spinBox, const QString &text)
{
    spinBox->setFocus();
    spinBox->selectAll();
    QTest::keyClicks(spinBox, text);
    QTest::keyClick(spinBox, Qt::Key_Return);
}

void TestParameterWidgets::test_unit_scaled_parameter_uses_float_widget()
{
    QWidget *widget = createExposure(100.0);
    QVERIFY2(widget->findChild<QDoubleSpinBox *>(QStringLiteral("spinBox")) != nullptr,
             "a unit-scaled parameter must render a QDoubleSpinBox, not an integer one");
    QVERIFY2(widget->findChild<QSpinBox *>(QStringLiteral("spinBox")) == nullptr,
             "an integer spin box cannot represent sub-unit values");
    QVERIFY(widget->findChild<QComboBox *>(QStringLiteral("unitComboBox")) != nullptr);
}

void TestParameterWidgets::test_typed_whole_seconds_survive()
{
    QWidget *widget = createExposure(100.0);
    auto *spinBox = widget->findChild<QDoubleSpinBox *>(QStringLiteral("spinBox"));
    auto *unitCombo = widget->findChild<QComboBox *>(QStringLiteral("unitComboBox"));
    QVERIFY(spinBox && unitCombo);

    unitCombo->setCurrentIndex(2); // "s"
    typeAndCommit(spinBox, QStringLiteral("10"));

    QVERIFY2(qFuzzyCompare(spinBox->value(), 10.0),
             qPrintable(QStringLiteral("typed 10 s but the editor holds %1")
                            .arg(spinBox->value())));
    const QVariant raw = ParameterWidgetFactory::getWidgetValue(widget, ParameterType::FloatRange);
    QVERIFY2(qFuzzyCompare(raw.toDouble(), 10000000.0),
             qPrintable(QStringLiteral("raw value is %1 us, expected 10000000")
                            .arg(raw.toDouble())));
}

void TestParameterWidgets::test_typed_fractional_seconds_survive()
{
    QWidget *widget = createExposure(100.0);
    auto *spinBox = widget->findChild<QDoubleSpinBox *>(QStringLiteral("spinBox"));
    auto *unitCombo = widget->findChild<QComboBox *>(QStringLiteral("unitComboBox"));
    QVERIFY(spinBox && unitCombo);

    unitCombo->setCurrentIndex(2); // "s"
    typeAndCommit(spinBox, QStringLiteral("1.5"));

    QVERIFY2(qFuzzyCompare(spinBox->value(), 1.5),
             qPrintable(QStringLiteral("typed 1.5 s but the editor holds %1")
                            .arg(spinBox->value())));
    const QVariant raw = ParameterWidgetFactory::getWidgetValue(widget, ParameterType::FloatRange);
    QVERIFY2(qFuzzyCompare(raw.toDouble(), 1500000.0),
             qPrintable(QStringLiteral("raw value is %1 us, expected 1500000")
                            .arg(raw.toDouble())));
}

void TestParameterWidgets::test_unit_switch_keeps_raw_value()
{
    QWidget *widget = createExposure(2000000.0); // 2 s, so the editor opens in "s"
    auto *spinBox = widget->findChild<QDoubleSpinBox *>(QStringLiteral("spinBox"));
    auto *unitCombo = widget->findChild<QComboBox *>(QStringLiteral("unitComboBox"));
    QVERIFY(spinBox && unitCombo);

    QCOMPARE(unitCombo->currentIndex(), 2);
    QVERIFY(qFuzzyCompare(spinBox->value(), 2.0));

    unitCombo->setCurrentIndex(0); // back to "us"
    QVERIFY2(qFuzzyCompare(spinBox->value(), 2000000.0),
             qPrintable(QStringLiteral("switching to us lost the value: %1")
                            .arg(spinBox->value())));

    unitCombo->setCurrentIndex(2); // "s" again
    QVERIFY2(qFuzzyCompare(spinBox->value(), 2.0),
             qPrintable(QStringLiteral("switching back to s lost the value: %1")
                            .arg(spinBox->value())));
}

QTEST_MAIN(TestParameterWidgets)
#include "test_parameter_widgets.moc"
