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
    /// Skips the calling test when no camera is attached.
    void requireCamera();
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
    if (m_driver && m_driver->isConnected()) {
        m_driver->disconnectCamera();
    }
}

void TestHk16011Driver::requireCamera()
{
    if (!m_cameraPresent) {
        QSKIP("No HK16011 camera attached");
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
    requireCamera();

    const QStringList cameras = m_driver->enumerate();
    QVERIFY2(!cameras.isEmpty(), "an attached camera should be enumerated");
    QVERIFY2(cameras.first().startsWith(QStringLiteral("hk16011:")),
             qPrintable(QString("unexpected camera id: %1").arg(cameras.first())));
}

void TestHk16011Driver::test_connect()
{
    requireCamera();

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
    requireCamera();

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
    requireCamera();

    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
    const bool ok = m_driver->connectToCamera(QStringLiteral("hk16011:DEAD:BEEF"));
    QVERIFY2(!ok, "an unknown id must be rejected");
    QCOMPARE(m_driver->state(), CameraState::Disconnected);
    QVERIFY2(errorSpy.count() >= 1, "rejecting an id should report an error");
}

void TestHk16011Driver::test_disconnect()
{
    requireCamera();

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
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const QStringList names = m_driver->parameterNames();
    QVERIFY2(names.size() >= 30,
             qPrintable(QString("expected the full parameter table, got %1").arg(names.size())));

    // Parameters the vendor regression tests prove round-trip on the wire.
    const QStringList expected = {
        QStringLiteral("exposure_time_us"), QStringLiteral("read_mode"),
        QStringLiteral("freq_sel"),        QStringLiteral("mock_mode"),
        QStringLiteral("cdsclk_delay"),    QStringLiteral("image_width"),
        QStringLiteral("image_height"),    QStringLiteral("bevel_left"),
        QStringLiteral("blank_right"),     QStringLiteral("tec_kp"),
        QStringLiteral("tec_set_temp"),    QStringLiteral("adc_gain_r"),
        QStringLiteral("adc_offset_b"),    QStringLiteral("camera_name"),
    };
    for (const QString &name : expected) {
        QVERIFY2(names.contains(name),
                 qPrintable(QString("parameter table is missing '%1'").arg(name)));
    }
}

void TestHk16011Driver::test_everyDefinitionIsValid()
{
    requireCamera();
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
    requireCamera();
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
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("exposure_time_us"), QVariant::fromValue(qlonglong(1234)));
}

void TestHk16011Driver::test_parameter_read_mode()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const ParameterDefinition def = m_driver->parameter(QStringLiteral("read_mode"));
    QCOMPARE(def.type, ParameterType::StringCollection);
    QVERIFY2(def.constraint.validValues.size() >= 2,
             "read_mode should offer both a line_binning and an image mode");

    roundTripParameter(QStringLiteral("read_mode"), QStringLiteral("image"));
}

void TestHk16011Driver::test_parameter_freq_sel()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const ParameterDefinition def = m_driver->parameter(QStringLiteral("freq_sel"));
    QCOMPARE(def.type, ParameterType::StringCollection);
    QVERIFY2(def.constraint.validValues.contains(QStringLiteral("500k")),
             "freq_sel should offer the 500k token");

    roundTripParameter(QStringLiteral("freq_sel"), QStringLiteral("500k"));
}

void TestHk16011Driver::test_parameter_mock_mode()
{
    requireCamera();
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
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("cdsclk_delay"), QVariant::fromValue(qlonglong(7)));
}

void TestHk16011Driver::test_parameter_image_width()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("image_width"), QVariant::fromValue(qlonglong(2048)));
}

void TestHk16011Driver::test_parameter_image_height()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("image_height"), QVariant::fromValue(qlonglong(32)));
}

void TestHk16011Driver::test_parameter_bevel_left()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("bevel_left"), QVariant::fromValue(qlonglong(5)));
}

