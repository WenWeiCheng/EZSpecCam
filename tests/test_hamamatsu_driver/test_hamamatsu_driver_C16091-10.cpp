#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QSharedPointer>
#include <qtestcase.h>

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "plugins/hamamatsu/HamamatsuDriver.h"

static const QStringList s_excludedProps = {
    "SUBARRAY MODE", "SUBARRAY HPOS", "SUBARRAY HSIZE",
    "SUBARRAY VPOS", "SUBARRAY VSIZE",
    "TIMING READOUT TIME", "TIMING CYCLIC TRIGGER PERIOD",
    "TIMING MIN TRIGGER BLANKING", "TIMING MIN TRIGGER INTERVAL",
    "RECORD FIXED BYTES PER FILE",
    "RECORD FIXED BYTES PER SESSION",
    "RECORD FIXED BYTES PER FRAME"
};

class TestHamamatsuDriver : public QObject
{
    Q_OBJECT

private:
    HamamatsuDriver *m_driver = nullptr;
    QString m_cameraId;
    QStringList m_params;

    bool tryConnect()
    {
        QStringList cameras = m_driver->enumerate();
        if (cameras.isEmpty()) {
            return false;
        }
        m_cameraId = cameras.first();
        if (!m_driver->connectToCamera(m_cameraId)) {
            return false;
        }
        m_params = m_driver->parameterNames();
        return true;
    }

private slots:
    void initTestCase()
    {
        m_driver = new HamamatsuDriver();
        QVERIFY2(m_driver != nullptr, "HamamatsuDriver should be created");
    }

    void cleanupTestCase()
    {
        if (m_driver) {
            if (m_driver->isConnected()) {
                m_driver->disconnectCamera();
            }
            delete m_driver;
            m_driver = nullptr;
        }
    }

    void init()
    {
        if (m_driver && m_driver->isConnected()) {
            m_driver->disconnectCamera();
        }
    }

    void cleanup()
    {
        if (m_driver && m_driver->isConnected()) {
            m_driver->disconnectCamera();
        }
    }

    //==========================================================================
    // Enumeration Tests
    //==========================================================================

    void test_enumerate()
    {
        QStringList cameras = m_driver->enumerate();
        qDebug() << "Found" << cameras.size() << "camera(s):" << cameras;

        for (const QString &id : cameras) {
            QVERIFY2(!id.isEmpty(),
                     qPrintable(QString("Camera ID should not be empty")));
            // ID format: "MODEL (SERIAL) on BUS" or "MODEL (SERIAL)"
            QVERIFY2(id.contains('('),
                     qPrintable(QString("Camera ID should contain serial number in parentheses: %1").arg(id)));
        }
    }

    //==========================================================================
    // Connection Tests
    //==========================================================================

    void test_connect()
    {
        if (!tryConnect()) {
            QSKIP("No Hamamatsu camera found, skipping test");
        }

        QVERIFY2(m_driver->isConnected(),
                 qPrintable(QString("isConnected should be true after connect: %1").arg(m_cameraId)));
        QVERIFY2(m_driver->state() == CameraState::Connected,
                 "State should be Connected after connection");
        QVERIFY2(!m_driver->cameraId().isEmpty(),
                 "cameraId should not be empty after connection");
    }

    void test_connect_invalid()
    {
        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        bool result = m_driver->connectToCamera("nonexistent-camera-xyz");
        QVERIFY2(result == false,
                 "Should fail to connect to nonexistent camera");
        QVERIFY2(m_driver->state() == CameraState::Disconnected,
                 "State should be Disconnected after failed connection");

        QVERIFY2(errorSpy.count() > 0 || errorSpy.wait(500),
                 "errorOccurred should be emitted on failed connection");
    }

    void test_disconnect()
    {
        if (!tryConnect()) {
            QSKIP("No Hamamatsu camera found, skipping test");
        }

        QSignalSpy connectionSpy(m_driver, &ICameraDriver::connectionChanged);

        m_driver->disconnectCamera();
        QVERIFY2(!m_driver->isConnected(),
                 "isConnected should be false after disconnect");
        QVERIFY2(m_driver->state() == CameraState::Disconnected,
                 "State should be Disconnected after disconnect");

        QVERIFY2(connectionSpy.count() > 0,
                 "connectionChanged should be emitted on disconnect");
        if (connectionSpy.count() > 0) {
            QVERIFY2(!connectionSpy.first().at(0).toBool(),
                     "connectionChanged signal should carry false on disconnect");
        }
    }

