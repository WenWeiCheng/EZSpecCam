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

        bool result = m_driver->setParameter("exposure", 200.0);
        QVERIFY2(result == true, "Should set exposure successfully");

        QVariant value = m_driver->parameterValue("exposure");
        QVERIFY2(value.toDouble() == 200.0, "Exposure should be 200.0 after setParameter");
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
        QVERIFY2(m_driver->commitParameters() == true, "Should commit successfully");

        QVariant exp = m_driver->parameterValue("exposure");
        QVariant g = m_driver->parameterValue("gain");
        QVERIFY2(exp.toDouble() == 300.0, "Exposure should be 300.0 after commit");
        QVERIFY2(g.toDouble() == 20.0, "Gain should be 20.0 after commit");
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

private:
    MockCameraDriver *m_driver = nullptr;
};

QTEST_MAIN(TestMockDriver)
#include "test_mock_driver.moc"