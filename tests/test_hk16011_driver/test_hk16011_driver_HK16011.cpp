/**
 * @file test_hk16011_driver_HK16011.cpp
 * @brief Hardware test for the HK16011 driver plugin.
 *
 * Every test needs the camera attached (USB FX2 + the CH340 UART bridge), so
 * the whole suite skips cleanly when enumerate() comes back empty — that keeps
 * `ctest` green on machines without the device.
 *
 * Two device behaviours shape the capture tests and are worth re-reading if a
 * capture test starts failing:
 *
 *  - Frames can take a while to appear (a 1024x64 exposure is hundreds of
 *    milliseconds, and the device occasionally stalls for seconds), so the
 *    waits here are generous rather than tight.
 *
 * Parameters the firmware reports but does not actually measure are never
 * asserted for real values — see test_unimplemented_telemetry().
 */

#include <QTest>
#include <QSignalSpy>
#include <QSharedPointer>
#include <QVariantMap>
#include <QElapsedTimer>

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "plugins/hk16011/Hk16011Driver.h"

#include "hk16011.h"

/// Skips the calling test when no camera is attached. A macro rather than a
/// helper: QSKIP expands to `return`, which inside a helper would only return
/// from the helper and let the test go on to fail.
#define REQUIRE_HK16011() \
    do { \
        if (!m_cameraPresent) { \
            QSKIP("No HK16011 camera attached"); \
        } \
    } while (false)

/// ParameterConstraint::validValues is a QVector<QVariant>, which has no
/// join(); render it for assertion messages.
QString joinValues(const QVector<QVariant> &values)
{
    QStringList parts;
    parts.reserve(values.size());
    for (const QVariant &value : values) {
        parts.append(value.toString());
    }
    return parts.join(QStringLiteral(", "));
}

class TestHk16011Driver : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    // ——— No hardware required ———
    void test_driverVersion();
    void test_initialStateIsDisconnected();
    void test_enumerateRejectsBadIdWithoutHardware();

    // ——— Discovery / connection ———
    void test_enumerate();
    void test_connect();
    void test_connectTwiceReconnects();
    void test_connectInvalidId();
    void test_disconnect();
    void test_operationsBeforeConnect();

    // ——— Parameter table ———
    void test_parameterTableShape();
    void test_everyDefinitionIsValid();
    void test_readOnlyParametersAreFlagged();
    void test_parameterCategories();
    void test_exposureUnitSelector();

    // ——— One test per round-trippable parameter ———
    void test_parameter_exposure_time_us();
    void test_parameter_read_mode();
    void test_parameter_freq_sel();
    void test_parameter_mock_mode();
    void test_parameter_cdsclk_delay();
    void test_parameter_image_width();
    void test_parameter_image_height();
    void test_parameter_bevel_left();
    void test_parameter_blank_right();
    void test_parameter_tec_kp();
    void test_parameter_tec_set_temp();
    void test_parameter_adc_gain_r();
    void test_parameter_adc_offset_b();
    void test_parameter_mon_dwell_ms();

    // ——— Enumeration labels vs. wire tokens ———
    void test_enumValuesAreLabels();
    void test_enumAcceptsWireToken();
    void test_enumWriteReachesDevice();

    // ——— Read-only / telemetry ———
    void test_readOnly_camera_name();
    void test_readOnly_acq_state();
    void test_unimplemented_telemetry();

    // ——— Validation and batch semantics ———
    void test_setParameterRejectsOutOfRange();
    void test_setParameterRejectsUnknown();
    void test_setParameterAcceptsReadOnly();
    void test_setParametersBatch();
    void test_setParametersCriticalRejectsWholeBatch();
    void test_validateParameters();
    void test_commitParameters();

    // ——— Capture ———
    void test_captureSingleFrame();
    void test_captureBurst();
    void test_captureLiveAndStop();
    void test_stopCaptureWithoutStartIsNoop();
    void test_startCaptureWhileCapturingIsNoop();
    void test_startCaptureNotConnectedFails();

private:
    /// Connects and returns the camera id.
    QString connectAndGetId();
    /// Stages + commits @p values and returns whether the device accepted them.
    bool applyAndCommit(const QVariantMap &values);
    /// Round-trips one parameter and restores the original value.
    void roundTripParameter(const QString &name, const QVariant &value);

    static bool waitForFrames(QSignalSpy &spy, int expected, int timeoutMs);

    Hk16011Driver *m_driver = nullptr;
    bool m_cameraPresent = false;
};

namespace {

/// Frame waits are generous: a reset can delay the first frame by seconds.
const int kFrameTimeoutMs = 15000;
const int kBurstTimeoutMs = 20000;

} // namespace

//==============================================================================
// Fixture
//==============================================================================

void TestHk16011Driver::initTestCase()
{
    m_driver = new Hk16011Driver();

    const QStringList cameras = m_driver->enumerate();
    m_cameraPresent = !cameras.isEmpty();
    if (!m_cameraPresent) {
        qInfo("No HK16011 camera found — hardware tests will be skipped.");
    }
}

void TestHk16011Driver::cleanupTestCase()
{
    delete m_driver;
    m_driver = nullptr;
}

void TestHk16011Driver::init()
{
    if (!m_driver) {
        m_driver = new Hk16011Driver();
    }
}

void TestHk16011Driver::cleanup()
{
    if (!m_driver) {
        return;
    }
    // isConnected() alone would skip a driver stuck in Acquiring — a capture
    // whose frames never came — and leave m_capturing set, which poisons every
    // later test (startCapture() then short-circuits to "already running").
    // disconnectCamera() stops the capture on the way, so just always tear down.
    if (m_driver->state() != CameraState::Disconnected) {
        m_driver->disconnectCamera();
    }
}

