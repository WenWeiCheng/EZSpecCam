#include <QTest>
#include <QSignalSpy>
#include <QPluginLoader>
#include <QImage>

#include "core/ICameraDriver.h"

class TestPluginLoading : public QObject
{
    Q_OBJECT

private:
    QPluginLoader *m_loader = nullptr;

    ICameraDriver *loadPlugin()
    {
        QObject *instance = m_loader->instance();
        if (!instance) {
            return nullptr;
        }
        return dynamic_cast<ICameraDriver*>(instance);
    }

private slots:
    void init()
    {
        m_loader = new QPluginLoader(MOCK_PLUGIN_PATH);
    }

    void cleanup()
    {
        if (m_loader) {
            if (m_loader->isLoaded()) {
                m_loader->unload();
            }
            delete m_loader;
            m_loader = nullptr;
        }
    }

    void test_plugin_load_valid()
    {
        bool loaded = m_loader->load();
        QVERIFY2(loaded, qPrintable(QString("load() failed: %1").arg(m_loader->errorString())));

        QObject *instance = m_loader->instance();
        QVERIFY2(instance != nullptr, "Plugin instance should be non-null after loading");

        ICameraDriver *driver = loadPlugin();
        QVERIFY2(driver != nullptr, "Instance should be castable to ICameraDriver");
    }

    void test_plugin_metadata()
    {
        QVERIFY2(m_loader->load(), qPrintable(QString("load() failed: %1").arg(m_loader->errorString())));

        QJsonObject metaData = m_loader->metaData();
        QString iid = metaData.value("IID").toString();
        QVERIFY2(iid == "com.ezspeccam.ICameraDriver",
                 qPrintable(QString("IID should be 'com.ezspeccam.ICameraDriver', got '%1'").arg(iid)));
    }

    void test_plugin_enumerate()
    {
        QVERIFY2(m_loader->load(), qPrintable(QString("load() failed: %1").arg(m_loader->errorString())));

        ICameraDriver *driver = loadPlugin();
        QVERIFY2(driver != nullptr, "Driver should be available");

        QStringList cameras = driver->enumerate();
        QVERIFY2(cameras.size() == 3,
                 qPrintable(QString("Expected 3 cameras, got %1").arg(cameras.size())));
        QVERIFY2(cameras.contains("mock-001"), "Should contain mock-001");
        QVERIFY2(cameras.contains("mock-002"), "Should contain mock-002");
        QVERIFY2(cameras.contains("mock-003"), "Should contain mock-003");
    }

    void test_plugin_connect()
    {
        QVERIFY2(m_loader->load(), qPrintable(QString("load() failed: %1").arg(m_loader->errorString())));

        ICameraDriver *driver = loadPlugin();
        QVERIFY2(driver != nullptr, "Driver should be available");

        QVERIFY2(driver->connectToCamera("mock-001"), "Should connect to mock-001");
        QVERIFY2(driver->isConnected(), "Should be connected");
        QVERIFY2(driver->cameraId() == "mock-001", "Camera ID should be mock-001");
        QVERIFY2(driver->state() == CameraState::Connected, "State should be Connected");
    }

    void test_plugin_disconnect()
    {
        QVERIFY2(m_loader->load(), qPrintable(QString("load() failed: %1").arg(m_loader->errorString())));

        ICameraDriver *driver = loadPlugin();
        QVERIFY2(driver != nullptr, "Driver should be available");
        QVERIFY2(driver->connectToCamera("mock-001"), "Should connect to mock-001");

        driver->disconnectCamera();
        QVERIFY2(!driver->isConnected(), "Should be disconnected");
        QVERIFY2(driver->state() == CameraState::Disconnected, "State should be Disconnected");
        QVERIFY2(driver->cameraId() == "", "Camera ID should be empty after disconnect");
    }

    void test_plugin_capture_single()
    {
        QVERIFY2(m_loader->load(), qPrintable(QString("load() failed: %1").arg(m_loader->errorString())));

        ICameraDriver *driver = loadPlugin();
        QVERIFY2(driver != nullptr, "Driver should be available");

        driver->connectToCamera("mock-001");
        driver->setParameter("exposure", 10.0);
        driver->commitParameters();

        QSignalSpy frameSpy(driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(driver, &ICameraDriver::captureStopped);

        QVERIFY2(driver->startCapture(1), "Should start capture");
        QVERIFY2(frameSpy.wait(2000), "Should receive frame within 2 seconds");
        QVERIFY2(stoppedSpy.wait(1000), "Should receive captureStopped signal");

        QList<QVariant> frameArgs = frameSpy.takeFirst();
        QSharedPointer<QImage> image = frameArgs.at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!image.isNull(), "Frame image should be non-null");
        QVERIFY2(!image->isNull(), "QImage should be valid");
    }

    void test_plugin_capture_continuous()
    {
        QVERIFY2(m_loader->load(), qPrintable(QString("load() failed: %1").arg(m_loader->errorString())));

        ICameraDriver *driver = loadPlugin();
        QVERIFY2(driver != nullptr, "Driver should be available");

        driver->connectToCamera("mock-001");
        driver->setParameter("exposure", 10.0);
        driver->commitParameters();

        QSignalSpy frameSpy(driver, &ICameraDriver::frameReady);
        QSignalSpy stoppedSpy(driver, &ICameraDriver::captureStopped);

        QVERIFY2(driver->startCapture(0), "Should start continuous capture");
        QVERIFY2(frameSpy.wait(1000), "Should receive at least one frame in continuous mode");

        driver->stopCapture(1000);

        QVERIFY2(stoppedSpy.count() > 0 || stoppedSpy.wait(500),
                 "Should receive captureStopped signal after stopCapture");
    }

    void test_plugin_unload()
    {
        QVERIFY2(m_loader->load(), qPrintable(QString("load() failed: %1").arg(m_loader->errorString())));

        ICameraDriver *driver = loadPlugin();
        QVERIFY2(driver != nullptr, "Driver should be available");

        QVERIFY2(m_loader->unload(), "Plugin should unload successfully");
        QVERIFY2(!m_loader->isLoaded(), "Plugin should no longer be loaded after unload");
    }

    void test_plugin_load_nonexistent()
    {
        QPluginLoader fakeLoader("C:/nonexistent/path/fake_plugin.dll");
        QVERIFY2(!fakeLoader.load(), "Loading nonexistent plugin should fail");
        QVERIFY2(!fakeLoader.errorString().isEmpty(), "Error string should be non-empty for failed load");
    }
};

QTEST_GUILESS_MAIN(TestPluginLoading)
#include "test_plugin_loading.moc"
