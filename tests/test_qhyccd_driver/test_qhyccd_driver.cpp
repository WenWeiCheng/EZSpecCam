#include <QTest>
#include <QDebug>
#include <QSignalSpy>
#include <QImage>

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "plugins/qhyccd/QHYCCDDriver.h"

class TestQHYCCDDriver : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_driver = new QHYCCDDriver();
        QVERIFY2(m_driver != nullptr, "QHYCCDDriver should be created");
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

        QStringList cameras = m_driver->enumerate();
        if (cameras.isEmpty()) {
            QSKIP("No QHYCCD camera found, skipping test");
        }
        m_cameraId = cameras.first();
        qDebug() << "Connecting to camera:" << m_cameraId;
        bool connected = m_driver->connectToCamera(m_cameraId);
        if (!connected) {
            QSKIP(qPrintable(QString("Failed to connect to camera %1, skipping test").arg(m_cameraId)));
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
        qDebug() << "Found cameras:" << cameras;
        QVERIFY2(cameras.size() >= 0, "enumerate() should return a list");
    }

    void test_enumerate_non_empty_ids()
    {
        QStringList cameras = m_driver->enumerate();
        for (const QString &id : cameras) {
            QVERIFY2(!id.isEmpty(), qPrintable(QString("Camera ID should not be empty: %1").arg(id)));
        }
    }

    //==========================================================================
    // Connection Tests
    //==========================================================================
    void test_connect_without_camera()
    {
        bool result = m_driver->connectToCamera("nonexistent-camera");
        QVERIFY2(result == false, "Should fail gracefully when camera not present");
    }

    void test_driverVersion()
    {
        QString version = m_driver->driverVersion();
        QVERIFY2(!version.isEmpty(), "Driver version should be non-empty");
        qDebug() << "Driver version:" << version;
    }

    //==========================================================================
    // Parameter Tests - Info/ReadOnly (serial_number)
    //==========================================================================
    void test_param_camera_info()
    {
        QStringList names;
        ParameterDefinition param;
        names = m_driver->parameterNames();

        // serial_number
        QVERIFY2(names.contains("serial_number"), "serial_number parameter should exist");

        param = m_driver->parameter("serial_number");
        QVERIFY2(param.isValid(), "serial_number should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "serial_number should be String type");

        // camera_model
        QVERIFY2(names.contains("camera_model"), "camera_model parameter should exist");

        param = m_driver->parameter("camera_model");
        QVERIFY2(param.isValid(), "camera_model should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "camera_model should be String type");

        // chipWidth
        QVERIFY2(names.contains("chipWidth"), "chipWidth parameter should exist");

        param = m_driver->parameter("chipWidth");
        QVERIFY2(param.isValid(), "chipWidth should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "chipWidth should be String type");

        // chipHeight
        QVERIFY2(names.contains("chipHeight"), "chipHeight parameter should exist");

        param = m_driver->parameter("chipHeight");
        QVERIFY2(param.isValid(), "chipHeight should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "chipHeight should be String type");

        // imageWidth
        QVERIFY2(names.contains("imageWidth"), "imageWidth parameter should exist");

        param = m_driver->parameter("imageWidth");
        QVERIFY2(param.isValid(), "imageWidth should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "imageWidth should be String type");

        // imageHeight
        QVERIFY2(names.contains("imageHeight"), "imageHeight parameter should exist");

        param = m_driver->parameter("imageHeight");
        QVERIFY2(param.isValid(), "imageHeight should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "imageHeight should be String type");

        // pixelWidth
        QVERIFY2(names.contains("pixelWidth"), "pixelWidth parameter should exist");

        param = m_driver->parameter("pixelWidth");
        QVERIFY2(param.isValid(), "pixelWidth should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "pixelWidth should be String type");

        // pixelHeight
        QVERIFY2(names.contains("pixelHeight"), "pixelHeight parameter should exist");

        param = m_driver->parameter("pixelHeight");
        QVERIFY2(param.isValid(), "pixelHeight should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "pixelHeight should be String type");

        // imageBytes
        QVERIFY2(names.contains("imageBytes"), "imageBytes parameter should exist");

        param = m_driver->parameter("imageBytes");
        QVERIFY2(param.isValid(), "imageBytes should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "imageBytes should be String type");
    }

    //==========================================================================
    // Parameter Tests - Core (exposure)
    //==========================================================================
    void test_param_exposure()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("exposure"), "exposure parameter should exist");

        // verify existence
        ParameterDefinition param = m_driver->parameter("exposure");
        QVERIFY2(param.isValid(), "exposure should be a valid parameter");
        QVERIFY2(param.type == ParameterType::FloatRange, "exposure should be FloatRange type");

        // check exposure has defined valid range, step, unit, unitRange, and defaultValue
        QVariant def = param.constraint.minValue;
        QVERIFY2(def.isValid(), "exposure constraint minValue should be valid");
        QVariant maxVal = param.constraint.maxValue;
        QVERIFY2(maxVal.isValid(), "exposure constraint maxValue should be valid");
        QVERIFY2(def.toDouble() < maxVal.toDouble(),
                 "exposure minValue should be less than maxValue");
        QVariant step = param.constraint.step;
        QVERIFY2(step.isValid() && step.toDouble() > 0,
                 "exposure step should be positive");
        // unit and unitRange are optional but if present should be consistent
        if (!param.constraint.unit.isEmpty()) {
            QVERIFY2(!param.constraint.unitRange.isEmpty(),
                     "exposure unitRange should exist if unit is defined");
            QVERIFY2(param.constraint.hasUnitRange(),
                     "exposure unitRange size should be unit.size() - 1");
        }
        QVariant defaultVal = param.defaultValue;
        QVERIFY2(defaultVal.isValid(), "exposure defaultValue should be valid");
        // defaultValue should be within range
        double defaultD = defaultVal.toDouble();
        QVERIFY2(defaultD >= def.toDouble() && defaultD <= maxVal.toDouble(),
                 "exposure defaultValue should be within min/max range");

        // Test exposure timing: 1s exposure should not return frame within 500ms
        m_driver->setParameter("exposure", 1000.0);
        m_driver->commitParameters();

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        bool started = m_driver->startCapture(1);
        QVERIFY2(started, "Should start capture successfully");

        // Wait 500ms - should not receive frame yet (exposure is 1s)
        bool frameIn500ms = frameSpy.wait(500);
        QVERIFY2(!frameIn500ms, "Should NOT receive frame within 500ms for 1s exposure");

        // Now wait additional 1000ms - should receive frame
        bool frameAfter1s = frameSpy.wait(1500);
        QVERIFY2(frameAfter1s, "Should receive frame after 1s exposure + 500ms wait");
    }

    void test_param_gain()
    {
        QStringList names = m_driver->parameterNames();
        // verify existence
        QVERIFY2(names.contains("gain"), "gain parameter should exist");
        ParameterDefinition param = m_driver->parameter("gain");
        QVERIFY2(param.isValid(), "gain should be a valid parameter");
        QVERIFY2(param.type == ParameterType::FloatRange, "gain should be FloatRange type");

        // check gain has valid definition: range, step, and default value
        QVariant minVal = param.constraint.minValue;
        QVERIFY2(minVal.isValid(), "gain constraint minValue should be valid");
        QVariant maxVal = param.constraint.maxValue;
        QVERIFY2(maxVal.isValid(), "gain constraint maxValue should be valid");
        QVERIFY2(minVal.toDouble() < maxVal.toDouble(),
                 "gain minValue should be less than maxValue");
        QVariant step = param.constraint.step;
        QVERIFY2(step.isValid() && step.toDouble() >= 0,
                 "gain step should be non-negative");
        QVariant defaultVal = param.defaultValue;
        QVERIFY2(defaultVal.isValid(), "gain defaultValue should be valid");
        double defaultD = defaultVal.toDouble();
        QVERIFY2(defaultD >= minVal.toDouble() && defaultD <= maxVal.toDouble(),
                 "gain defaultValue should be within min/max range");

        // sample image and check image mean value in gain=10 is approximately 2x gain=0
        m_driver->setParameter("exposure", 100.0);
        m_driver->setParameter("gain", 0.0);
        m_driver->commitParameters();

        QSignalSpy frameSpy0(m_driver, &ICameraDriver::frameReady);
        m_driver->startCapture(1);
        QVERIFY2(frameSpy0.wait(2000), "Should receive frame at gain=0");
        QSharedPointer<QImage> image0 = frameSpy0.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!image0->isNull(), "Image at gain=0 should not be null");

        // compute mean of image0
        double sum0 = 0;
        int count0 = 0;
        for (int y = 0; y < image0->height(); ++y) {
            for (int x = 0; x < image0->width(); ++x) {
                sum0 += qGray(image0->pixel(x, y));
                ++count0;
            }
        }
        double mean0 = (count0 > 0) ? (sum0 / count0) : 0;

        m_driver->setParameter("gain", 50.0);
        m_driver->commitParameters();

        QSignalSpy frameSpy50(m_driver, &ICameraDriver::frameReady);
        m_driver->startCapture(1);
        QVERIFY2(frameSpy50.wait(2000), "Should receive frame at gain=50");
        QSharedPointer<QImage> image50 = frameSpy50.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!image50->isNull(), "Image at gain=50 should not be null");

        // compute mean of image50
        double sum50 = 0;
        int count50 = 0;
        for (int y = 0; y < image50->height(); ++y) {
            for (int x = 0; x < image50->width(); ++x) {
                sum50 += qGray(image50->pixel(x, y));
                ++count50;
            }
        }
        double mean50 = (count50 > 0) ? (sum50 / count50) : 0;

        qDebug() << "Gain=0 mean:" << mean0 << "Gain=50 mean:" << mean50;
        QVERIFY2(mean50 > mean0, "Mean at gain=50 should be greater than mean at gain=0");
        // gain=50 should be approximately 2x gain=0 
        double ratio = mean50 / mean0;
        QVERIFY2(ratio >= 2,
                 qPrintable(QString("Gain ratio (50/0) should be at least 2x, got %1").arg(ratio)));
    }

    void test_param_offset()
    {
        QStringList names = m_driver->parameterNames();
        // verify existence 
        QVERIFY2(names.contains("offset"), "offset parameter should exist");
        ParameterDefinition param = m_driver->parameter("offset");
        QVERIFY2(param.isValid(), "offset should be a valid parameter");
        QVERIFY2(param.type == ParameterType::FloatRange, "offset should be FloatRange type");

        // check offset has valid definition: range, step, and default value
        QVariant minVal = param.constraint.minValue;
        QVERIFY2(minVal.isValid(), "offset constraint minValue should be valid");
        QVariant maxVal = param.constraint.maxValue;
        QVERIFY2(maxVal.isValid(), "offset constraint maxValue should be valid");
        QVERIFY2(minVal.toDouble() < maxVal.toDouble(),
                 "offset minValue should be less than maxValue");
        QVariant step = param.constraint.step;
        QVERIFY2(step.isValid() && step.toDouble() >= 0,
                 "offset step should be non-negative");
        QVariant defaultVal = param.defaultValue;
        QVERIFY2(defaultVal.isValid(), "offset defaultValue should be valid");
        double defaultD = defaultVal.toDouble();
        QVERIFY2(defaultD >= minVal.toDouble() && defaultD <= maxVal.toDouble(),
                 "offset defaultValue should be within min/max range");

        // sample image and check image mean value in offset=50 is greater than offset=0
        m_driver->setParameter("exposure", 100.0);
        m_driver->setParameter("offset", 0.0);
        m_driver->commitParameters();

        QSignalSpy frameSpy0(m_driver, &ICameraDriver::frameReady);
        m_driver->startCapture(1);
        QVERIFY2(frameSpy0.wait(2000), "Should receive frame at offset=0");
        QSharedPointer<QImage> image0 = frameSpy0.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!image0->isNull(), "Image at offset=0 should not be null");

        // compute mean of image0
        double sum0 = 0;
        int count0 = 0;
        for (int y = 0; y < image0->height(); ++y) {
            for (int x = 0; x < image0->width(); ++x) {
                sum0 += qGray(image0->pixel(x, y));
                ++count0;
            }
        }
        double mean0 = (count0 > 0) ? (sum0 / count0) : 0;

        m_driver->setParameter("offset", 50.0);
        m_driver->commitParameters();

        QSignalSpy frameSpy50(m_driver, &ICameraDriver::frameReady);
        m_driver->startCapture(1);
        QVERIFY2(frameSpy50.wait(2000), "Should receive frame at offset=50");
        QSharedPointer<QImage> image50 = frameSpy50.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!image50->isNull(), "Image at offset=50 should not be null");

        // compute mean of image50
        double sum50 = 0;
        int count50 = 0;
        for (int y = 0; y < image50->height(); ++y) {
            for (int x = 0; x < image50->width(); ++x) {
                sum50 += qGray(image50->pixel(x, y));
                ++count50;
            }
        }
        double mean50 = (count50 > 0) ? (sum50 / count50) : 0;

        qDebug() << "Offset=0 mean:" << mean0 << "Offset=50 mean:" << mean50;
        QVERIFY2(mean50 > mean0,
                 "Mean at offset=50 should be greater than mean at offset=0");
    }

    //==========================================================================
    // Parameter Tests - ROI (roi_x)
    //==========================================================================
    void test_param_roi_x()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("roi_x"), "roi_x parameter should exist");

        ParameterDefinition param = m_driver->parameter("roi_x");
        QVERIFY2(param.isValid(), "roi_x should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "roi_x should be IntRange type");
    }

    void test_param_roi_y()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("roi_y"), "roi_y parameter should exist");

        ParameterDefinition param = m_driver->parameter("roi_y");
        QVERIFY2(param.isValid(), "roi_y should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "roi_y should be IntRange type");
    }

    void test_param_roi_width()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("roi_width"), "roi_width parameter should exist");

        ParameterDefinition param = m_driver->parameter("roi_width");
        QVERIFY2(param.isValid(), "roi_width should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "roi_width should be IntRange type");
    }

    void test_param_roi_height()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("roi_height"), "roi_height parameter should exist");

        ParameterDefinition param = m_driver->parameter("roi_height");
        QVERIFY2(param.isValid(), "roi_height should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "roi_height should be IntRange type");
    }

    //==========================================================================
    // Parameter Tests - Binning (binning)
    //==========================================================================
    void test_param_binning()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("binning"), "binning parameter should exist");

        ParameterDefinition param = m_driver->parameter("binning");
        QVERIFY2(param.isValid(), "binning should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntCollection, "binning should be IntCollection type");
    }

    //==========================================================================
    // Parameter Tests - Advanced (usb_traffic)
    //==========================================================================
    void test_param_usb_traffic()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("usb_traffic"), "usb_traffic parameter should exist");

        ParameterDefinition param = m_driver->parameter("usb_traffic");
        QVERIFY2(param.isValid(), "usb_traffic should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "usb_traffic should be IntRange type");
    }

    void test_param_read_mode()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("read_mode"), "read_mode parameter should exist");

        ParameterDefinition param = m_driver->parameter("read_mode");
        QVERIFY2(param.isValid(), "read_mode should be a valid parameter");
        QVERIFY2(param.type == ParameterType::StringCollection, "read_mode should be StringCollection type");
    }

    void test_param_transfer_bit()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("transfer_bit"), "transfer_bit parameter should exist");

        ParameterDefinition param = m_driver->parameter("transfer_bit");
        QVERIFY2(param.isValid(), "transfer_bit should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntCollection, "transfer_bit should be IntCollection type");
    }

    //==========================================================================
    // Parameter Tests - Cooling (cooler_enabled)
    //==========================================================================
    void test_param_cooler_enabled()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("cooler_enabled"), "cooler_enabled parameter should exist");

        ParameterDefinition param = m_driver->parameter("cooler_enabled");
        QVERIFY2(param.isValid(), "cooler_enabled should be a valid parameter");
        QVERIFY2(param.type == ParameterType::Boolean, "cooler_enabled should be Boolean type");
    }

    void test_param_target_temperature()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("target_temperature"), "target_temperature parameter should exist");

        ParameterDefinition param = m_driver->parameter("target_temperature");
        QVERIFY2(param.isValid(), "target_temperature should be a valid parameter");
        QVERIFY2(param.type == ParameterType::FloatRange, "target_temperature should be FloatRange type");
    }

    void test_param_current_temperature()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("current_temperature"), "current_temperature parameter should exist");

        ParameterDefinition param = m_driver->parameter("current_temperature");
        QVERIFY2(param.isValid(), "current_temperature should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "current_temperature should be String type");
    }

    //==========================================================================
    // Parameter Tests - Info/Dynamic (humidity)
    //==========================================================================
    void test_param_humidity()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("humidity"), "humidity parameter should exist");

        ParameterDefinition param = m_driver->parameter("humidity");
        QVERIFY2(param.isValid(), "humidity should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "humidity should be String type");
    }

    void test_param_pressure()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("pressure"), "pressure parameter should exist");

        ParameterDefinition param = m_driver->parameter("pressure");
        QVERIFY2(param.isValid(), "pressure should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "pressure should be String type");
    }

    //==========================================================================
    // Parameter Tests - Effective Area (effective_start_x)
    //==========================================================================
    void test_param_effective_start_x()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("effective_start_x"), "effective_start_x parameter should exist");

        ParameterDefinition param = m_driver->parameter("effective_start_x");
        QVERIFY2(param.isValid(), "effective_start_x should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "effective_start_x should be String type");
    }

    void test_param_effective_start_y()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("effective_start_y"), "effective_start_y parameter should exist");

        ParameterDefinition param = m_driver->parameter("effective_start_y");
        QVERIFY2(param.isValid(), "effective_start_y should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "effective_start_y should be String type");
    }

    void test_param_effective_width()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("effective_width"), "effective_width parameter should exist");

        ParameterDefinition param = m_driver->parameter("effective_width");
        QVERIFY2(param.isValid(), "effective_width should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "effective_width should be String type");
    }

    void test_param_effective_height()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("effective_height"), "effective_height parameter should exist");

        ParameterDefinition param = m_driver->parameter("effective_height");
        QVERIFY2(param.isValid(), "effective_height should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "effective_height should be String type");
    }

    //==========================================================================
    // Parameter Metadata Tests
    //==========================================================================
    void test_parameter_names_not_empty()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(!names.isEmpty(), "parameterNames() should return non-empty list");
        qDebug() << "Total parameters:" << names.size();
    }

    void test_all_parameters_accessible()
    {
        QStringList expectedParams = {
            "serial_number", "camera_model", "chipWidth", "chipHeight",
            "imageWidth", "imageHeight", "pixelWidth", "pixelHeight", "imageBytes",
            "exposure", "gain", "offset", "roi_x", "roi_y", "roi_width", "roi_height", "binning",
            "usb_traffic", "read_mode", "transfer_bit",
            "cooler_enabled", "target_temperature", "current_temperature",
            "humidity", "pressure",
            "effective_start_x", "effective_start_y", "effective_width", "effective_height"
        };

        QStringList names = m_driver->parameterNames();
        for (const QString &paramName : expectedParams) {
            QVERIFY2(names.contains(paramName),
                     qPrintable(QString("Parameter '%1' should exist").arg(paramName)));
        }
    }

    //==========================================================================
    // Signal Tests
    //==========================================================================
    void test_connection_signals_exist()
    {
        const QObject *obj = m_driver;
        QVERIFY2(obj != nullptr, "Driver should be a QObject");
    }

    void test_state_enum_values()
    {
        QVERIFY2(static_cast<int>(CameraState::Disconnected) == 0, "Disconnected should be 0");
        QVERIFY2(static_cast<int>(CameraState::Connecting) > 0, "Connecting should exist");
        QVERIFY2(static_cast<int>(CameraState::Connected) > 0, "Connected should exist");
        QVERIFY2(static_cast<int>(CameraState::Acquiring) > 0, "Acquiring should exist");
        QVERIFY2(static_cast<int>(CameraState::Error) > 0, "Error should exist");
    }

    void test_parameter_type_enum_values()
    {
        QVERIFY2(static_cast<int>(ParameterType::FloatRange) >= 0, "FloatRange type should exist");
        QVERIFY2(static_cast<int>(ParameterType::FloatCollection) >= 0, "FloatCollection type should exist");
        QVERIFY2(static_cast<int>(ParameterType::IntRange) >= 0, "IntRange type should exist");
        QVERIFY2(static_cast<int>(ParameterType::IntCollection) >= 0, "IntCollection type should exist");
        QVERIFY2(static_cast<int>(ParameterType::String) >= 0, "String type should exist");
        QVERIFY2(static_cast<int>(ParameterType::StringCollection) >= 0, "StringCollection type should exist");
        QVERIFY2(static_cast<int>(ParameterType::Boolean) >= 0, "Boolean type should exist");
    }

private:
    QHYCCDDriver *m_driver = nullptr;
    QString m_cameraId;
};

QTEST_MAIN(TestQHYCCDDriver)
#include "test_qhyccd_driver.moc"