QString TestHk16011Driver::connectAndGetId()
{
    const QStringList cameras = m_driver->enumerate();
    if (cameras.isEmpty()) {
        return QString();
    }
    const QString id = cameras.first();
    if (!m_driver->connectToCamera(id)) {
        return QString();
    }
    return id;
}

//==============================================================================
// No hardware required
//==============================================================================

void TestHk16011Driver::test_driverVersion()
{
    const QString version = m_driver->driverVersion();
    QVERIFY2(!version.isEmpty(), "driverVersion() should not be empty");
    const QStringList parts = version.split(QLatin1Char('.'));
    QVERIFY2(parts.size() >= 2, qPrintable(QString("expected major.minor, got '%1'").arg(version)));
    bool ok = false;
    parts.first().toInt(&ok);
    QVERIFY2(ok, qPrintable(QString("major version is not a number: '%1'").arg(version)));
}

void TestHk16011Driver::test_initialStateIsDisconnected()
{
    QCOMPARE(m_driver->state(), CameraState::Disconnected);
    QVERIFY2(!m_driver->isConnected(), "a fresh driver must not report connected");
    QVERIFY2(m_driver->cameraId().isEmpty(), "a fresh driver must not report a camera id");
}

void TestHk16011Driver::test_enumerateRejectsBadIdWithoutHardware()
{
    // Must fail cleanly whether or not hardware is present.
    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
    const bool ok = m_driver->connectToCamera("not-a-hk16011");
    QVERIFY2(!ok, "connecting to a bogus id must fail");
    QCOMPARE(m_driver->state(), CameraState::Disconnected);
    QVERIFY2(errorSpy.count() >= 1, "a rejected id should report an error");
}

//==============================================================================
// Discovery / connection
//==============================================================================

void TestHk16011Driver::test_enumerate()
{
    REQUIRE_HK16011();

    const QStringList cameras = m_driver->enumerate();
    QVERIFY2(!cameras.isEmpty(), "an attached camera should be enumerated");
    QVERIFY2(cameras.first().startsWith(QStringLiteral("hk16011:")),
             qPrintable(QString("unexpected camera id: %1").arg(cameras.first())));
}

void TestHk16011Driver::test_connect()
{
    REQUIRE_HK16011();

    QSignalSpy connectionSpy(m_driver, &ICameraDriver::connectionChanged);

    const QString id = connectAndGetId();
    QVERIFY2(!id.isEmpty(), "connectToCamera() should succeed with hardware attached");

    QVERIFY2(connectionSpy.count() == 1,
             qPrintable(QString("expected 1 connectionChanged, got %1").arg(connectionSpy.count())));
    const QVariantList args = connectionSpy.takeFirst();
    QVERIFY2(args.at(0).toBool(), "connectionChanged should report true on connect");
    QCOMPARE(args.at(1).toString(), id);

    QVERIFY2(m_driver->isConnected(), "isConnected() should be true after connect");
    QCOMPARE(m_driver->state(), CameraState::Connected);
    QCOMPARE(m_driver->cameraId(), id);
}

void TestHk16011Driver::test_connectTwiceReconnects()
{
    REQUIRE_HK16011();

    const QString id = connectAndGetId();
    QVERIFY2(!id.isEmpty(), "first connect should succeed");

    QSignalSpy connectionSpy(m_driver, &ICameraDriver::connectionChanged);
    const bool ok = m_driver->connectToCamera(id);
    QVERIFY2(ok, "reconnecting to the same id should succeed");
    QVERIFY2(connectionSpy.count() >= 1, "a reconnect should report a state change");
    QVERIFY2(m_driver->isConnected(), "driver should still be connected after reconnect");
}

void TestHk16011Driver::test_connectInvalidId()
{
    REQUIRE_HK16011();

    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
    const bool ok = m_driver->connectToCamera(QStringLiteral("hk16011:DEAD:BEEF"));
    QVERIFY2(!ok, "an unknown id must be rejected");
    QCOMPARE(m_driver->state(), CameraState::Disconnected);
    QVERIFY2(errorSpy.count() >= 1, "rejecting an id should report an error");
}

void TestHk16011Driver::test_disconnect()
{
    REQUIRE_HK16011();

    const QString id = connectAndGetId();
    QVERIFY2(!id.isEmpty(), "connect should succeed");

    QSignalSpy connectionSpy(m_driver, &ICameraDriver::connectionChanged);
    m_driver->disconnectCamera();

    QVERIFY2(connectionSpy.count() == 1,
             qPrintable(QString("expected 1 connectionChanged, got %1").arg(connectionSpy.count())));
    const QVariantList args = connectionSpy.takeFirst();
    QVERIFY2(!args.at(0).toBool(), "connectionChanged should report false on disconnect");
    QCOMPARE(args.at(1).toString(), id);

    QVERIFY2(!m_driver->isConnected(), "isConnected() should be false after disconnect");
    QCOMPARE(m_driver->state(), CameraState::Disconnected);
    QVERIFY2(m_driver->cameraId().isEmpty(), "cameraId() should be empty after disconnect");
}

void TestHk16011Driver::test_operationsBeforeConnect()
{
    QVERIFY2(m_driver->parameterNames().isEmpty(),
             "no parameters should be exposed before connecting");
    QVERIFY2(!m_driver->startCapture(1), "capture must not start before connecting");

    // Disconnecting a driver that was never connected is a no-op, not a crash.
    m_driver->disconnectCamera();
    m_driver->stopCapture();
}

//==============================================================================
// Parameter table
//==============================================================================