    void test_double_connect()
    {
        if (!tryConnect()) {
            QSKIP("No Hamamatsu camera found, skipping test");
        }

        QString firstCamera = m_cameraId;

        // Connect to same camera again
        bool result = m_driver->connectToCamera(firstCamera);
        QVERIFY2(result == true,
                 "Re-connect to same camera should succeed");
        QVERIFY2(m_driver->isConnected(),
                 "Should be connected after re-connect");
    }

    void test_driver_version()
    {
        QString version = m_driver->driverVersion();
        QVERIFY2(!version.isEmpty(),
                 "Driver version should not be empty");
        qDebug() << "Driver version:" << version;
    }

    void test_connection_signal()
    {
        if (!tryConnect()) {
            QSKIP("No Hamamatsu camera found, skipping test");
        }

        // Disconnect first
        m_driver->disconnectCamera();

        QSignalSpy spy(m_driver, &ICameraDriver::connectionChanged);

        bool ok = m_driver->connectToCamera(m_cameraId);
        QVERIFY2(ok, "Should connect successfully");

        QVERIFY2(spy.count() > 0 || spy.wait(1000),
                 "connectionChanged signal should be emitted on connect");
        if (spy.count() > 0) {
            QVariantList args = spy.takeFirst();
            QVERIFY2(args.at(0).toBool() == true,
                     "connectionChanged should carry true on connect");
            QVERIFY2(!args.at(1).toString().isEmpty(),
                     "cameraId should not be empty in connectionChanged");
        }
    }

    //==========================================================================
    // Parameter Definition Tests
    //==========================================================================

    void test_parameter_names_not_empty()
    {
        if (!tryConnect()) {
            QSKIP("No Hamamatsu camera found, skipping test");
        }

        QVERIFY2(!m_params.isEmpty(),
                 qPrintable(QString("parameterNames should not be empty, got %1 params").arg(m_params.size())));
        qDebug() << "Parameter count:" << m_params.size();
    }

    void test_excluded_params_not_present()
    {
        if (!tryConnect()) {
            QSKIP("No Hamamatsu camera found, skipping test");
        }

        // Verify excluded properties (by DCAM name pattern) are NOT in our parameter list
        // The exclusion list maps to near-DCAM names; verify our param names don't match
        for (const QString &param : m_params) {
            QVERIFY2(!param.startsWith("subarray", Qt::CaseInsensitive),
                     qPrintable(QString("SUBARRAY params should be excluded but found: %1").arg(param)));
            QVERIFY2(!param.startsWith("timing_", Qt::CaseInsensitive),
                     qPrintable(QString("TIMING params should be excluded but found: %1").arg(param)));
            QVERIFY2(!param.startsWith("record_", Qt::CaseInsensitive),
                     qPrintable(QString("RECORD params should be excluded but found: %1").arg(param)));
        }
    }

