#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QSharedPointer>

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "plugins/mock/MockCameraDriver.h"

class TestMockDriver : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_driver = new MockCameraDriver();
    }

    void cleanup()
    {
        if (m_driver) {
            if (m_driver->isConnected()) {
                m_driver->disconnectCamera();
            }
            delete m_driver;
            m_driver = nullptr;
        }
    }

    //==========================================================================
    // Enumeration Tests
    //==========================================================================
    void test_enumerate()
    {
        QStringList cameras = m_driver->enumerate();

        QVERIFY2(cameras.size() == 3,
                 qPrintable(QString("Expected 3 cameras, got %1").arg(cameras.size())));
        QVERIFY2(cameras.contains("mock-001"), "Camera list should contain 'mock-001'");
        QVERIFY2(cameras.contains("mock-002"), "Camera list should contain 'mock-002'");
        QVERIFY2(cameras.contains("mock-003"), "Camera list should contain 'mock-003'");
    }

    //==========================================================================
    // Connection Tests
    //==========================================================================
    void test_connect_valid_camera()
    {
        bool connected = m_driver->connectToCamera("mock-001");
        QVERIFY2(connected == true, "Should connect successfully to mock-001");
        QVERIFY2(m_driver->isConnected() == true,
                 "isConnected should return true after connection");
        QVERIFY2(m_driver->state() == CameraState::Connected,
                 "State should be Connected after connection");
    }

    void test_connect_invalid_camera()
    {
        bool connected = m_driver->connectToCamera("invalid-camera");
        QVERIFY2(connected == false, "Should fail to connect to invalid camera");
        QVERIFY2(m_driver->isConnected() == false,
                 "isConnected should return false after failed connection");
    }

    void test_disconnect()
    {
        QVERIFY2(m_driver->connectToCamera("mock-002") == true, "Should connect successfully");
        QVERIFY2(m_driver->isConnected() == true, "Should be connected");

        m_driver->disconnectCamera();
        QVERIFY2(m_driver->isConnected() == false,
                 "Should be disconnected after disconnectCamera()");
        QVERIFY2(m_driver->state() == CameraState::Disconnected,
                 "State should be Disconnected after disconnect");
    }

    void test_disconnect_when_not_connected()
    {
        QVERIFY2(m_driver->isConnected() == false, "Should not be connected initially");
        m_driver->disconnectCamera();
        QVERIFY2(m_driver->isConnected() == false, "Should still not be connected");
    }

    void test_connection_signal()
    {
        QSignalSpy spy(m_driver, &ICameraDriver::connectionChanged);

        m_driver->connectToCamera("mock-001");

        QVERIFY2(spy.count() == 1,
                 qPrintable(QString("Expected 1 connectionChanged signal, got %1").arg(spy.count())));
        if (spy.count() > 0) {
            QVariantList args = spy.takeFirst();
            QVERIFY2(args.at(0).toBool() == true, "Signal argument should be true for connection");
            QVERIFY2(args.at(1).toString() == "mock-001", "Camera ID should be mock-001");
        }
    }

    void test_reconnect()
    {
        m_driver->connectToCamera("mock-001");
        QVERIFY2(m_driver->isConnected() == true, "Should be connected to mock-001");

        bool result = m_driver->connectToCamera("mock-002");
        QVERIFY2(result == true, "Should successfully reconnect");
        QVERIFY2(m_driver->isConnected() == true, "Should still be connected");
        QVERIFY2(m_driver->cameraId() == "mock-002", "Camera ID should be mock-002");
    }

    //==========================================================================
    // Parameter System Tests
    //==========================================================================
    void test_parameter_names()
    {
        m_driver->connectToCamera("mock-001");

        QStringList params = m_driver->parameterNames();
        QVERIFY2(params.contains("exposure"), "Should have exposure parameter");
        QVERIFY2(params.contains("gain"), "Should have gain parameter");
        QVERIFY2(params.contains("offset"), "Should have offset parameter");
        QVERIFY2(params.contains("binning"), "Should have binning parameter");
        QVERIFY2(params.contains("roi_x"), "Should have roi_x parameter");
    }

    void test_parameter_definition()
    {
        m_driver->connectToCamera("mock-001");

        ParameterDefinition expDef = m_driver->parameter("exposure");
        QVERIFY2(expDef.isValid(), "Exposure definition should be valid");
        QVERIFY2(expDef.name == "exposure", "Name should be exposure");
        QVERIFY2(expDef.type == ParameterType::FloatRange, "Type should be FloatRange");

        ParameterDefinition gainDef = m_driver->parameter("gain");
        QVERIFY2(gainDef.isValid(), "Gain definition should be valid");

        ParameterDefinition invalidDef = m_driver->parameter("nonexistent");
        QVERIFY2(!invalidDef.isValid(), "Invalid parameter should not be valid");
    }

    void test_parameter_value()
    {
        m_driver->connectToCamera("mock-001");

        QVariant exposure = m_driver->parameterValue("exposure");
        QVERIFY2(exposure.isValid(), "Should get valid exposure value");
        QVERIFY2(exposure.toDouble() == 100.0, "Default exposure should be 100.0");

        QVariant gain = m_driver->parameterValue("gain");
        QVERIFY2(gain.isValid(), "Should get valid gain value");
        QVERIFY2(gain.toDouble() == 1.0, "Default gain should be 1.0");
    }

    void test_set_parameter()
    {
        m_driver->connectToCamera("mock-001");

        // Test FloatRange parameters
        bool result = m_driver->setParameter("exposure", 200.0);
        QVERIFY2(result == true, "Should set exposure successfully");
        QVariant value = m_driver->parameterValue("exposure");
        QVERIFY2(value.toDouble() == 200.0, "Exposure should be 200.0 after setParameter");

        result = m_driver->setParameter("gain", 15.5);
        QVERIFY2(result == true, "Should set gain successfully");
        value = m_driver->parameterValue("gain");
        QVERIFY2(value.toDouble() == 15.5, "Gain should be 15.5 after setParameter");

        result = m_driver->setParameter("offset", 100.0);
        QVERIFY2(result == true, "Should set offset successfully");
        value = m_driver->parameterValue("offset");
        QVERIFY2(value.toDouble() == 100.0, "Offset should be 100.0 after setParameter");

        result = m_driver->setParameter("cooling_target_temp", -30.0);
        QVERIFY2(result == true, "Should set cooling_target_temp successfully");
        value = m_driver->parameterValue("cooling_target_temp");
        QVERIFY2(value.toDouble() == -30.0, "cooling_target_temp should be -30.0 after setParameter");

        // Test IntRange parameters
        result = m_driver->setParameter("roi_x", 100);
        QVERIFY2(result == true, "Should set roi_x successfully");
        value = m_driver->parameterValue("roi_x");
        QVERIFY2(value.toInt() == 100, "roi_x should be 100 after setParameter");

        result = m_driver->setParameter("roi_y", 200);
        QVERIFY2(result == true, "Should set roi_y successfully");
        value = m_driver->parameterValue("roi_y");
        QVERIFY2(value.toInt() == 200, "roi_y should be 200 after setParameter");

        result = m_driver->setParameter("roi_width", 1024);
        QVERIFY2(result == true, "Should set roi_width successfully");
        value = m_driver->parameterValue("roi_width");
        QVERIFY2(value.toInt() == 1024, "roi_width should be 1024 after setParameter");

        result = m_driver->setParameter("roi_height", 768);
        QVERIFY2(result == true, "Should set roi_height successfully");
        value = m_driver->parameterValue("roi_height");
        QVERIFY2(value.toInt() == 768, "roi_height should be 768 after setParameter");

        result = m_driver->setParameter("pattern_type", 2);
        QVERIFY2(result == true, "Should set pattern_type successfully");
        value = m_driver->parameterValue("pattern_type");
        QVERIFY2(value.toInt() == 2, "pattern_type should be 2 after setParameter");

        // Test IntCollection parameter
        result = m_driver->setParameter("binning", 2);
        QVERIFY2(result == true, "Should set binning successfully");
        value = m_driver->parameterValue("binning");
        QVERIFY2(value.toInt() == 2, "binning should be 2 after setParameter");

        // Test Boolean parameters
        result = m_driver->setParameter("vertical_binning", true);
        QVERIFY2(result == true, "Should set vertical_binning successfully");
        value = m_driver->parameterValue("vertical_binning");
        QVERIFY2(value.toBool() == true, "vertical_binning should be true after setParameter");

        result = m_driver->setParameter("cooling_enabled", true);
        QVERIFY2(result == true, "Should set cooling_enabled successfully");
        value = m_driver->parameterValue("cooling_enabled");
        QVERIFY2(value.toBool() == true, "cooling_enabled should be true after setParameter");
    }

    void test_set_invalid_parameter()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("exposure", -10.0);
        QVERIFY2(result == false, "Should reject negative exposure");

        result = m_driver->setParameter("nonexistent", 100.0);
        QVERIFY2(result == false, "Should reject nonexistent parameter");
    }

    void test_validate_parameters()
    {
        m_driver->connectToCamera("mock-001");

        m_driver->setParameter("exposure", 200.0);
        m_driver->setParameter("gain", 10.0);
        QVERIFY2(m_driver->validateParameters() == true, "Should validate with valid params");

        bool result = m_driver->setParameter("exposure", -10.0);
        QVERIFY2(result == false, "Should reject negative exposure");
        result = m_driver->setParameter("exposure", 20000.0);
        QVERIFY2(result == false, "Should reject exposure beyond max");
        QVERIFY2(m_driver->validateParameters() == true, "Should still validate since invalid params weren't stored");
    }

    void test_commit_parameters()
    {
        m_driver->connectToCamera("mock-001");

        m_driver->setParameter("exposure", 300.0);
        m_driver->setParameter("gain", 20.0);
        m_driver->setParameter("offset", 15.5);
        m_driver->setParameter("roi_x", 100);
        m_driver->setParameter("cooling_enabled", true);
        m_driver->setParameter("cooling_target_temp", -20.0);
        QVERIFY2(m_driver->commitParameters() == true, "Should commit successfully");

        QVariant exp = m_driver->parameterValue("exposure");
        QVariant g = m_driver->parameterValue("gain");
        QVariant off = m_driver->parameterValue("offset");
        QVariant roiX = m_driver->parameterValue("roi_x");
        QVariant coolEn = m_driver->parameterValue("cooling_enabled");
        QVariant coolTemp = m_driver->parameterValue("cooling_target_temp");
        QVERIFY2(exp.toDouble() == 300.0, "Exposure should be 300.0 after commit");
        QVERIFY2(g.toDouble() == 20.0, "Gain should be 20.0 after commit");
        QVERIFY2(off.toDouble() == 15.5, "Offset should be 15.5 after commit");
        QVERIFY2(roiX.toInt() == 100, "roi_x should be 100 after commit");
        QVERIFY2(coolEn.toBool() == true, "cooling_enabled should be true after commit");
        QVERIFY2(coolTemp.toDouble() == -20.0, "cooling_target_temp should be -20.0 after commit");
    }

    void test_read_only_parameter()
    {
        m_driver->connectToCamera("mock-001");

        ParameterDefinition sensorWidthDef = m_driver->parameter("sensor_width");
        QVERIFY2(sensorWidthDef.isReadOnly == true, "sensor_width should be read-only");

        QVariant widthBefore = m_driver->parameterValue("sensor_width");
        m_driver->setParameter("sensor_width", 1024);
        QVariant widthAfter = m_driver->parameterValue("sensor_width");
        QVERIFY2(widthBefore == widthAfter, "Read-only parameter should not change");
    }

    //==========================================================================
    // Capture Tests
    //==========================================================================
    void test_capture_without_connection()
    {
        bool started = m_driver->startCapture(1);
        QVERIFY2(started == false, "Should fail to start capture without connection");
        QVERIFY2(m_driver->state() != CameraState::Acquiring, "state should not be Acquiring");
    }

    void test_single_capture()
    {
        m_driver->connectToCamera("mock-001");
        m_driver->setParameter("exposure", 50.0);
        m_driver->commitParameters();

        QSignalSpy startedSpy(m_driver, &ICameraDriver::captureStarted);
        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

        bool started = m_driver->startCapture(1);
        QVERIFY2(started == true, "Should start single capture successfully");
        QVERIFY2(m_driver->state() == CameraState::Acquiring, "state should be Acquiring after capture start");

        QVERIFY2(startedSpy.count() > 0 || startedSpy.wait(500),
                 "captureStarted signal was not received");

        QVERIFY2(frameSpy.wait(500), "frameReady signal was not received within timeout");

        QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(500),
                 "captureStopped signal was not received");

        if (frameSpy.count() > 0) {
            QVariantList args = frameSpy.takeFirst();
            QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
            quint64 timestamp = args.at(1).value<quint64>();
            int frameNumber = args.at(2).value<int>();
            QString cameraId = args.at(3).toString();

            QVERIFY2(!image->isNull(), "Image should not be null");
            QVERIFY2(timestamp > 0, "Timestamp should be set");
            QVERIFY2(frameNumber > 0, "Frame number should be positive");
            QVERIFY2(cameraId == "mock-001", "Camera ID should be mock-001");
        }

        QVERIFY2(m_driver->state() != CameraState::Acquiring, "Should stop capturing after single frame");
    }

    void test_continuous_capture()
    {
        m_driver->connectToCamera("mock-001");
        m_driver->setParameter("exposure", 20.0);
        m_driver->commitParameters();

        QSignalSpy startedSpy(m_driver, &ICameraDriver::captureStarted);
        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

        bool started = m_driver->startCapture(0);
        QVERIFY2(started == true, "Should start continuous capture");
        QVERIFY2(m_driver->state() == CameraState::Acquiring, "Should be capturing");

        QVERIFY2(startedSpy.count() > 0 || startedSpy.wait(500),
                 "captureStarted signal was not received");

        QVERIFY2(frameSpy.wait(500), "Should receive at least one frame");

        int frameCountBeforeStop = frameSpy.count();

        m_driver->stopCapture(1000);

        QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(500),
                 "captureStopped signal was not received");

        QVERIFY2(m_driver->state() != CameraState::Acquiring, "Should stop capturing after stopCapture");
        QVERIFY2(frameSpy.count() > frameCountBeforeStop,
                 "Should have received more frames before stop");
    }

    void test_burst_capture()
    {
        m_driver->connectToCamera("mock-001");
        m_driver->setParameter("exposure", 10.0);
        m_driver->commitParameters();

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(m_driver, &ICameraDriver::captureStopped);

        bool started = m_driver->startCapture(3);
        QVERIFY2(started == true, "Should start burst capture");

        QVERIFY2(frameSpy.wait(2000), "Should receive 3 frames");

        QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(1000),
                 "captureStopped signal was not received");

        QVERIFY2(frameSpy.count() == 3,
                 qPrintable(QString("Expected 3 frames, got %1").arg(frameSpy.count())));

        QVERIFY2(m_driver->state() != CameraState::Acquiring, "Should stop capturing after burst complete");
    }

    void test_stop_capture_timeout()
    {
        m_driver->connectToCamera("mock-001");
        m_driver->setParameter("exposure", 1000.0);
        m_driver->commitParameters();

        bool started = m_driver->startCapture(0);
        QVERIFY2(started == true, "Should start capture");

        QTest::qWait(50);

        m_driver->stopCapture(100);

        QVERIFY2(m_driver->state() != CameraState::Acquiring, "Should stop capturing after stopCapture with timeout");
    }

    //==========================================================================
    // State Machine Tests
    //==========================================================================
    void test_state_disconnected()
    {
        QVERIFY2(m_driver->state() == CameraState::Disconnected,
                 "Initial state should be Disconnected");
        QVERIFY2(m_driver->cameraId() == "", "Camera ID should be empty when disconnected");
    }

    void test_state_connected()
    {
        m_driver->connectToCamera("mock-001");
        QVERIFY2(m_driver->state() == CameraState::Connected,
                 "State should be Connected after connection");
        QVERIFY2(m_driver->cameraId() == "mock-001", "Camera ID should be mock-001");
    }

    void test_state_acquiring()
    {
        m_driver->connectToCamera("mock-001");
        m_driver->startCapture(1);

        QVERIFY2(m_driver->state() == CameraState::Acquiring,
                 "State should be Acquiring during capture");

        QTest::qWait(200);

        QVERIFY2(m_driver->state() == CameraState::Connected ||
                 m_driver->state() == CameraState::Acquiring,
                 "State should be Acquiring or returning to Connected");
    }

    void test_driver_version()
    {
        QString version = m_driver->driverVersion();
        QVERIFY2(!version.isEmpty(), "Driver version should not be empty");
        QVERIFY2(version == "1.0.0", "Driver version should be 1.0.0");
    }

    //==========================================================================
    // Error Handling Tests
    //==========================================================================
    void test_error_signal_on_invalid_connect()
    {
        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        m_driver->connectToCamera("invalid-camera");

        QVERIFY2(errorSpy.count() > 0, "Should emit error for invalid camera");
    }

    void test_error_signal_on_invalid_parameter()
    {
        m_driver->connectToCamera("mock-001");

        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        m_driver->setParameter("exposure", -100.0);

        QVERIFY2(errorSpy.count() > 0, "Should emit error for invalid parameter value");
    }

    //==========================================================================
    // Camera-Specific Tests
    //==========================================================================
    void test_camera_001()
    {
        m_driver->connectToCamera("mock-001");
        QVariant width = m_driver->parameterValue("sensor_width");
        QVariant height = m_driver->parameterValue("sensor_height");
        QVERIFY2(width.toInt() == 2048, "mock-001 width should be 2048");
        QVERIFY2(height.toInt() == 2048, "mock-001 height should be 2048");
    }

    void test_camera_002()
    {
        m_driver->connectToCamera("mock-002");
        QVariant width = m_driver->parameterValue("sensor_width");
        QVariant height = m_driver->parameterValue("sensor_height");
        QVERIFY2(width.toInt() == 4096, "mock-002 width should be 4096");
        QVERIFY2(height.toInt() == 4096, "mock-002 height should be 4096");
    }

    void test_camera_003()
    {
        m_driver->connectToCamera("mock-003");
        QVariant width = m_driver->parameterValue("sensor_width");
        QVariant height = m_driver->parameterValue("sensor_height");
        QVERIFY2(width.toInt() == 1024, "mock-003 width should be 1024");
        QVERIFY2(height.toInt() == 1024, "mock-003 height should be 1024");
    }

    //==========================================================================
    // ConnectionChanged Signal Tests
    //==========================================================================
    void test_connection_changed_on_disconnect()
    {
        QSignalSpy spy(m_driver, &ICameraDriver::connectionChanged);

        m_driver->connectToCamera("mock-001");

        QVERIFY2(spy.count() >= 1, "Should emit connectionChanged on connect");
        spy.clear();

        m_driver->disconnectCamera();

        QVERIFY2(spy.count() == 1,
                 qPrintable(QString("Expected 1 connectionChanged signal on disconnect, got %1").arg(spy.count())));
        if (spy.count() > 0) {
            QVariantList args = spy.takeFirst();
            QVERIFY2(args.at(0).toBool() == false,
                     "First argument should be false for disconnection");
            QVERIFY2(args.at(1).toString() == "mock-001",
                     "Second argument should be camera ID 'mock-001'");
        }
    }

    //==========================================================================
    // Error Signal Payload Tests
    //==========================================================================
    void test_error_payload_on_invalid_connect()
    {
        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        m_driver->connectToCamera("invalid-camera");

        QVERIFY2(errorSpy.count() >= 1,
                 "Should emit errorOccurred for invalid camera");

        if (errorSpy.count() > 0) {
            QVariantList args = errorSpy.takeFirst();
            QVERIFY2(args.size() >= 1, "Signal should have at least 1 argument");
            CameraError err = args.at(0).value<CameraError>();
            QVERIFY2(err.code != CameraError::Code::None,
                     "Error code should not be None for invalid camera");
            QVERIFY2(err.severity == CameraError::Severity::Error ||
                     err.severity == CameraError::Severity::Fatal,
                     "Error severity should be Error or Fatal");
            QVERIFY2(!err.description.isEmpty(),
                     "Error description should be non-empty");
        }
    }

    void test_error_payload_on_invalid_parameter()
    {
        m_driver->connectToCamera("mock-001");

        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        bool result = m_driver->setParameter("exposure", -100.0);
        QVERIFY2(result == false, "Should reject negative exposure");

        QVERIFY2(errorSpy.count() >= 1,
                 "Should emit errorOccurred for invalid parameter value");

        if (errorSpy.count() > 0) {
            QVariantList args = errorSpy.takeFirst();
            CameraError err = args.at(0).value<CameraError>();
            QVERIFY2(err.code != CameraError::Code::None,
                     "Error code should not be None for invalid parameter");
            QVERIFY2(!err.description.isEmpty(),
                     "Error description should be non-empty");
        }
    }

    //==========================================================================
    // Cooling Parameter Tests
    //==========================================================================
    void test_cooling_enable()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("cooling_enabled", true);
        QVERIFY2(result == true, "Should set cooling_enabled to true");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("cooling_enabled");
        QVERIFY2(value.toBool() == true, "cooling_enabled should be true after set and commit");
    }

    void test_cooling_disable()
    {
        m_driver->connectToCamera("mock-001");

        m_driver->setParameter("cooling_enabled", true);
        m_driver->commitParameters();

        bool result = m_driver->setParameter("cooling_enabled", false);
        QVERIFY2(result == true, "Should set cooling_enabled to false");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("cooling_enabled");
        QVERIFY2(value.toBool() == false, "cooling_enabled should be false after set and commit");
    }

    void test_cooling_target_temp_valid()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("cooling_target_temp", -30.0);
        QVERIFY2(result == true, "Should set cooling_target_temp to -30.0");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("cooling_target_temp");
        QVERIFY2(qAbs(value.toDouble() - (-30.0)) < 0.1,
                 "cooling_target_temp should be -30.0 after commit");
    }

    void test_cooling_target_temp_min()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("cooling_target_temp", -40.0);
        QVERIFY2(result == true, "Should set cooling_target_temp to -40.0 (min for mock-001)");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("cooling_target_temp");
        QVERIFY2(qAbs(value.toDouble() - (-40.0)) < 0.1,
                 "cooling_target_temp should be -40.0 after commit");
    }

    void test_cooling_target_temp_max()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("cooling_target_temp", 25.0);
        QVERIFY2(result == true, "Should set cooling_target_temp to 25.0 (max for mock-001)");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("cooling_target_temp");
        QVERIFY2(qAbs(value.toDouble() - 25.0) < 0.1,
                 "cooling_target_temp should be 25.0 after commit");
    }

    void test_cooling_target_temp_below_min()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("cooling_target_temp", -50.0);
        QVERIFY2(result == false,
                 "Should reject cooling_target_temp -50.0 (below -40 min for mock-001)");
    }

    void test_cooling_target_temp_above_max()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("cooling_target_temp", 30.0);
        QVERIFY2(result == false,
                 "Should reject cooling_target_temp 30.0 (above 25 max for mock-001)");
    }

    void test_cooling_sensor_temp_readonly()
    {
        m_driver->connectToCamera("mock-001");

        ParameterDefinition def = m_driver->parameter("cooling_sensor_temp");
        QVERIFY2(def.isReadOnly == true, "cooling_sensor_temp should be read-only");
    }

    void test_cooling_heatsink_temp_readonly()
    {
        m_driver->connectToCamera("mock-001");

        ParameterDefinition def = m_driver->parameter("cooling_heatsink_temp");
        QVERIFY2(def.isReadOnly == true, "cooling_heatsink_temp should be read-only");
    }

    //==========================================================================
    // ROI Parameter Tests (mock-001: max 2048x2048)
    //==========================================================================
    void test_roi_set_xy()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("roi_x", 100);
        QVERIFY2(result == true, "Should set roi_x to 100");
        result = m_driver->setParameter("roi_y", 200);
        QVERIFY2(result == true, "Should set roi_y to 200");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant xVal = m_driver->parameterValue("roi_x");
        QVariant yVal = m_driver->parameterValue("roi_y");
        QVERIFY2(xVal.toInt() == 100, "roi_x should be 100 after commit");
        QVERIFY2(yVal.toInt() == 200, "roi_y should be 200 after commit");
    }

    void test_roi_set_width_height()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("roi_width", 1024);
        QVERIFY2(result == true, "Should set roi_width to 1024");
        result = m_driver->setParameter("roi_height", 1024);
        QVERIFY2(result == true, "Should set roi_height to 1024");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant wVal = m_driver->parameterValue("roi_width");
        QVariant hVal = m_driver->parameterValue("roi_height");
        QVERIFY2(wVal.toInt() == 1024, "roi_width should be 1024 after commit");
        QVERIFY2(hVal.toInt() == 1024, "roi_height should be 1024 after commit");
    }

    void test_roi_full_sensor()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("roi_x", 0);
        QVERIFY2(result == true, "Should set roi_x to 0");
        result = m_driver->setParameter("roi_y", 0);
        QVERIFY2(result == true, "Should set roi_y to 0");
        result = m_driver->setParameter("roi_width", 2048);
        QVERIFY2(result == true, "Should set roi_width to 2048");
        result = m_driver->setParameter("roi_height", 2048);
        QVERIFY2(result == true, "Should set roi_height to 2048");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant xVal = m_driver->parameterValue("roi_x");
        QVariant yVal = m_driver->parameterValue("roi_y");
        QVariant wVal = m_driver->parameterValue("roi_width");
        QVariant hVal = m_driver->parameterValue("roi_height");
        QVERIFY2(xVal.toInt() == 0, "roi_x should be 0 for full sensor");
        QVERIFY2(yVal.toInt() == 0, "roi_y should be 0 for full sensor");
        QVERIFY2(wVal.toInt() == 2048, "roi_width should be 2048 for full sensor");
        QVERIFY2(hVal.toInt() == 2048, "roi_height should be 2048 for full sensor");
    }

    void test_roi_x_below_min()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("roi_x", -1);
        QVERIFY2(result == false, "Should reject roi_x = -1 (below min of 0)");
    }

    void test_roi_x_above_max()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("roi_x", 3000);
        QVERIFY2(result == false, "Should reject roi_x = 3000 (above max 2047 for mock-001)");
    }

    void test_roi_width_zero()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("roi_width", 0);
        QVERIFY2(result == false, "Should reject roi_width = 0 (min is 1)");
    }

    void test_roi_height_above_max()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("roi_height", 3000);
        QVERIFY2(result == false, "Should reject roi_height = 3000 (above max 2048 for mock-001)");
    }

    //==========================================================================
    // Binning Tests (mock-001: {1, 2, 4, 8})
    //==========================================================================
    void test_binning_valid_1()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("binning", 1);
        QVERIFY2(result == true, "Should set binning to 1");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("binning");
        QVERIFY2(value.toInt() == 1, "binning should be 1 after commit");
    }

    void test_binning_valid_8()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("binning", 8);
        QVERIFY2(result == true, "Should set binning to 8");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("binning");
        QVERIFY2(value.toInt() == 8, "binning should be 8 after commit");
    }

    void test_binning_invalid_3()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("binning", 3);
        QVERIFY2(result == false, "Should reject binning = 3 (not in {1,2,4,8})");
    }

    void test_binning_invalid_0()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("binning", 0);
        QVERIFY2(result == false, "Should reject binning = 0");
    }

    void test_binning_invalid_negative()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("binning", -1);
        QVERIFY2(result == false, "Should reject binning = -1");
    }

    //==========================================================================
    // Vertical Binning Tests (Boolean)
    //==========================================================================
    void test_vertical_binning_enable()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("vertical_binning", true);
        QVERIFY2(result == true, "Should set vertical_binning to true");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("vertical_binning");
        QVERIFY2(value.toBool() == true, "vertical_binning should be true after commit");
    }

    void test_vertical_binning_disable()
    {
        m_driver->connectToCamera("mock-001");

        m_driver->setParameter("vertical_binning", true);
        m_driver->commitParameters();

        bool result = m_driver->setParameter("vertical_binning", false);
        QVERIFY2(result == true, "Should set vertical_binning to false");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QVariant value = m_driver->parameterValue("vertical_binning");
        QVERIFY2(value.toBool() == false, "vertical_binning should be false after commit");
    }

    //==========================================================================
    // Pattern Type Tests (IntRange 0-3)
    //==========================================================================
    void test_pattern_gradient()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("pattern_type", 0);
        QVERIFY2(result == true, "Should set pattern_type to 0 (gradient)");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        result = m_driver->startCapture(1);
        QVERIFY2(result == true, "Should start capture with gradient pattern");

        QVERIFY2(frameSpy.wait(500), "Should receive frameReady signal");

        if (frameSpy.count() > 0) {
            QVariantList args = frameSpy.takeFirst();
            QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image->isNull(), "Image should not be null for gradient pattern");
        }
    }

    void test_pattern_noise()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("pattern_type", 1);
        QVERIFY2(result == true, "Should set pattern_type to 1 (noise)");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        result = m_driver->startCapture(1);
        QVERIFY2(result == true, "Should start capture with noise pattern");

        QVERIFY2(frameSpy.wait(500), "Should receive frameReady signal");

        if (frameSpy.count() > 0) {
            QVariantList args = frameSpy.takeFirst();
            QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image->isNull(), "Image should not be null for noise pattern");
        }
    }

    void test_pattern_interference()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("pattern_type", 2);
        QVERIFY2(result == true, "Should set pattern_type to 2 (interference)");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        result = m_driver->startCapture(1);
        QVERIFY2(result == true, "Should start capture with interference pattern");

        QVERIFY2(frameSpy.wait(500), "Should receive frameReady signal");

        if (frameSpy.count() > 0) {
            QVariantList args = frameSpy.takeFirst();
            QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image->isNull(), "Image should not be null for interference pattern");
        }
    }

    void test_pattern_fastfill()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("pattern_type", 3);
        QVERIFY2(result == true, "Should set pattern_type to 3 (fastfill)");
        QVERIFY2(m_driver->commitParameters() == true, "Should commit parameters");

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        result = m_driver->startCapture(1);
        QVERIFY2(result == true, "Should start capture with fastfill pattern");

        QVERIFY2(frameSpy.wait(500), "Should receive frameReady signal");

        if (frameSpy.count() > 0) {
            QVariantList args = frameSpy.takeFirst();
            QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image->isNull(), "Image should not be null for fastfill pattern");
        }
    }

    void test_pattern_invalid()
    {
        m_driver->connectToCamera("mock-001");

        bool result = m_driver->setParameter("pattern_type", -1);
        QVERIFY2(result == false, "Should reject pattern_type = -1 (below min 0)");

        result = m_driver->setParameter("pattern_type", 4);
        QVERIFY2(result == false, "Should reject pattern_type = 4 (above max 3)");
    }

    //==========================================================================
    // Info Parameter Tests
    //==========================================================================
    void test_info_bit_depth()
    {
        m_driver->connectToCamera("mock-001");

        ParameterDefinition def = m_driver->parameter("bit_depth");
        QVERIFY2(def.isReadOnly == true, "bit_depth should be read-only");

        QVariant value = m_driver->parameterValue("bit_depth");
        QVERIFY2(value.toString() == "16-bit",
                 qPrintable(QString("bit_depth should be '16-bit', got '%1'").arg(value.toString())));
    }

    void test_info_camera_model()
    {
        m_driver->connectToCamera("mock-001");

        ParameterDefinition def = m_driver->parameter("camera_model");
        QVERIFY2(def.isReadOnly == true, "camera_model should be read-only");

        QVariant value = m_driver->parameterValue("camera_model");
        QVERIFY2(value.toString() == "MockCamera-mock-001",
                 qPrintable(QString("camera_model should be 'MockCamera-mock-001', got '%1'").arg(value.toString())));
    }

    //==========================================================================
    // Capture with ROI/Binning Tests
    //==========================================================================
    void test_capture_with_roi_dimensions()
    {
        m_driver->connectToCamera("mock-001");

        m_driver->setParameter("roi_x", 0);
        m_driver->setParameter("roi_y", 0);
        m_driver->setParameter("roi_width", 512);
        m_driver->setParameter("roi_height", 512);
        QVERIFY2(m_driver->commitParameters() == true, "Should commit ROI parameters");

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        bool started = m_driver->startCapture(1);
        QVERIFY2(started == true, "Should start capture with 512x512 ROI");

        QVERIFY2(frameSpy.wait(500), "Should receive frameReady signal");

        if (frameSpy.count() > 0) {
            QVariantList args = frameSpy.takeFirst();
            QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image->isNull(), "Image should not be null");
            QVERIFY2(image->width() == 512,
                     qPrintable(QString("Image width should be 512, got %1").arg(image->width())));
            QVERIFY2(image->height() == 512,
                     qPrintable(QString("Image height should be 512, got %1").arg(image->height())));
        }
    }

    void test_capture_with_binning()
    {
        m_driver->connectToCamera("mock-001");

        m_driver->setParameter("binning", 2);
        m_driver->setParameter("roi_x", 0);
        m_driver->setParameter("roi_y", 0);
        m_driver->setParameter("roi_width", 2048);
        m_driver->setParameter("roi_height", 2048);
        QVERIFY2(m_driver->commitParameters() == true, "Should commit binning parameters");

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        bool started = m_driver->startCapture(1);
        QVERIFY2(started == true, "Should start capture with binning=2");

        QVERIFY2(frameSpy.wait(500), "Should receive frameReady signal");

        if (frameSpy.count() > 0) {
            QVariantList args = frameSpy.takeFirst();
            QSharedPointer<QImage> image = args.at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image->isNull(), "Image should not be null with binning=2");
        }
    }

private:
    MockCameraDriver *m_driver = nullptr;
};

QTEST_MAIN(TestMockDriver)
#include "test_mock_driver.moc"