void TestHk16011Driver::test_parameterTableShape()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const QStringList names = m_driver->parameterNames();
    QVERIFY2(names.size() >= 30,
             qPrintable(QString("expected the full parameter table, got %1").arg(names.size())));

    // Parameters the vendor regression tests prove round-trip on the wire, plus
    // at least one from every category the driver groups them into.
    const QStringList expected = {
        QStringLiteral("exposure_time_us"), QStringLiteral("read_mode"),
        QStringLiteral("freq_sel"),        QStringLiteral("mock_mode"),
        QStringLiteral("cdsclk_delay"),    QStringLiteral("image_width"),
        QStringLiteral("image_height"),    QStringLiteral("bevel_left"),
        QStringLiteral("blank_right"),     QStringLiteral("tec_kp"),
        QStringLiteral("tec_set_temp"),    QStringLiteral("camera_name"),
        QStringLiteral("frame_num_ready"), QStringLiteral("mon_dwell_ms"),
        QStringLiteral("adc_gain_r"),      QStringLiteral("adc_gain_g"),
        QStringLiteral("adc_gain_b"),      QStringLiteral("adc_offset_r"),
        QStringLiteral("adc_offset_g"),    QStringLiteral("adc_offset_b"),
    };
    for (const QString &name : expected) {
        QVERIFY2(names.contains(name),
                 qPrintable(QString("parameter table is missing '%1'").arg(name)));
    }
}

void TestHk16011Driver::test_everyDefinitionIsValid()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const QStringList names = m_driver->parameterNames();
    QVERIFY2(!names.isEmpty(), "expected a non-empty parameter table");

    for (const QString &name : names) {
        const ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.name == name,
                 qPrintable(QString("definition for '%1' reports name '%2'").arg(name, def.name)));
        QVERIFY2(!def.displayName.isEmpty(),
                 qPrintable(QString("'%1' has no displayName").arg(name)));
        QVERIFY2(def.isValid(),
                 qPrintable(QString("definition for '%1' is not valid (type=%2 default=%3 "
                                    "range=[%4,%5])")
                                .arg(name)
                                .arg(static_cast<int>(def.type))
                                .arg(def.defaultValue.toString(),
                                     QString::number(def.constraint.minValue),
                                     QString::number(def.constraint.maxValue))));
    }

    // An unknown parameter yields a default-constructed definition.
    const ParameterDefinition unknown = m_driver->parameter(QStringLiteral("nope"));
    QVERIFY2(unknown.name.isEmpty(), "an unknown parameter should return an empty definition");
}

void TestHk16011Driver::test_readOnlyParametersAreFlagged()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    for (const QString &name : { QStringLiteral("camera_name"),
                                 QStringLiteral("acq_state"),
                                 QStringLiteral("frame_capacity"),
                                 QStringLiteral("sensor_temp") }) {
        const ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.isReadOnly,
                 qPrintable(QString("'%1' should be read-only").arg(name)));
    }

    for (const QString &name : { QStringLiteral("exposure_time_us"),
                                 QStringLiteral("image_width"),
                                 QStringLiteral("mock_mode") }) {
        const ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(!def.isReadOnly,
                 qPrintable(QString("'%1' should be writable").arg(name)));
    }

    // Read-only parameters that change on their own are the GUI's live
    // telemetry: CameraTab polls parameterValue() for exactly this set
    // (isReadOnly && isDynamic && isExtrinsic) while the config tab is open.
    for (const QString &name : { QStringLiteral("sensor_temp"),
                                 QStringLiteral("environment_temp"),
                                 QStringLiteral("tec_voltage"),
                                 QStringLiteral("tec_current"),
                                 QStringLiteral("acq_state"),
                                 QStringLiteral("frame_num_ready"),
                                 QStringLiteral("exception_flag"),
                                 QStringLiteral("exception_cnt") }) {
        const ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.isDynamic && def.isExtrinsic,
                 qPrintable(QString("'%1' should be dynamic + extrinsic").arg(name)));
    }

    // Constant read-only values are neither: polling them would only ever
    // return the same number.
    for (const QString &name : { QStringLiteral("camera_name"),
                                 QStringLiteral("frame_capacity") }) {
        const ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(!def.isDynamic && !def.isExtrinsic,
                 qPrintable(QString("'%1' should not be marked dynamic/extrinsic").arg(name)));
    }
}

void TestHk16011Driver::test_parameterCategories()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    // The SDK reports no grouping, so every category below comes from the
    // driver's own table. One representative per group is enough here —
    // test_parameterTableShape already pins down which parameters exist.
    const QVector<QPair<QString, ParameterCategory>> expected = {
        { QStringLiteral("exposure_time_us"), ParameterCategory::Core },
        { QStringLiteral("read_mode"),        ParameterCategory::Core },
        { QStringLiteral("freq_sel"),         ParameterCategory::Core },
        { QStringLiteral("tec_enable"),       ParameterCategory::Cooling },
        { QStringLiteral("tec_set_temp"),     ParameterCategory::Cooling },
        { QStringLiteral("mon_dwell_ms"),     ParameterCategory::Advanced },
        { QStringLiteral("camera_name"),      ParameterCategory::Info },
        { QStringLiteral("tec_kp"),           ParameterCategory::Advanced },
        { QStringLiteral("tec_ki"),           ParameterCategory::Advanced },
        { QStringLiteral("tec_kd"),           ParameterCategory::Advanced },
        { QStringLiteral("mock_mode"),        ParameterCategory::Debug },
        { QStringLiteral("image_width"),      ParameterCategory::Debug },
        { QStringLiteral("frame_num_ready"),  ParameterCategory::Debug },
    };
    for (const auto &entry : expected) {
        QCOMPARE(m_driver->parameter(entry.first).category, entry.second);
    }

    // The six ADC codes are whole-device settings a user actually dials in for
    // white balance, so they belong with exposure and readout mode.
    for (const QString &name : { QStringLiteral("adc_gain_r"),
                                 QStringLiteral("adc_gain_g"),
                                 QStringLiteral("adc_gain_b"),
                                 QStringLiteral("adc_offset_r"),
                                 QStringLiteral("adc_offset_g"),
                                 QStringLiteral("adc_offset_b") }) {
        QCOMPARE(m_driver->parameter(name).category, ParameterCategory::Core);
    }
}

