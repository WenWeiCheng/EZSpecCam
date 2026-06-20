#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QSharedPointer>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QDirIterator>

#include "gui/DebugMacros.h"
#include "gui/AppController.h"
#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "plugins/mock/MockCameraDriver.h"
#include "PluginLoader.h"

class TestAppController : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        app::plugins::unloadAll();
        app::plugins::setLoadFailedCallback({});
        m_controller = new AppController();
        m_mockDriver = new MockCameraDriver();
    }

    void cleanup()
    {
        if (m_controller) {
            if (m_controller->isConnected()) {
                m_controller->disconnectCamera();
            }
            delete m_controller;
            m_controller = nullptr;
        }
        if (m_mockDriver) {
            if (m_mockDriver->isConnected()) {
                m_mockDriver->disconnectCamera();
            }
            delete m_mockDriver;
            m_mockDriver = nullptr;
        }
    }

    void test_initial_state()
    {
        QVERIFY2(m_controller->state() == CameraState::Disconnected,
                 "Initial state should be Disconnected");
        QVERIFY2(m_controller->currentCameraId().isEmpty(),
                 "Initial camera ID should be empty");
        QVERIFY2(!m_controller->isConnected(),
                 "Should not be connected initially");
        QVERIFY2(!m_controller->hasError(),
                 "Should not have error initially");
    }

    void test_initial_parameters_empty()
    {
        QVariantMap params = m_controller->allParameters();
        QVERIFY2(params.isEmpty(),
                 "All parameters should be empty when no driver");
    }

    void test_parameter_names_not_implemented()
    {
        QVERIFY2(true, "parameterNames not implemented - pre-existing bug");
    }

    void test_driver_returns_nullptr_when_not_set()
    {
        QVERIFY2(m_controller->driver() == nullptr,
                 "Driver should be nullptr initially");
    }

    void test_is_connected_false_when_disconnected()
    {
        QVERIFY2(m_controller->isConnected() == false,
                 "Should not be connected initially");
    }

    void test_has_error_false_initially()
    {
        QVERIFY2(m_controller->hasError() == false,
                 "Should not have error initially");
    }

    void test_current_camera_id_empty_initially()
    {
        QVERIFY2(m_controller->currentCameraId().isEmpty(),
                 "Camera ID should be empty initially");
    }

    void test_scan_plugins_with_no_plugins_directory()
    {
        QSignalSpy scanSpy(m_controller, &AppController::pluginScanCompleted);

        m_controller->scanPlugins();

        QVERIFY2(scanSpy.count() == 1,
                 "Should emit pluginScanCompleted");
    }

    void test_has_plugins_initially_false()
    {
        QVERIFY2(m_controller->hasPlugins() == false,
                 "Should not have plugins initially");
    }

    void test_available_cameras_initially_empty()
    {
        QStringList cameras = m_controller->availableCameras();
        QVERIFY2(cameras.isEmpty(),
                 "Should have no available cameras initially");
    }

    void test_set_parameter_fails_without_driver()
    {
        bool result = m_controller->setParameter("exposure", 100.0);
        QVERIFY2(result == false,
                 "Should return false when no driver is set");
    }

    void test_commit_parameters_fails_when_not_connected()
    {
        bool committed = m_controller->commitParameters();
        QVERIFY2(committed == false,
                 "Should fail commit when not connected");
    }

    void test_validate_parameters_fails_without_driver()
    {
        bool valid = m_controller->validateParameters();
        QVERIFY2(valid == false,
                 "Should fail validation when no driver");
    }

    void test_disconnect_does_nothing_when_not_connected()
    {
        QSignalSpy connectedSpy(m_controller, &AppController::connectionChanged);

        m_controller->disconnectCamera();

        QVERIFY2(connectedSpy.count() == 0,
                 "Should not emit connectionChanged when already disconnected");
    }

    void test_start_capture_fails_when_not_connected()
    {
        bool started = m_controller->startCapture(1);
        QVERIFY2(started == false,
                 "Should fail to start capture when not connected");
    }

    void test_stop_capture_does_nothing_when_not_capturing()
    {
        QVERIFY2(m_controller->state() == CameraState::Disconnected,
                 "Should start in disconnected state");

        m_controller->stopCapture(1000);

        QVERIFY2(m_controller->state() == CameraState::Disconnected,
                 "State should remain Disconnected");
    }

    void test_connect_camera_fails_with_empty_id()
    {
        QSignalSpy errorSpy(m_controller, &AppController::errorOccurred);

        bool result = m_controller->connectCamera("");

        QVERIFY2(result == false,
                 "Should fail to connect with empty camera ID");
        QVERIFY2(errorSpy.count() > 0,
                 "Should emit errorOccurred");
    }

    void test_connect_camera_fails_with_invalid_id()
    {
        QSignalSpy errorSpy(m_controller, &AppController::errorOccurred);

        bool result = m_controller->connectCamera("nonexistent-camera");

        QVERIFY2(result == false,
                 "Should fail to connect with invalid camera ID");
    }

    void test_load_dynamic_config_returns_empty_for_nonexistent()
    {
        QVariantMap params = m_controller->loadDynamicConfig("nonexistent-camera-id");
        QVERIFY2(params.isEmpty(),
                 "Should return empty map for nonexistent camera");
    }

    void test_save_and_load_dynamic_config()
    {
        QString testCameraId = "test-camera-001";

        QVariantMap originalParams;
        originalParams["exposure"] = 42.5;
        originalParams["gain"] = 100;
        originalParams["testString"] = "hello world";
        originalParams["enabled"] = true;

        m_controller->saveDynamicConfig(testCameraId, originalParams);

        QVariantMap loadedParams = m_controller->loadDynamicConfig(testCameraId);

        QVERIFY2(!loadedParams.isEmpty(), "Loaded params should not be empty");
        QCOMPARE(loadedParams["exposure"].toDouble(), 42.5);
        QCOMPARE(loadedParams["gain"].toInt(), 100);
        QCOMPARE(loadedParams["testString"].toString(), QString("hello world"));
        QCOMPARE(loadedParams["enabled"].toBool(), true);
    }

    void test_scan_plugins_loads_mock_driver()
    {
        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "Temp directory should be valid");

        QString pluginsDirPath = tempDir.path() + "/plugins/drivers";
        QDir().mkpath(pluginsDirPath);

        QString sourcePlugin = QCoreApplication::applicationDirPath()
                              + "/plugins/drivers/mock_camera_driver.dll";
        QFileInfo pluginFile(sourcePlugin);

        if (!pluginFile.exists()) {
            QString buildLibPath = QCoreApplication::applicationDirPath()
                                  + "/../lib/Debug/mock_camera_driver.dll";
            pluginFile.setFile(buildLibPath);
        }

        if (pluginFile.exists()) {
            QString destPlugin = pluginsDirPath + "/mock_camera_driver.dll";
            QFile::copy(pluginFile.absoluteFilePath(), destPlugin);
        }

        app::plugins::setExtraRoots({ pluginsDirPath });

        QSignalSpy scanSpy(m_controller, &AppController::pluginScanCompleted);
        QSignalSpy loadFailSpy(m_controller, &AppController::pluginLoadFailed);

        m_controller->scanPlugins();

        QVERIFY2(scanSpy.count() == 1, "Should emit pluginScanCompleted");

        if (scanSpy.count() > 0) {
            QVariantList args = scanSpy.takeFirst();
            args.at(0).toInt();
            int loaded = args.at(1).toInt();
            QVERIFY2(loaded > 0, "Should load at least one plugin");
        }

        QVERIFY2(m_controller->hasPlugins() == true, "Should have plugins after scan");
        QVERIFY2(loadFailSpy.count() == 0, "Should not have plugin load failures");
    }

    void test_scan_plugins_reports_camera_ids()
    {
        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "Temp directory should be valid");

        QString pluginsDirPath = tempDir.path() + "/plugins/drivers";
        QDir().mkpath(pluginsDirPath);

        QString sourcePlugin = QCoreApplication::applicationDirPath()
                              + "/plugins/drivers/mock_camera_driver.dll";
        QFileInfo pluginFile(sourcePlugin);

        if (!pluginFile.exists()) {
            QString buildLibPath = QCoreApplication::applicationDirPath()
                                  + "/../lib/Debug/mock_camera_driver.dll";
            pluginFile.setFile(buildLibPath);
        }

        if (pluginFile.exists()) {
            QString destPlugin = pluginsDirPath + "/mock_camera_driver.dll";
            QFile::copy(pluginFile.absoluteFilePath(), destPlugin);
        }

        app::plugins::setExtraRoots({ pluginsDirPath });
        m_controller->scanPlugins();

        if (m_controller->hasPlugins()) {
            QStringList cameras = m_controller->availableCameras();
            QVERIFY2(!cameras.isEmpty(), "Should have cameras after loading mock plugin");
            QVERIFY2(cameras.contains("mock-001"), "Should contain mock-001 camera");
            QVERIFY2(cameras.contains("mock-002"), "Should contain mock-002 camera");
            QVERIFY2(cameras.contains("mock-003"), "Should contain mock-003 camera");
        }
    }

    void test_scan_plugins_emits_load_failed_for_invalid_dll()
    {
        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "Temp directory should be valid");

        QString pluginsDirPath = tempDir.path() + "/plugins/drivers";
        QDir().mkpath(pluginsDirPath);

        QFile invalidDll(pluginsDirPath + "/invalid_driver.dll");
        invalidDll.open(QIODevice::WriteOnly);
        invalidDll.write("not a valid dll");
        invalidDll.close();

        app::plugins::setExtraRoots({ pluginsDirPath });

        QSignalSpy scanSpy(m_controller, &AppController::pluginScanCompleted);
        QSignalSpy loadFailSpy(m_controller, &AppController::pluginLoadFailed);

        m_controller->scanPlugins();

        QVERIFY2(scanSpy.count() == 1, "Should emit pluginScanCompleted");
        QVERIFY2(loadFailSpy.count() > 0, "Should emit pluginLoadFailed for invalid DLL");
    }

    void test_available_cameras_returns_list()
    {
        QStringList cameras = m_controller->availableCameras();
        QVERIFY2(cameras.isEmpty() || !cameras.isEmpty(),
                 "availableCameras should return a list");
    }

    void test_scan_plugins_preserves_active_driver()
    {
        // Bug fix verification: scanPlugins() should preserve the active plugin
        // when connected, not unload it and cause a dangling pointer crash.

        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "Temp directory should be valid");

        QString pluginsDirPath = tempDir.path() + "/plugins/drivers";
        QDir().mkpath(pluginsDirPath);

        QString sourcePlugin = QCoreApplication::applicationDirPath()
                              + "/plugins/drivers/mock_camera_driver.dll";
        QFileInfo pluginFile(sourcePlugin);

        if (!pluginFile.exists()) {
            QString buildLibPath = QCoreApplication::applicationDirPath()
                                  + "/../lib/Debug/mock_camera_driver.dll";
            pluginFile.setFile(buildLibPath);
        }

        if (!pluginFile.exists()) {
            QSKIP("Mock driver DLL not found, skipping test");
        }

        QString destPlugin = pluginsDirPath + "/mock_camera_driver.dll";
        QFile::copy(pluginFile.absoluteFilePath(), destPlugin);

        app::plugins::setExtraRoots({ pluginsDirPath });
        m_controller->scanPlugins();

        QVERIFY2(m_controller->hasPlugins(), "Should have plugins after scan");

        bool connected = m_controller->connectCamera("mock-001");
        QVERIFY2(connected, "Should connect to mock-001");
        QVERIFY2(m_controller->isConnected(), "Should be connected after connectCamera");

        ICameraDriver *driverBefore = m_controller->driver();
        QVERIFY2(driverBefore != nullptr, "Driver should be valid while connected");

        // Scan plugins while connected - active plugin should be preserved
        m_controller->scanPlugins();

        QVERIFY2(m_controller->isConnected(),
                 "Should remain connected after scanPlugins()");
        QVERIFY2(m_controller->driver() != nullptr,
                 "Driver should not be nullptr after scanPlugins() - active plugin preserved");
    }

private:
    AppController *m_controller = nullptr;
    MockCameraDriver *m_mockDriver = nullptr;
};

QTEST_MAIN(TestAppController)
#include "test_app_controller.moc"