    // Helper: verify a parameter exists and has valid definition
    void verifyParamExists(const QString &name)
    {
        QVERIFY2(m_params.contains(name),
                 qPrintable(QString("Parameter '%1' should exist in parameterNames").arg(name)));
        ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.isValid(),
                 qPrintable(QString("Parameter definition for '%1' should be valid").arg(name)));
    }

    // Helper: verify read-only + type
    void verifyParamReadOnly(const QString &name, ParameterType expectedType)
    {
        verifyParamExists(name);
        ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.isReadOnly,
                 qPrintable(QString("Parameter '%1' should be read-only").arg(name)));
        QVERIFY2(def.type == expectedType,
                 qPrintable(QString("Parameter '%1' type should be %2, got %3")
                     .arg(name).arg(static_cast<int>(expectedType)).arg(static_cast<int>(def.type))));
    }

    // Helper: verify read-write + type
    void verifyParamReadWrite(const QString &name, ParameterType expectedType)
    {
        verifyParamExists(name);
        ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(!def.isReadOnly,
                 qPrintable(QString("Parameter '%1' should be writable").arg(name)));
        QVERIFY2(def.type == expectedType,
                 qPrintable(QString("Parameter '%1' type should be %2, got %3")
                     .arg(name).arg(static_cast<int>(expectedType)).arg(static_cast<int>(def.type))));
    }

    // Helper: test setting range parameter at boundary values
    void testRangeParam(const QString &name)
    {
        verifyParamReadWrite(name, ParameterType::FloatRange);
        ParameterDefinition def = m_driver->parameter(name);

        qDebug() << "parameter " << name << ": min(" << def.constraint.minValue << ") max(" << def.constraint.maxValue << ") " << "step(" << def.constraint.step << ")";

        // Set min
        bool ok = m_driver->setParameter(name, def.constraint.minValue);
        QVERIFY2(ok,
                 qPrintable(QString("Should set '%1' to min value %2")
                     .arg(name).arg(def.constraint.minValue)));

        // Set max
        ok = m_driver->setParameter(name, def.constraint.maxValue);
        QVERIFY2(ok,
                 qPrintable(QString("Should set '%1' to max value %2")
                     .arg(name).arg(def.constraint.maxValue)));

        // Set midpoint
        double mid = (def.constraint.minValue + def.constraint.maxValue) / 2.0;
        ok = m_driver->setParameter(name, mid);
        QVERIFY2(ok,
                 qPrintable(QString("Should set '%1' to midpoint %2")
                     .arg(name).arg(mid)));
    }

    // Helper: test setting integer range parameter at boundary values
    void testIntRangeParam(const QString &name)
    {
        verifyParamReadWrite(name, ParameterType::IntRange);
        ParameterDefinition def = m_driver->parameter(name);

        // Set min
        bool ok = m_driver->setParameter(name, static_cast<int>(def.constraint.minValue));
        QVERIFY2(ok,
                 qPrintable(QString("Should set '%1' to min value %2")
                     .arg(name).arg(def.constraint.minValue)));

        // Set max
        ok = m_driver->setParameter(name, static_cast<int>(def.constraint.maxValue));
        QVERIFY2(ok,
                 qPrintable(QString("Should set '%1' to max value %2")
                     .arg(name).arg(def.constraint.maxValue)));
    }

    // Helper: test setting each valid value of a collection parameter
    void testCollectionParam(const QString &name, ParameterType expectedType)
    {
        verifyParamReadWrite(name, expectedType);
        ParameterDefinition def = m_driver->parameter(name);

        QVERIFY2(!def.constraint.validValues.isEmpty(),
                 qPrintable(QString("Parameter '%1' should have valid values").arg(name)));

        for (const QVariant &v : def.constraint.validValues) {
            bool ok = m_driver->setParameter(name, v);
            QVERIFY2(ok,
                     qPrintable(QString("Should set '%1' to value '%2'")
                         .arg(name).arg(v.toString())));
            QVERIFY2(m_driver->commitParameters(),
                     qPrintable(QString("Should commit '%1' = '%2'")
                         .arg(name).arg(v.toString())));
        }
    }

    //==========================================================================
    // Info Parameters (Read-only)
    //==========================================================================

    void test_param_vendor()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("vendor", ParameterType::String);
    }

    void test_param_model()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("model", ParameterType::String);
    }

    void test_param_camera_id()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("camera_id", ParameterType::String);
    }

    void test_param_bus()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("bus", ParameterType::String);
    }

    void test_param_camera_version()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("camera_version", ParameterType::String);
    }

    void test_param_driver_version()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("driver_version", ParameterType::String);
    }

    void test_param_module_version()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("module_version", ParameterType::String);
    }

    void test_param_dcamapi_version()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("dcamapi_version", ParameterType::String);
    }

    void test_param_color_type()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("color_type", ParameterType::StringCollection);
    }

    void test_param_bit_depth()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("bit_depth", ParameterType::IntRange);
    }

    void test_param_detector_pixels_horz()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("detector_pixels_horz", ParameterType::IntRange);
    }

    void test_param_detector_pixels_vert()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("detector_pixels_vert", ParameterType::IntRange);
    }

    void test_param_image_width()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("image_width", ParameterType::IntRange);
    }

    void test_param_image_height()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("image_height", ParameterType::IntRange);
    }

    void test_param_image_pixel_type()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("image_pixel_type", ParameterType::StringCollection);
    }

    void test_param_buffer_pixel_type()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("buffer_pixel_type", ParameterType::StringCollection);
    }

    void test_param_system_alive()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamExists("system_alive");
        ParameterDefinition def = m_driver->parameter("system_alive");
        QVERIFY2(def.isReadOnly,
                 "system_alive should be read-only");
        QVERIFY2(def.isDynamic,
                 "system_alive should be isDynamic");
        QVERIFY2(def.isExtrinsic,
                 "system_alive should be isExtrinsic");
    }

    //==========================================================================
    // Core Parameters
    //==========================================================================

    void test_param_exposure()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        testRangeParam("exposure");
    }

    void test_param_contrast_gain()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        testIntRangeParam("contrast_gain");
    }

    void test_param_trigger_source()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        testCollectionParam("trigger_source", ParameterType::StringCollection);
    }

    void test_param_trigger_mode()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("trigger_mode", ParameterType::StringCollection);
    }

    void test_param_trigger_active()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("trigger_active", ParameterType::StringCollection);
    }

    void test_param_trigger_polarity()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        testCollectionParam("trigger_polarity", ParameterType::StringCollection);
    }

    void test_param_trigger_connector()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("trigger_connector", ParameterType::StringCollection);
    }

    //==========================================================================
    // Cooling Parameters
    //==========================================================================

    void test_param_sensor_temperature()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamExists("sensor_temperature");
        ParameterDefinition def = m_driver->parameter("sensor_temperature");
        QVERIFY2(def.isReadOnly,
                 "sensor_temperature should be read-only");
        QVERIFY2(def.isDynamic,
                 "sensor_temperature should be isDynamic");
        QVERIFY2(def.isExtrinsic,
                 "sensor_temperature should be isExtrinsic");
        QVERIFY2(def.type == ParameterType::FloatRange,
                 "sensor_temperature should be FloatRange");
    }

    void test_param_sensor_cooler()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        testCollectionParam("sensor_cooler", ParameterType::StringCollection);
    }

    void test_param_sensor_temperature_target()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        testRangeParam("sensor_temperature_target");
    }

    void test_param_sensor_cooler_status()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamExists("sensor_cooler_status");
        ParameterDefinition def = m_driver->parameter("sensor_cooler_status");
        QVERIFY2(def.isReadOnly,
                 "sensor_cooler_status should be read-only");
        QVERIFY2(def.isDynamic,
                 "sensor_cooler_status should be isDynamic");
        QVERIFY2(def.isExtrinsic,
                 "sensor_cooler_status should be isExtrinsic");
    }

    //==========================================================================
    // Advanced Parameters
    //==========================================================================

    void test_param_readout_frequency()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadWrite("readout_frequency", ParameterType::FloatCollection);

        ParameterDefinition def = m_driver->parameter("readout_frequency");
        QVERIFY2(!def.constraint.validValues.isEmpty(),
                 "readout_frequency should have valid values");

        for (const QVariant &v : def.constraint.validValues) {
            bool ok = m_driver->setParameter("readout_frequency", v);
            QVERIFY2(ok,
                     qPrintable(QString("Should set readout_frequency to %1").arg(v.toDouble())));
            QVERIFY2(m_driver->commitParameters(),
                     qPrintable(QString("Should commit readout_frequency = %1").arg(v.toDouble())));
        }
    }

    void test_param_sensor_mode()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("sensor_mode", ParameterType::StringCollection);
    }

    void test_param_line_bundle_height()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        testIntRangeParam("line_bundle_height");
    }

    void test_param_capture_mode()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("capture_mode", ParameterType::StringCollection);
    }

    void test_param_binning()
    {
        if (!tryConnect()) { QSKIP("No camera"); }
        verifyParamReadOnly("binning", ParameterType::StringCollection);
    }

    //==========================================================================
    // Parameter Set / Commit Tests
    //==========================================================================

    void test_set_param_unknown()
    {
        if (!tryConnect()) { QSKIP("No camera"); }

        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        bool result = m_driver->setParameter("nonexistent_param_xyz", 42);
        QVERIFY2(result == false,
                 "Setting unknown parameter should return false");

        QVERIFY2(errorSpy.count() > 0,
                 "errorOccurred should be emitted for unknown parameter");
    }

    void test_set_param_readonly_string()
    {
        if (!tryConnect()) { QSKIP("No camera"); }

        // Setting a read-only String param should return true (no-op)
        if (m_params.contains("vendor")) {
            bool result = m_driver->setParameter("vendor", QVariant("test"));
            QVERIFY2(result == true,
                     "Setting read-only parameter should return true (no-op)");
        }
    }

    void test_set_param_invalid_range()
    {
        if (!tryConnect()) { QSKIP("No camera"); }

        if (!m_params.contains("exposure")) {
            QSKIP("No exposure parameter");
        }

        ParameterDefinition def = m_driver->parameter("exposure");
        if (def.type != ParameterType::FloatRange) {
            QSKIP("exposure is not a range type");
        }

        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        // Try a value far outside range
        double wayOut = def.constraint.maxValue * 100 + 99999;
        bool result = m_driver->setParameter("exposure", wayOut);
        QVERIFY2(result == false,
                 qPrintable(QString("Setting exposure to %1 (way above max %2) should fail")
                     .arg(wayOut).arg(def.constraint.maxValue)));

        QVERIFY2(errorSpy.count() > 0,
                 "errorOccurred should be emitted for out-of-range parameter");
    }

    void test_commit_params()
    {
        if (!tryConnect()) { QSKIP("No camera"); }

        if (!m_params.contains("contrast_gain")) {
            QSKIP("No contrast_gain parameter");
        }

        m_driver->setParameter("contrast_gain", 1);
        bool committed = m_driver->commitParameters();
        QVERIFY2(committed,
                 "Should commit contrast_gain=1 successfully");
    }

    void test_commit_invalid_rollback()
    {
        if (!tryConnect()) { QSKIP("No camera"); }

        if (!m_params.contains("contrast_gain")) {
            QSKIP("No contrast_gain parameter");
        }

        // Set a valid value first
        m_driver->setParameter("contrast_gain", 1);
        QVERIFY2(m_driver->commitParameters(),
                 "Should commit valid value");

        // Now try to set an invalid value — setParameter should reject it
        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
        bool result = m_driver->setParameter("contrast_gain", 999);
        QVERIFY2(result == false,
                 "Setting out-of-range contrast_gain should fail");
    }

    //==========================================================================
    // Capture Tests
    //==========================================================================

    void test_capture_without_connection()
    {
        QVERIFY2(!m_driver->isConnected(),
                 "Should not be connected at start of this test");

        bool started = m_driver->startCapture(1);
        QVERIFY2(started == false,
                 "startCapture should fail without connection");
        QVERIFY2(m_driver->state() != CameraState::Acquiring,
                 "State should not be Acquiring after failed capture start");
    }

    void test_single_capture()
    {
        if (!tryConnect()) { QSKIP("No Hamamatsu camera found, skipping test"); }

        QSignalSpy startedSpy(m_driver, &ICameraDriver::captureStarted);
        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

        // Set a reasonable exposure
        if (m_params.contains("exposure")) {
            bool suc = m_driver->setParameter("exposure", 100000.0);  // 100ms
            suc &= m_driver->commitParameters();
            QVERIFY2(suc, "Setting exposure should succeed");
        }

        bool started = m_driver->startCapture(1);
        QVERIFY2(started,
                 "Should start single capture successfully");
        QVERIFY2(m_driver->state() == CameraState::Acquiring,
                 "State should be Acquiring after capture start");

        QVERIFY2(startedSpy.count() > 0 || startedSpy.wait(1000),
                 "captureStarted signal was not received");

        // Wait for frame
        QVERIFY2(frameSpy.wait(10000),
                 "frameReady signal was not received within timeout");

        // Wait for auto-stop
        QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(5000),
                 "captureStopped signal was not received after single frame");

        qDebug() << "Single capture: received" << frameSpy.count() << "frame(s)";

        // Verify frame content
        if (frameSpy.count() > 0) {
            QVariantList args = frameSpy.takeFirst();
            QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
            quint64 timestamp = args.at(1).value<quint64>();
            int frameNumber = args.at(2).value<int>();
            QString cameraId = args.at(3).toString();

            QVERIFY2(!image->isNull(),
                     "Captured image should not be null");
            QVERIFY2(image->width() > 0,
                     qPrintable(QString("Image width should be > 0, got %1").arg(image->width())));
            QVERIFY2(image->height() > 0,
                     qPrintable(QString("Image height should be > 0, got %1").arg(image->height())));
            QVERIFY2(timestamp > 0,
                     "Timestamp should be set");
            QVERIFY2(frameNumber > 0,
                     qPrintable(QString("Frame number should be positive, got %1").arg(frameNumber)));
            QVERIFY2(!cameraId.isEmpty(),
                     "Camera ID in frameReady should not be empty");
        }

        QVERIFY2(m_driver->state() != CameraState::Acquiring,
                 "State should not be Acquiring after single frame");
    }

    void test_live_capture()
    {
        if (!tryConnect()) { QSKIP("No Hamamatsu camera found, skipping test"); }

        QSignalSpy startedSpy(m_driver, &ICameraDriver::captureStarted);
        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

        // Set a short exposure for live capture
        if (m_params.contains("exposure")) {
            m_driver->setParameter("exposure", 50000.0);  // 50ms
            m_driver->commitParameters();
        }

        bool started = m_driver->startCapture(0);  // 0 = continuous
        QVERIFY2(started,
                 "Should start live capture successfully");
        QVERIFY2(m_driver->state() == CameraState::Acquiring,
                 "State should be Acquiring after live capture start");

        QVERIFY2(startedSpy.count() > 0 || startedSpy.wait(1000),
                 "captureStarted signal was not received");

        // Wait for first frame
        QVERIFY2(frameSpy.wait(10000),
                 "frameReady signal was not received within timeout");

        // Let a few frames accumulate
        QTest::qWait(500);
        int frameCountBeforeStop = frameSpy.count();
        qDebug() << "Live capture: received" << frameCountBeforeStop << "frame(s) before stop";

        // Stop capture
        m_driver->stopCapture(5000);
        QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(3000),
                 "captureStopped signal was not received after stopCapture()");

        // Verify state
        QVERIFY2(m_driver->state() != CameraState::Acquiring,
                 "State should not be Acquiring after stopCapture()");

        // After stopping, frame count should be >= what we had before stop
        int finalFrameCount = frameSpy.count();
        QVERIFY2(finalFrameCount >= frameCountBeforeStop,
                 qPrintable(QString("Live capture: expected >= %1 frames, got %2")
                     .arg(frameCountBeforeStop).arg(finalFrameCount)));
        QVERIFY2(finalFrameCount > 0,
                 "Live capture should have received at least 1 frame");
    }

    void test_burst_capture()
    {
        if (!tryConnect()) { QSKIP("No Hamamatsu camera found, skipping test"); }

        const int burstCount = 3;

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

        // Set a short exposure for burst capture
        if (m_params.contains("exposure")) {
            m_driver->setParameter("exposure", 50000.0);  // 50ms
            m_driver->commitParameters();
        }

        bool started = m_driver->startCapture(burstCount);
        QVERIFY2(started,
                 qPrintable(QString("Should start burst capture of %1 frames").arg(burstCount)));
        QVERIFY2(m_driver->state() == CameraState::Acquiring,
                 "State should be Acquiring after burst capture start");

        // Wait for all frames and auto-stop
        int waits = 0;
        while (frameSpy.count() < burstCount && waits < 30) {
            if (frameSpy.wait(1000)) {
                qDebug() << "Burst: received" << frameSpy.count() << "/" << burstCount << "frames";
            }
            waits++;
        }

        QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(3000),
                 "captureStopped signal was not received after burst complete");

        qDebug() << "Burst capture: received" << frameSpy.count() << "frame(s)";

        QVERIFY2(frameSpy.count() == burstCount,
                 qPrintable(QString("Burst capture should receive exactly %1 frames, got %2")
                     .arg(burstCount).arg(frameSpy.count())));

        QVERIFY2(m_driver->state() != CameraState::Acquiring,
                 "State should not be Acquiring after burst capture complete");
    }

    void test_capture_stop_before_start()
    {
        if (!tryConnect()) { QSKIP("No camera"); }

        // stopCapture when not capturing should not crash or cause issues
        m_driver->stopCapture(100);
        QVERIFY2(m_driver->state() == CameraState::Connected,
                 "State should remain Connected after stopping inactive capture");
    }
};

QTEST_MAIN(TestHamamatsuDriver)
#include "test_hamamatsu_driver_C16091-10.moc"