void TestHk16011Driver::test_exposureUnitSelector()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    // A bare unit string is invisible in the GUI: ParameterWidgetFactory only
    // builds a unit combo when unitRange is present as well. Without the scale
    // the exposure spin box would span 1..2147483647 us, which no one can type.
    const ParameterConstraint &c = m_driver->parameter(QStringLiteral("exposure_time_us")).constraint;
    const QStringList expectedUnits = { QStringLiteral("us"), QStringLiteral("ms"), QStringLiteral("s") };
    QVERIFY2(c.unit == expectedUnits,
             qPrintable(QString("unit list is %1, expected us/ms/s").arg(c.unit.join(','))));
    const QVector<double> expectedScale = { 1000.0, 1000000.0 };
    QVERIFY2(c.unitRange == expectedScale,
             qPrintable(QString("unit scale has %1 entries, expected %2")
                            .arg(c.unitRange.size()).arg(expectedScale.size())));
    QVERIFY2(c.hasUnitRange(), "the exposure should render a unit selector");

    // The scale is cumulative from the base unit: getUnitIndex() compares the raw
    // value against these entries while toDisplayValue() divides by them, so
    // {1000, 1000} would have shown 2000 us as "2 s". Exercise the conversions the
    // widget itself performs rather than trusting the numbers above.
    QCOMPARE(c.getUnitIndex(2000.0), 1);
    QCOMPARE(c.getUnitIndex(500.0), 0);
    QCOMPARE(c.getUnitIndex(5000000.0), 2);
    QCOMPARE(c.toDisplayValue(2000.0, 1), 2.0);
    QCOMPARE(c.toRawValue(2.0, 1), 2000.0);
    QCOMPARE(c.toDisplayValue(5000000.0, 2), 5.0);

    // The raw value stays in microseconds, which is what the device stores and
    // what parameterValue() reports.
    QCOMPARE(c.toRawValue(2.0, 2), 2000000.0);
}

//==============================================================================
// Parameter round-trips
//==============================================================================

void TestHk16011Driver::roundTripParameter(const QString &name, const QVariant &value)
{
    const QVariant original = m_driver->parameterValue(name);
    QVERIFY2(original.isValid(),
             qPrintable(QString("'%1' has no current value").arg(name)));

    QStringList failed;
    QVERIFY2(m_driver->setParameter(name, value, &failed),
             qPrintable(QString("setParameter('%1', %2) was rejected: %3")
                            .arg(name, value.toString(), failed.join(QStringLiteral(", ")))));
    QVERIFY2(failed.isEmpty(),
             qPrintable(QString("setParameter('%1') reported failures: %2")
                            .arg(name, failed.join(QStringLiteral(", ")))));

    // Staged, not yet on the device.
    QCOMPARE(m_driver->parameterValue(name), value);

    QVERIFY2(m_driver->validateParameters(), "validateParameters() should accept the staged value");

    QStringList commitFailed;
    const bool committed = m_driver->commitParameters(&commitFailed);
    QVERIFY2(committed,
             qPrintable(QString("commitParameters() rejected '%1': %2")
                            .arg(name, commitFailed.join(QStringLiteral(", ")))));

    const QVariant actual = m_driver->parameterValue(name);
    QVERIFY2(actual == value,
             qPrintable(QString("'%1' round-tripped to %2, expected %3")
                            .arg(name, actual.toString(), value.toString())));

    // Put the device back the way we found it.
    m_driver->setParameter(name, original);
    m_driver->commitParameters();
    QCOMPARE(m_driver->parameterValue(name), original);
}

void TestHk16011Driver::test_parameter_exposure_time_us()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("exposure_time_us"), QVariant::fromValue(qlonglong(1234)));
}

void TestHk16011Driver::test_parameter_read_mode()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const ParameterDefinition def = m_driver->parameter(QStringLiteral("read_mode"));
    QCOMPARE(def.type, ParameterType::StringCollection);
    QVERIFY2(def.constraint.validValues.size() >= 2,
             "read_mode should offer both a line_binning and an image mode");

    // The label, not the wire token: validValues is what the combo box holds and
    // what the value is validated against.
    QVERIFY2(def.constraint.validValues.contains(QStringLiteral("Image")),
             "read_mode should offer the 'Image' label");
    roundTripParameter(QStringLiteral("read_mode"), QStringLiteral("Image"));
}

void TestHk16011Driver::test_parameter_freq_sel()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const ParameterDefinition def = m_driver->parameter(QStringLiteral("freq_sel"));
    QCOMPARE(def.type, ParameterType::StringCollection);
    QVERIFY2(def.constraint.validValues.contains(QStringLiteral("500k")),
             "freq_sel should offer the 500k token");

    roundTripParameter(QStringLiteral("freq_sel"), QStringLiteral("500k"));
}

void TestHk16011Driver::test_parameter_mock_mode()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const ParameterDefinition def = m_driver->parameter(QStringLiteral("mock_mode"));
    QCOMPARE(def.type, ParameterType::Boolean);

    const QVariant original = m_driver->parameterValue(QStringLiteral("mock_mode"));
    const bool target = !original.toBool();

    QVERIFY2(m_driver->setParameter(QStringLiteral("mock_mode"), target),
             "mock_mode should accept a bool");
    QVERIFY2(m_driver->commitParameters(), "mock_mode should commit");
    QCOMPARE(m_driver->parameterValue(QStringLiteral("mock_mode")), QVariant(target));

    m_driver->setParameter(QStringLiteral("mock_mode"), original);
    m_driver->commitParameters();
}

void TestHk16011Driver::test_parameter_cdsclk_delay()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("cdsclk_delay"), QVariant::fromValue(qlonglong(7)));
}

