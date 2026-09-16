#include <QTest>
#include <QDebug>

#include "core/CameraTypes.h"
#include "core/ICameraDriver.h"
#include "cli/ParameterClamper.h"
#include "plugins/mock/MockCameraDriver.h"

namespace {

ParameterDefinition makeFloatRange(const QString &name,
                                   double min, double max, double step,
                                   double defaultValue)
{
    ParameterDefinition def;
    def.name = name;
    def.displayName = name;
    def.description = name;
    def.category = ParameterCategory::Core;
    def.type = ParameterType::FloatRange;
    def.constraint.minValue = min;
    def.constraint.maxValue = max;
    def.constraint.step = step;
    def.defaultValue = defaultValue;
    return def;
}

ParameterDefinition makeIntCollection(const QString &name,
                                      QVector<int> valid,
                                      int defaultValue)
{
    ParameterDefinition def;
    def.name = name;
    def.displayName = name;
    def.description = name;
    def.category = ParameterCategory::Core;
    def.type = ParameterType::IntCollection;
    for (int v : valid) def.constraint.validValues.append(v);
    def.defaultValue = defaultValue;
    return def;
}

ParameterDefinition makeBoolean(const QString &name, bool defaultValue)
{
    ParameterDefinition def;
    def.name = name;
    def.displayName = name;
    def.description = name;
    def.category = ParameterCategory::Core;
    def.type = ParameterType::Boolean;
    def.defaultValue = defaultValue;
    return def;
}

} // namespace

class TestCliClamp : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_driver = new MockCameraDriver();
        QVERIFY(m_driver->connectToCamera("mock-001"));
    }

    void cleanup()
    {
        if (m_driver) {
            if (m_driver->isConnected()) m_driver->disconnectCamera();
            delete m_driver;
            m_driver = nullptr;
        }
    }

    // ---- clampValue unit tests (no driver required) ----

    void test_float_range_clamp_above_max()
    {
        ParameterDefinition def = makeFloatRange("exposure", 1.0, 10000.0, 1.0, 100.0);
        ClampResult r = clampValue(QVariant(99999.0), def);
        QCOMPARE(r.value.toDouble(), 10000.0);
        QVERIFY(r.changed);
        QVERIFY(r.reason.contains("clamped"));
    }

    void test_float_range_clamp_below_min()
    {
        ParameterDefinition def = makeFloatRange("exposure", 1.0, 10000.0, 1.0, 100.0);
        ClampResult r = clampValue(QVariant(0.5), def);
        QCOMPARE(r.value.toDouble(), 1.0);
        QVERIFY(r.changed);
    }

    void test_float_range_step_align()
    {
        ParameterDefinition def = makeFloatRange("gain", 0.0, 40.0, 0.1, 1.0);
        ClampResult r = clampValue(QVariant(5.73), def);
        // correctStep(5.73, 0.0, 0.1) = 0 + qRound((5.73-0)/0.1)*0.1 = 57*0.1 = 5.7
        QCOMPARE(r.value.toDouble(), 5.7);
        QVERIFY(r.changed);
    }

    void test_int_range_valid_unchanged()
    {
        ParameterDefinition def = makeFloatRange("exposure", 1.0, 10000.0, 1.0, 100.0);
        ClampResult r = clampValue(QVariant(500.0), def);
        QCOMPARE(r.value.toDouble(), 500.0);
        QVERIFY(!r.changed);
        QVERIFY(r.reason.isEmpty());
    }

    // ---- applyClampedSet integration tests (need driver) ----

    void test_apply_exposure_above_max_is_clamped()
    {
        QVariantMap params;
        params.insert("exposure", 99999.0);
        cli::ApplyReport rep = cli::applyClampedSet(m_driver, params, QString());
        QCOMPARE(rep.clamped, 1);
        QCOMPARE(rep.skipped, 0);
        QCOMPARE(rep.setFailed, 0);
        QCOMPARE(m_driver->parameterValue("exposure").toDouble(), 10000.0);
    }

private:
    MockCameraDriver *m_driver = nullptr;
};

QTEST_APPLESS_MAIN(TestCliClamp)
#include "test_cli_clamp.moc"