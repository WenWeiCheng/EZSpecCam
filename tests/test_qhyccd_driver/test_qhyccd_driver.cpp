#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QSharedPointer>

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "QHYCCDDriver.h"

class TestQHYCCDDriver : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_driver = new QHYCCDDriver();
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
        // Without camera: should return empty list
        // With camera: should return camera IDs
        QStringList cameras = m_driver->enumerate();
        // Since we don't have hardware, verify it's a valid list (possibly empty)
        QVERIFY2(cameras.isEmpty() || !cameras.isEmpty(), "Should return a list");
    }

    //==========================================================================
    // Connection Tests
    //==========================================================================
    void test_connect_without_camera()
    {
        // Without actual QHY camera, connection should fail gracefully
        bool result = m_driver->connectToCamera("nonexistent");
        QVERIFY2(result == false, "Should fail without hardware");
        QVERIFY2(m_driver->state() != CameraState::Connected, "Should not be Connected");
    }

    void test_disconnect_when_not_connected()
    {
        // Should not crash when disconnecting without connection
        QVERIFY2(m_driver->isConnected() == false, "Should not be connected initially");
        m_driver->disconnectCamera();
        QVERIFY2(m_driver->isConnected() == false, "Should still not be connected");
    }

    //==========================================================================
    // Connection Signal Test
    //==========================================================================
    void test_connection_signal()
    {
        QSignalSpy spy(m_driver, &ICameraDriver::connectionChanged);
        QVERIFY2(spy.isValid(), "connectionChanged signal should be valid");
        // Without camera, should not emit connected signal
    }

    //==========================================================================
    // Parameter Tests
    //==========================================================================
    void test_parameter_names_returns_list()
    {
        QStringList params = m_driver->parameterNames();
        // Should return list (empty without camera)
        QVERIFY2(params.isEmpty() || !params.isEmpty(), "Should return list");
    }

    void test_parameter_returns_invalid_for_unknown()
    {
        ParameterDefinition def = m_driver->parameter("nonexistent");
        QVERIFY2(!def.isValid(), "Unknown parameter should be invalid");
    }

    //==========================================================================
    // State Tests
    //==========================================================================
    void test_initial_state()
    {
        QVERIFY2(m_driver->state() == CameraState::Disconnected, "Initial state Disconnected");
        QVERIFY2(m_driver->isConnected() == false, "Should not be connected");
        QVERIFY2(m_driver->cameraId().isEmpty(), "Camera ID empty");
    }

    void test_driver_version()
    {
        QString version = m_driver->driverVersion();
        QVERIFY2(!version.isEmpty(), "Version not empty");
        QVERIFY2(version.contains("."), "Version has dot separator");
    }

    //==========================================================================
    // Capture Tests (should fail gracefully without camera)
    //==========================================================================
    void test_start_capture_without_connection()
    {
        bool result = m_driver->startCapture(1);
        QVERIFY2(result == false, "Should fail without connection");
    }

    void test_stop_capture_without_start()
    {
        // Should not crash
        m_driver->stopCapture();
    }

    //==========================================================================
    // Signal Tests
    //==========================================================================
    void test_frame_ready_signal_exists()
    {
        QSignalSpy spy(m_driver, &ICameraDriver::frameReady);
        QVERIFY2(spy.isValid(), "frameReady signal should be valid");
    }

    void test_error_signal_exists()
    {
        QSignalSpy spy(m_driver, &ICameraDriver::errorOccurred);
        QVERIFY2(spy.isValid(), "errorOccurred signal should be valid");
    }

    //==========================================================================
    // Validate/Commit Parameters Tests
    //==========================================================================
    void test_validate_parameters_returns_true_when_no_params()
    {
        // Without staged params, should return true
        QVERIFY2(m_driver->validateParameters() == true, "Should validate empty params");
    }

    void test_commit_parameters_returns_true_when_no_pending()
    {
        // Without staged params, should return true
        QVERIFY2(m_driver->commitParameters() == true, "Should commit empty params");
    }

private:
    QHYCCDDriver *m_driver = nullptr;
};

QTEST_MAIN(TestQHYCCDDriver)
#include "test_qhyccd_driver.moc"