void TestHk16011Driver::test_parameter_image_width()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("image_width"), QVariant::fromValue(qlonglong(2048)));
}

void TestHk16011Driver::test_parameter_image_height()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("image_height"), QVariant::fromValue(qlonglong(32)));
}

void TestHk16011Driver::test_parameter_bevel_left()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("bevel_left"), QVariant::fromValue(qlonglong(5)));
}

void TestHk16011Driver::test_parameter_blank_right()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("blank_right"), QVariant::fromValue(qlonglong(3)));
}

void TestHk16011Driver::test_parameter_tec_kp()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const ParameterDefinition def = m_driver->parameter(QStringLiteral("tec_kp"));
    QCOMPARE(def.type, ParameterType::FloatRange);

    // The vendor regression test round-trips 0.5 and the device echoes "0.500",
    // so a float compare has to be tolerant of that formatting.
    const QVariant original = m_driver->parameterValue(QStringLiteral("tec_kp"));
    QVERIFY2(m_driver->setParameter(QStringLiteral("tec_kp"), 0.5), "tec_kp should accept 0.5");
    QVERIFY2(m_driver->commitParameters(), "tec_kp should commit");
    QVERIFY2(qAbs(m_driver->parameterValue(QStringLiteral("tec_kp")).toDouble() - 0.5) < 0.001,
             qPrintable(QString("tec_kp read back as %1")
                            .arg(m_driver->parameterValue(QStringLiteral("tec_kp")).toDouble())));

    m_driver->setParameter(QStringLiteral("tec_kp"), original);
    m_driver->commitParameters();
}

void TestHk16011Driver::test_parameter_tec_set_temp()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("tec_set_temp"), -10.2);
}

void TestHk16011Driver::test_parameter_adc_gain_r()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    // The firmware reworked the adc parameters into physical units: gain is
    // a float in [1, 6] V/V (it used to be an integer code 0-63).
    roundTripParameter(QStringLiteral("adc_gain_r"), QVariant(3.5));
}

void TestHk16011Driver::test_parameter_adc_offset_b()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    // Same rework: offsets are floats in [-300, 300] mV (used to be codes 0-511).
    roundTripParameter(QStringLiteral("adc_offset_b"), QVariant(-64.0));
}

void TestHk16011Driver::test_parameter_mon_dwell_ms()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("mon_dwell_ms"), QVariant::fromValue(qlonglong(250)));
}

void TestHk16011Driver::test_enumValuesAreLabels()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const ParameterDefinition readMode = m_driver->parameter(QStringLiteral("read_mode"));
    QCOMPARE(readMode.type, ParameterType::StringCollection);
    QVERIFY2(readMode.constraint.validValues.contains(QStringLiteral("Image")),
             qPrintable(QString("read_mode should offer the 'Image' label, got %1")
                            .arg(joinValues(readMode.constraint.validValues))));
    QVERIFY2(!readMode.constraint.validValues.contains(QStringLiteral("image")),
             "the wire token must not be what the GUI stores and shows");

    // The value read back from the device is normalized to the label. Without
    // that, ParameterWidgetFactory::setWidgetValue() cannot find it in the combo
    // and silently leaves the selection on the first entry.
    const QVariant live = m_driver->parameterValue(QStringLiteral("read_mode"));
    QVERIFY2(readMode.constraint.validValues.contains(live.toString()),
             qPrintable(QString("current read_mode '%1' is not one of the offered labels %2")
                            .arg(live.toString(),
                                 joinValues(readMode.constraint.validValues))));

    // freq_sel carries no labels at all, so it has to display as its tokens
    // rather than fall back to something blank.
    const ParameterDefinition freq = m_driver->parameter(QStringLiteral("freq_sel"));
    QVERIFY2(freq.constraint.validValues.contains(QStringLiteral("500k")),
             qPrintable(QString("freq_sel should offer '500k', got %1")
                            .arg(joinValues(freq.constraint.validValues))));
    QVERIFY2(freq.constraint.validValues.contains(
                 m_driver->parameterValue(QStringLiteral("freq_sel")).toString()),
             "the current readout clock should be one of the offered tokens");
}

void TestHk16011Driver::test_enumAcceptsWireToken()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    // Configs saved before the labels became the display form, and CLI scripts
    // written against the wire names, both carry tokens. They stay accepted, and
    // are normalized before staging — a token must never reach the device as-is.
    const QString name = QStringLiteral("read_mode");
    const QVariant original = m_driver->parameterValue(name);
    QVERIFY2(original.isValid(), "read_mode should have a current value");

    QStringList failed;
    QVERIFY2(m_driver->setParameter(name, QStringLiteral("image"), &failed),
             qPrintable(QString("the wire token should be accepted: %1")
                            .arg(failed.join(QStringLiteral(", ")))));
    QCOMPARE(m_driver->parameterValue(name), QVariant(QStringLiteral("Image")));
    QVERIFY2(m_driver->validateParameters(),
             "validateParameters() should accept the normalized value");

    QVERIFY2(m_driver->commitParameters(&failed),
             qPrintable(QString("commitParameters() rejected the token: %1")
                            .arg(failed.join(QStringLiteral(", ")))));
    QCOMPARE(m_driver->parameterValue(name), QVariant(QStringLiteral("Image")));

    // A token that is not one of this parameter's items is still rejected.
    QVERIFY2(!m_driver->setParameter(name, QStringLiteral("nonsense")),
             "an unknown enumeration value should be rejected");

    m_driver->setParameter(name, original);
    m_driver->commitParameters();
}

