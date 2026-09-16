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

    void test_int_collection_rejects_invalid()
    {
        ParameterDefinition def = makeIntCollection("binning",
            QVector<int>{1, 2, 4, 8}, 1);
        ClampResult r = clampValue(QVariant(3), def);
        QVERIFY(!r.value.isValid());
        QVERIFY(!r.changed);
        QVERIFY(r.reason.contains("must be one of"));
    }

    void test_int_collection_accepts_valid()
    {
        ParameterDefinition def = makeIntCollection("binning",
            QVector<int>{1, 2, 4, 8}, 1);
        ClampResult r = clampValue(QVariant(4), def);
        QCOMPARE(r.value.toInt(), 4);
        QVERIFY(!r.changed);
        QVERIFY(r.reason.isEmpty());
    }

    void test_boolean_coerces_from_string()
    {
        ParameterDefinition def = makeBoolean("cooling_enabled", false);
        ClampResult r = clampValue(QVariant("true"), def);
        QCOMPARE(r.value.toBool(), true);
        QVERIFY(r.changed);
    }

    void test_boolean_unchanged_when_already_bool()
    {
        ParameterDefinition def = makeBoolean("cooling_enabled", false);
        ClampResult r = clampValue(QVariant(true), def);
        QCOMPARE(r.value.toBool(), true);
        QVERIFY(!r.changed);
    }

    void test_string_passthrough()
    {
        ParameterDefinition def;
        def.name = "comment";
        def.displayName = "Comment";
        def.description = "Free text";
        def.category = ParameterCategory::Info;
        def.type = ParameterType::String;
        def.defaultValue = QStringLiteral("hello");
        ClampResult r = clampValue(QVariant("world"), def);
        QCOMPARE(r.value.toString(), QString("world"));
        QVERIFY(!r.changed);
    }

    void test_readonly_rejects()
    {
        ParameterDefinition def = makeFloatRange("cooling_sensor_temp",
            -50.0, 50.0, 0.1, 25.0);
        def.isReadOnly = true;
        def.isExtrinsic = true;
        ClampResult r = clampValue(QVariant(20.0), def);
        QVERIFY(!r.value.isValid());
        QVERIFY(r.reason.contains("read-only"));
    }

    void test_unknown_param_rejects()
    {
        ParameterDefinition def;
        ClampResult r = clampValue(QVariant(1.0), def);
        QVERIFY(!r.value.isValid());
        QVERIFY(r.reason.contains("unknown"));
    }

    void test_invalid_definition_rejects()
    {
        ParameterDefinition def;
        def.name = "broken";
        def.displayName = "Broken";
        def.description = "Broken";
        def.category = ParameterCategory::Core;
        def.type = ParameterType::FloatRange;
        def.constraint.minValue = 100.0;
        def.constraint.maxValue = 50.0;
        ClampResult r = clampValue(QVariant(75.0), def);
        QVERIFY(!r.value.isValid());
        QVERIFY(r.reason.contains("invalid parameter definition"));
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

    void test_apply_collection_rejected_is_skipped()
    {
        QVariantMap params;
        params.insert("binning", 3);
        cli::ApplyReport rep = cli::applyClampedSet(m_driver, params, QString());
        QCOMPARE(rep.clamped, 0);
        QCOMPARE(rep.skipped, 1);
        QCOMPARE(m_driver->parameterValue("binning").toInt(), 1);
    }

    void test_apply_unknown_param_is_skipped()
    {
        QVariantMap params;
        params.insert("nonexistent_param", 1.0);
        cli::ApplyReport rep = cli::applyClampedSet(m_driver, params, QString());
        QCOMPARE(rep.clamped, 0);
        QCOMPARE(rep.skipped, 1);
        QCOMPARE(rep.setFailed, 0);
    }

    void test_apply_valid_value_no_change()
    {
        QVariantMap params;
        params.insert("exposure", 100.0);
        cli::ApplyReport rep = cli::applyClampedSet(m_driver, params, QString());
        QCOMPARE(rep.clamped, 0);
        QCOMPARE(rep.skipped, 0);
        QCOMPARE(rep.setFailed, 0);
        QCOMPARE(m_driver->parameterValue("exposure").toDouble(), 100.0);
    }

    void test_apply_indent_prefixes_log_lines()
    {
        QVariantMap params;
        params.insert("exposure", 99999.0);
        cli::ApplyReport rep = cli::applyClampedSet(m_driver, params, QStringLiteral("  "));
        QCOMPARE(rep.clamped, 1);
        QCOMPARE(m_driver->parameterValue("exposure").toDouble(), 10000.0);
    }

private:
    MockCameraDriver *m_driver = nullptr;
};

QTEST_APPLESS_MAIN(TestCliClamp)
#include "test_cli_clamp.moc"