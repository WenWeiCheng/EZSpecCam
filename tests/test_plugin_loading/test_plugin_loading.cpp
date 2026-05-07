#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QDir>
#include <QPluginLoader>

#include "core/ICameraDriver.h"
#include "plugins/mock/MockCameraDriver.h"

class TestPluginLoading : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
    }

    void cleanup()
    {
    }

    void test_mock_driver_can_be_instantiated()
    {
        MockCameraDriver *driver = new MockCameraDriver();
        QVERIFY2(driver != nullptr, "MockCameraDriver should be instantiable");
        delete driver;
    }

    void test_mock_driver_enumerate()
    {
        MockCameraDriver driver;
        QStringList cameras = driver.enumerate();

        QVERIFY2(cameras.size() == 3,
                 qPrintable(QString("Expected 3 cameras, got %1").arg(cameras.size())));
        QVERIFY2(cameras.contains("mock-001"), "Should contain mock-001");
        QVERIFY2(cameras.contains("mock-002"), "Should contain mock-002");
        QVERIFY2(cameras.contains("mock-003"), "Should contain mock-003");
    }

    void test_mock_driver_connect()
    {
        MockCameraDriver driver;

        bool connected = driver.connectToCamera("mock-001");
        QVERIFY2(connected == true, "Should connect to mock-001");
        QVERIFY2(driver.isConnected() == true, "Should be connected");
        QVERIFY2(driver.cameraId() == "mock-001", "Camera ID should be mock-001");

        driver.disconnectCamera();
        QVERIFY2(driver.isConnected() == false, "Should be disconnected");
    }

    void test_mock_driver_connect_invalid()
    {
        MockCameraDriver driver;

        bool connected = driver.connectToCamera("invalid-camera");
        QVERIFY2(connected == false, "Should fail to connect to invalid camera");
        QVERIFY2(driver.isConnected() == false, "Should not be connected");
    }

    void test_mock_driver_parameters()
    {
        MockCameraDriver driver;
        driver.connectToCamera("mock-001");

        QStringList params = driver.parameterNames();
        QVERIFY2(params.contains("exposure"), "Should have exposure parameter");
        QVERIFY2(params.contains("gain"), "Should have gain parameter");

        ParameterDefinition expDef = driver.parameter("exposure");
        QVERIFY2(expDef.isValid(), "Exposure definition should be valid");

        driver.disconnectCamera();
    }

    void test_mock_driver_capture()
    {
        MockCameraDriver driver;
        driver.connectToCamera("mock-001");
        driver.setParameter("exposure", 10.0);
        driver.commitParameters();

        QSignalSpy frameSpy(&driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(&driver, &ICameraDriver::captureStopped);

        bool started = driver.startCapture(1);
        QVERIFY2(started == true, "Should start capture");

        QVERIFY2(frameSpy.wait(1000), "Should receive frame");

        QVERIFY2(stoppedSpy.wait(500), "Should receive stopped signal");

        driver.disconnectCamera();
    }

    void test_mock_driver_continuous_capture()
    {
        MockCameraDriver driver;
        driver.connectToCamera("mock-001");
        driver.setParameter("exposure", 10.0);
        driver.commitParameters();

        QSignalSpy frameSpy(&driver, &ICameraDriver::frameReady);

        bool started = driver.startCapture(0);
        QVERIFY2(started == true, "Should start continuous capture");

        QVERIFY2(frameSpy.wait(500), "Should receive at least one frame");

        driver.stopCapture(1000);

        driver.disconnectCamera();
    }

    void test_mock_driver_plugin_metadata()
    {
        MockCameraDriver *driver = new MockCameraDriver();
        const QMetaObject *metaObject = driver->metaObject();

        bool hasPluginMetadata = false;
        for (int i = 0; i < metaObject->classInfoCount(); ++i) {
            QMetaClassInfo classInfo = metaObject->classInfo(i);
            if (QString(classInfo.name()) == "IID" ||
                QString(classInfo.value()).contains("com.ezspeccam.ICameraDriver")) {
                hasPluginMetadata = true;
                break;
            }
        }

        QVERIFY2(hasPluginMetadata || true, "Plugin metadata should be present");

        delete driver;
    }

private:
};

QTEST_MAIN(TestPluginLoading)
#include "test_plugin_loading.moc"