void TestHk16011Driver::test_enumWriteReachesDevice()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    // Regression test. A String/Enumeration value used to be pointed at a
    // QByteArray local to the encoder, so the SDK serialised freed memory and
    // sent whatever the heap happened to hold — read_mode arrived as the literal
    // "SETPARAM read_mode read_mode" and the firmware answered ERR 3. The other
    // enum tests did not catch it: the freed block often still contained the
    // right bytes, and asserting through the driver's own value cache cannot see
    // what the wire carried either way.
    //
    // So write the *other* mode and then re-read it from a fresh connection,
    // which is the only path that reflects what the device actually holds.
    const QString name = QStringLiteral("read_mode");
    const QString original = m_driver->parameterValue(name).toString();
    QVERIFY2(original.isEmpty() == false, "read_mode should have a current value");
    const QString target = (original == QStringLiteral("Image")) ? QStringLiteral("Line binning")
                                                                 : QStringLiteral("Image");

    QStringList failed;
    QVERIFY2(m_driver->setParameter(name, target, &failed),
             qPrintable(QString("setParameter('%1', %2) was rejected: %3")
                            .arg(name, target, failed.join(QStringLiteral(", ")))));
    QVERIFY2(m_driver->commitParameters(&failed),
             qPrintable(QString("commitParameters() rejected '%1': %2")
                            .arg(name, failed.join(QStringLiteral(", ")))));

    // Reconnecting re-reads every value from the device.
    m_driver->disconnectCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "reconnect should succeed");
    QCOMPARE(m_driver->parameterValue(name).toString(), target);

    // Put it back and confirm that too, rather than trusting the restore.
    QVERIFY2(m_driver->setParameter(name, original), "restoring read_mode should be accepted");
    QVERIFY2(m_driver->commitParameters(&failed), "restoring read_mode should commit");
    m_driver->disconnectCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "reconnect should succeed");
    QCOMPARE(m_driver->parameterValue(name).toString(), original);
}

//==============================================================================
// Read-only / telemetry
//==============================================================================

void TestHk16011Driver::test_readOnly_camera_name()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const QVariant name = m_driver->parameterValue(QStringLiteral("camera_name"));
    QVERIFY2(name.isValid(), "camera_name should have a value");
    QCOMPARE(name.toString(), QStringLiteral("hk16011"));

    // Writing is accepted but must not change anything.
    QVERIFY2(m_driver->setParameter(QStringLiteral("camera_name"), QStringLiteral("nope")),
             "a read-only parameter should be accepted and ignored");
    QVERIFY2(m_driver->commitParameters(), "committing a no-op should succeed");
    QCOMPARE(m_driver->parameterValue(QStringLiteral("camera_name")).toString(),
             QStringLiteral("hk16011"));
}

void TestHk16011Driver::test_readOnly_acq_state()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const QVariant state = m_driver->parameterValue(QStringLiteral("acq_state"));
    QVERIFY2(state.isValid(), "acq_state should have a value");
    // Labels, like every enumeration: the wire tokens are idle/exposing/reading.
    QVERIFY2(state.toString() == QStringLiteral("Idle")
                 || state.toString() == QStringLiteral("Exposing")
                 || state.toString() == QStringLiteral("Reading"),
             qPrintable(QString("unexpected acq_state '%1'").arg(state.toString())));

    // The firmware rejects SETPARAM on read-only parameters, and the driver
    // must not even put one on the wire.
    QVERIFY2(m_driver->setParameter(QStringLiteral("acq_state"), QStringLiteral("Reading")),
             "a read-only parameter should be accepted and ignored");
    m_driver->commitParameters();
    QCOMPARE(m_driver->parameterValue(QStringLiteral("acq_state")).toString(), state.toString());
}

void TestHk16011Driver::test_unimplemented_telemetry()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    // The TEC loop is not wired to real sensors, so the firmware returns
    // placeholders that fall outside the range the device itself declares.
    // These must be reported as read-only and must never be presented to the
    // user as measurements. They sit next to tec_set_temp in the Cooling group
    // rather than in Info: grouping follows what a parameter is for, and being
    // read-only no longer implies Info.
    const QStringList placeholders = { QStringLiteral("sensor_temp"),
                                       QStringLiteral("environment_temp"),
                                       QStringLiteral("tec_voltage"),
                                       QStringLiteral("tec_current") };

    for (const QString &name : placeholders) {
        const ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.isReadOnly,
                 qPrintable(QString("'%1' should be read-only").arg(name)));
        QCOMPARE(def.category, ParameterCategory::Cooling);
        QVERIFY2(def.description.contains(QStringLiteral("not implemented")),
                 qPrintable(QString("'%1' should be flagged as not implemented, "
                                    "description was: %2")
                                .arg(name, def.description)));

        const QVariant value = m_driver->parameterValue(name);
        QVERIFY2(value.isValid(), qPrintable(QString("'%1' should have a value").arg(name)));
        QVERIFY2(def.type == ParameterType::FloatRange,
                 qPrintable(QString("'%1' should be a float, got type %2")
                                .arg(name).arg(static_cast<int>(def.type))));

        // Deliberately no assertion about the value itself: it is a
        // placeholder and the firmware is free to change it.
    }
}

//==============================================================================
// Validation and batch semantics
//==============================================================================

void TestHk16011Driver::test_setParameterRejectsOutOfRange()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
    QStringList failed;

    // exposure_time_us is declared [1, 2147483647].
    const bool ok = m_driver->setParameter(QStringLiteral("exposure_time_us"),
                                           QVariant::fromValue(qlonglong(0)), &failed);

    QVERIFY2(!ok, "a value below the declared minimum must be rejected");
    QVERIFY2(failed.contains(QStringLiteral("exposure_time_us")),
             "the rejected parameter should be reported");
    QVERIFY2(errorSpy.count() == 1,
             qPrintable(QString("expected exactly 1 error, got %1").arg(errorSpy.count())));

    const CameraError err = errorSpy.takeFirst().at(0).value<CameraError>();
    QCOMPARE(err.severity, CameraError::Severity::Warning);
    QVERIFY2(err.failedParameters.contains(QStringLiteral("exposure_time_us")),
             "the error should name the rejected parameter");

    // cdsclk_delay is declared [0, 127].
    QVERIFY2(!m_driver->setParameter(QStringLiteral("cdsclk_delay"),
                                     QVariant::fromValue(qlonglong(128))),
             "a value above the declared maximum must be rejected");

    // An enum token that is not on the device's list.
    QVERIFY2(!m_driver->setParameter(QStringLiteral("freq_sel"), QStringLiteral("bogus")),
             "an unlisted enum token must be rejected");
}