void TestHk16011Driver::test_parameter_blank_right()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("blank_right"), QVariant::fromValue(qlonglong(3)));
}

void TestHk16011Driver::test_parameter_tec_kp()
{
    requireCamera();
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
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("tec_set_temp"), -10.2);
}

void TestHk16011Driver::test_parameter_adc_gain_r()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("adc_gain_r"), QVariant::fromValue(qlonglong(63)));
}

void TestHk16011Driver::test_parameter_adc_offset_b()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");
    roundTripParameter(QStringLiteral("adc_offset_b"), QVariant::fromValue(qlonglong(511)));
}

//==============================================================================
// Read-only / telemetry
//==============================================================================

void TestHk16011Driver::test_readOnly_camera_name()
{
    requireCamera();
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
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    const QVariant state = m_driver->parameterValue(QStringLiteral("acq_state"));
    QVERIFY2(state.isValid(), "acq_state should have a value");
    QVERIFY2(state.toString() == QStringLiteral("idle")
                 || state.toString() == QStringLiteral("exposing")
                 || state.toString() == QStringLiteral("reading"),
             qPrintable(QString("unexpected acq_state '%1'").arg(state.toString())));

    // The firmware rejects SETPARAM on read-only parameters, and the driver
    // must not even put one on the wire.
    QVERIFY2(m_driver->setParameter(QStringLiteral("acq_state"), QStringLiteral("reading")),
             "a read-only parameter should be accepted and ignored");
    m_driver->commitParameters();
    QCOMPARE(m_driver->parameterValue(QStringLiteral("acq_state")).toString(), state.toString());
}

void TestHk16011Driver::test_unimplemented_telemetry()
{
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    // The TEC loop is not wired to real sensors, so the firmware returns
    // placeholders that fall outside the range the device itself declares.
    // These must be reported as read-only info and must never be presented to
    // the user as measurements.
    const QStringList placeholders = { QStringLiteral("sensor_temp"),
                                       QStringLiteral("environment_temp"),
                                       QStringLiteral("tec_voltage"),
                                       QStringLiteral("tec_current") };

    for (const QString &name : placeholders) {
        const ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.isReadOnly,
                 qPrintable(QString("'%1' should be read-only").arg(name)));
        QCOMPARE(def.category, ParameterCategory::Info);
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
    requireCamera();
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
    requireCamera();
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
    requireCamera();
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
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

    QVariantMap batch;
    batch.insert(QStringLiteral("mock_mode"), true);
    batch.insert(QStringLiteral("exposure_time_us"), QVariant::fromValue(qlonglong(1500)));
    batch.insert(QStringLiteral("adc_gain_r"), QVariant::fromValue(qlonglong(999)));  // max 63
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
    requireCamera();
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
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QVERIFY2(m_driver->validateParameters(), "an empty pending set should validate");

    QVERIFY2(m_driver->setParameter(QStringLiteral("adc_gain_r"), QVariant::fromValue(qlonglong(10))),
             "a valid value should stage");
    QVERIFY2(m_driver->validateParameters(), "a valid pending value should validate");
    QVERIFY2(m_driver->commitParameters(), "it should also commit");

    // validateParameters() only ever sees what setParameter() already
    // accepted, so a rejection has to come from setParameter() itself.
    QVERIFY2(!m_driver->setParameter(QStringLiteral("adc_gain_r"), QVariant::fromValue(qlonglong(-1))),
             "an out-of-range value should never reach validateParameters()");
}

void TestHk16011Driver::test_commitParameters()
{
    requireCamera();
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
    requireCamera();
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
    requireCamera();
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
    requireCamera();
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
    requireCamera();
    QVERIFY2(!connectAndGetId().isEmpty(), "connect should succeed");

    QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);
    m_driver->stopCapture();
    QCOMPARE(stoppedSpy.count(), 0);
    QCOMPARE(m_driver->state(), CameraState::Connected);
}

void TestHk16011Driver::test_startCaptureWhileCapturingIsNoop()
{
    requireCamera();
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