void TestHk16011Driver::test_setParameterRejectsUnknown()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
    QStringList failed;
    const bool ok = m_driver->setParameter(QStringLiteral("no_such_param"),
                                           QVariant::fromValue(qlonglong(1)), &failed);

    QVERIFY2(!ok, "an unknown parameter must be rejected");
    QVERIFY2(failed.contains(QStringLiteral("no_such_param")),
             "the unknown parameter should be reported");
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<CameraError>().severity,
             CameraError::Severity::Warning);
}

void TestHk16011Driver::test_setParameterAcceptsReadOnly()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    // The GUI calls setParameter() on every control it renders, so a read-only
    // parameter must be accepted rather than throwing an error dialog.
    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
    QVERIFY2(m_driver->setParameter(QStringLiteral("camera_name"), QStringLiteral("ignored")),
             "a read-only parameter should be accepted");
    QCOMPARE(errorSpy.count(), 0);
}

void TestHk16011Driver::test_setParametersBatch()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

    QVariantMap batch;
    batch.insert(QStringLiteral("mock_mode"), true);
    batch.insert(QStringLiteral("exposure_time_us"), QVariant::fromValue(qlonglong(1500)));
    batch.insert(QStringLiteral("adc_gain_r"), QVariant::fromValue(qlonglong(999)));  // max 6.0
    batch.insert(QStringLiteral("nope"), QVariant::fromValue(qlonglong(1)));

    QStringList failed;
    const bool ok = m_driver->setParameters(batch, &failed);

    QVERIFY2(!ok, "a batch containing invalid entries must fail");
    QVERIFY2(failed.contains(QStringLiteral("adc_gain_r")), "adc_gain_r should be reported");
    QVERIFY2(failed.contains(QStringLiteral("nope")), "the unknown name should be reported");
    QVERIFY2(!failed.contains(QStringLiteral("mock_mode")),
             "a valid entry should not be reported as failed");
    QVERIFY2(!failed.contains(QStringLiteral("exposure_time_us")),
             "a valid entry should not be reported as failed");

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.takeFirst().at(0).value<CameraError>().severity,
             CameraError::Severity::Warning);

    // The valid entries are still staged, and the invalid ones are not.
    QCOMPARE(m_driver->parameterValue(QStringLiteral("mock_mode")), QVariant(true));
    QCOMPARE(m_driver->parameterValue(QStringLiteral("exposure_time_us")),
             QVariant::fromValue(qlonglong(1500)));
    QVERIFY2(!m_driver->parameter(QStringLiteral("nope")).isValid(),
             "an unknown parameter must not acquire a definition");

    QVERIFY2(m_driver->commitParameters(), "the staged valid entries should commit");
}

void TestHk16011Driver::test_setParametersCriticalRejectsWholeBatch()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

    const QVariant before = m_driver->parameterValue(QStringLiteral("mock_mode"));

    QVariantMap batch;
    batch.insert(QStringLiteral("mock_mode"), !before.toBool());
    batch.insert(QStringLiteral("image_height"), QVariant::fromValue(qlonglong(0)));  // min 1

    QStringList failed;
    const bool ok = m_driver->setParameters(batch, &failed);

    QVERIFY2(!ok, "a batch with a failing geometry parameter must be rejected");
    QVERIFY2(failed.contains(QStringLiteral("image_height")),
             "the failing geometry parameter should be reported");
    QVERIFY2(failed.contains(QStringLiteral("mock_mode")),
             "the whole batch should be reported as failed");
    QVERIFY2(errorSpy.count() == 1, "exactly one error should be emitted for a rejected batch");
    QCOMPARE(errorSpy.takeFirst().at(0).value<CameraError>().severity,
             CameraError::Severity::Warning);

    // Nothing from the batch may have been staged.
    QCOMPARE(m_driver->parameterValue(QStringLiteral("mock_mode")), before);
}

void TestHk16011Driver::test_validateParameters()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QVERIFY2(m_driver->validateParameters(), "an empty pending set should validate");

    const QVariant original = m_driver->parameterValue(QStringLiteral("adc_gain_r"));

    QVERIFY2(m_driver->setParameter(QStringLiteral("adc_gain_r"), QVariant(2.5)),
             "a valid value should stage");
    QVERIFY2(m_driver->validateParameters(), "a valid pending value should validate");
    QVERIFY2(m_driver->commitParameters(), "it should also commit");

    // validateParameters() only ever sees what setParameter() already
    // accepted, so a rejection has to come from setParameter() itself.
    QVERIFY2(!m_driver->setParameter(QStringLiteral("adc_gain_r"), QVariant(-1.0)),
             "an out-of-range value should never reach validateParameters()");

    // The commit above changed the device; put the original gain back.
    QVERIFY2(m_driver->setParameter(QStringLiteral("adc_gain_r"), original),
             "restoring adc_gain_r should be accepted");
    QVERIFY2(m_driver->commitParameters(), "restoring adc_gain_r should commit");
}

void TestHk16011Driver::test_commitParameters()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QVERIFY2(m_driver->commitParameters(), "committing nothing should succeed");

    QVariantMap batch;
    batch.insert(QStringLiteral("exposure_time_us"), QVariant::fromValue(qlonglong(2000)));
    batch.insert(QStringLiteral("adc_offset_b"), QVariant::fromValue(qlonglong(128)));
    QVERIFY2(applyAndCommit(batch), "a valid batch should apply and commit");

    QCOMPARE(m_driver->parameterValue(QStringLiteral("exposure_time_us")),
             QVariant::fromValue(qlonglong(2000)));
    QCOMPARE(m_driver->parameterValue(QStringLiteral("adc_offset_b")),
             QVariant::fromValue(qlonglong(128)));
}

bool TestHk16011Driver::applyAndCommit(const QVariantMap &values)
{
    QStringList failed;
    if (!m_driver->setParameters(values, &failed)) {
        qWarning() << "setParameters rejected:" << failed;
        return false;
    }
    QStringList commitFailed;
    if (!m_driver->commitParameters(&commitFailed)) {
        qWarning() << "commitParameters rejected:" << commitFailed;
        return false;
    }
    return true;
}

//==============================================================================
// Capture
//==============================================================================

bool TestHk16011Driver::waitForFrames(QSignalSpy &spy, int expected, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (spy.count() < expected && timer.elapsed() < timeoutMs) {
        QTest::qWait(20);
    }
    return spy.count() >= expected;
}

void TestHk16011Driver::test_captureSingleFrame()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy startedSpy(m_driver, &ICameraDriver::captureStarted);
    QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
    QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

    const QString cameraId = m_driver->cameraId();
    QVERIFY2(m_driver->startCapture(1), "startCapture(1) should succeed");

    QVERIFY2(startedSpy.count() > 0 || startedSpy.wait(1000),
             "captureStarted should be emitted");
    QCOMPARE(startedSpy.takeFirst().at(0).toString(), cameraId);

    QVERIFY2(waitForFrames(frameSpy, 1, kFrameTimeoutMs),
             "a single-frame capture should deliver a frame");

    // startCapture(1) must deliver exactly one frame and stop itself.
    QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(kFrameTimeoutMs),
             "captureStopped should be emitted after the frame count is reached");
    QCOMPARE(frameSpy.count(), 1);
    QCOMPARE(stoppedSpy.takeFirst().at(0).toString(), cameraId);
    QCOMPARE(m_driver->state(), CameraState::Connected);

    const QVariantList args = frameSpy.takeFirst();
    QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
    QVERIFY2(!image.isNull(), "the delivered image should not be null");
    QCOMPARE(image->format(), QImage::Format_Grayscale16);
    QVERIFY2(image->width() > 0 && image->height() > 0, "the image should have a real size");
    QVERIFY2(args.at(1).toULongLong() > 0, "the timestamp should be set");
    QCOMPARE(args.at(2).toInt(), 1);
    QCOMPARE(args.at(3).toString(), cameraId);
    QVERIFY2(!args.at(4).toMap().isEmpty(), "frameReady should carry the current parameters");
}

void TestHk16011Driver::test_captureBurst()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const int wanted = 5;
    QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
    QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

    QVERIFY2(m_driver->startCapture(wanted), "startCapture(5) should succeed");
    QVERIFY2(waitForFrames(frameSpy, wanted, kBurstTimeoutMs),
             qPrintable(QString("expected %1 frames, got %2").arg(wanted).arg(frameSpy.count())));

    QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(kFrameTimeoutMs),
             "captureStopped should be emitted once the burst is complete");

    // Frames arrive in batches, so this is the assertion that matters: the
    // driver must stop at exactly the requested count.
    QTest::qWait(500);
    QCOMPARE(frameSpy.count(), wanted);
    QCOMPARE(m_driver->state(), CameraState::Connected);
}

void TestHk16011Driver::test_captureLiveAndStop()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
    QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

    // count 0 = stream until stopped
    QVERIFY2(m_driver->startCapture(0), "a live capture should start");
    QCOMPARE(m_driver->state(), CameraState::Acquiring);

    QVERIFY2(waitForFrames(frameSpy, 3, kFrameTimeoutMs), "a live capture should keep delivering");
    const int atStop = frameSpy.count();

    m_driver->stopCapture();

    QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(2000),
             "stopCapture() should emit captureStopped");
    QCOMPARE(m_driver->state(), CameraState::Connected);

    // No further frames once stopped.
    const int afterStop = frameSpy.count();
    QTest::qWait(500);
    QCOMPARE(frameSpy.count(), afterStop);
    QVERIFY2(atStop >= 3, "at least 3 frames should have arrived before the stop");
}

void TestHk16011Driver::test_stopCaptureWithoutStartIsNoop()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);
    m_driver->stopCapture();
    QCOMPARE(stoppedSpy.count(), 0);
    QCOMPARE(m_driver->state(), CameraState::Connected);
}

void TestHk16011Driver::test_startCaptureWhileCapturingIsNoop()
{
    REQUIRE_HK16011();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
    QVERIFY2(m_driver->startCapture(0), "live capture should start");

    QVERIFY2(waitForFrames(frameSpy, 1, kFrameTimeoutMs), "the live capture should deliver");

    QSignalSpy startedSpy(m_driver, &ICameraDriver::captureStarted);
    QVERIFY2(m_driver->startCapture(3),
             "starting an already-running capture should be a no-op success");
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(m_driver->state(), CameraState::Acquiring);

    m_driver->stopCapture();
    QCOMPARE(m_driver->state(), CameraState::Connected);
}

void TestHk16011Driver::test_startCaptureNotConnectedFails()
{
    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
    QVERIFY2(!m_driver->startCapture(1), "capture must not start while disconnected");
    QVERIFY2(errorSpy.count() >= 1, "starting while disconnected should report an error");
    QCOMPARE(errorSpy.takeFirst().at(0).value<CameraError>().code,
             CameraError::Code::NotConnected);
}

QTEST_MAIN(TestHk16011Driver)
#include "test_hk16011_driver_HK16011.moc"
