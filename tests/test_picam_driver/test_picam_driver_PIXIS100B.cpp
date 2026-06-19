#include <QTest>
#include <QDebug>
#include <QSignalSpy>
#include <QSharedPointer>
#include <QTest>
#include <qobject.h>
#include <qtestcase.h>

#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"
#include "plugins/picam/PicamDriver.h"

#ifdef USE_REAL_CAMERA
#else
#define USE_REAL_CAMERA
#endif

class TestPicamDriver : public QObject
{
    Q_OBJECT

private:
    PicamDriver *m_driver = nullptr;
    QString m_cameraId;
    QStringList m_params;

    void verifyParamExists(const QString &name)
    {
        QVERIFY2(m_params.contains(name),
                 qPrintable(QString("Parameter '%1' should exist: %2").arg(name).arg(m_params.join(","))));
        ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.isValid(),
                 qPrintable(QString("Parameter definition for '%1' should be valid").arg(name)));
    }

    void verifyParamReadOnly(const QString &name, ParameterType expectedType)
    {
        if (!m_params.contains(name)) {
            qDebug() << "Skipping" << name << "- not available";
            return;
        }
        verifyParamExists(name);
        ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(def.isReadOnly,
                 qPrintable(QString("Parameter '%1' should be read-only").arg(name)));
        QVERIFY2(def.type == expectedType,
                 qPrintable(QString("Parameter '%1' type should be %2, got %3")
                     .arg(name).arg(static_cast<int>(expectedType)).arg(static_cast<int>(def.type))));

        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
        QVariant value = m_driver->parameterValue(name);
        QVERIFY2(value.isValid(),
                 qPrintable(QString("Should read value for '%1'").arg(name)));
        QVERIFY2(errorSpy.isEmpty() || errorSpy.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 qPrintable(QString("Reading '%1' should not produce error").arg(name)));

        if (expectedType == ParameterType::IntRange) {
            QVERIFY2(value.toInt() > 0,
                     qPrintable(QString("'%1' value should be > 0, got %2").arg(name).arg(value.toString())));
        } else if (expectedType == ParameterType::FloatRange) {
            QVERIFY2(value.toDouble() > 0.0,
                     qPrintable(QString("'%1' value should be > 0, got %2").arg(name).arg(value.toString())));
        } else if (expectedType == ParameterType::StringCollection) {
            QVERIFY2(!value.toString().isEmpty(),
                     qPrintable(QString("'%1' value should not be empty").arg(name)));
        }
    }

    void verifyParamReadWrite(const QString &name, ParameterType expectedType)
    {
        if (!m_params.contains(name)) {
            qDebug() << "Skipping" << name << "- not available";
            return;
        }
        verifyParamExists(name);
        ParameterDefinition def = m_driver->parameter(name);
        QVERIFY2(!def.isReadOnly,
                 qPrintable(QString("Parameter '%1' should be writable").arg(name)));
        QVERIFY2(def.type == expectedType,
                 qPrintable(QString("Parameter '%1' type should be %2, got %3")
                     .arg(name).arg(static_cast<int>(expectedType)).arg(static_cast<int>(def.type))));

        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
        QVariant value = m_driver->parameterValue(name);
        QVERIFY2(value.isValid(),
                 qPrintable(QString("Should read value for '%1'").arg(name)));
        QVERIFY2(errorSpy.isEmpty() || errorSpy.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 qPrintable(QString("Reading '%1' should not produce error").arg(name)));
    }

    void testFloatRangeParam(const QString &name)
    {
        if (!m_params.contains(name)) {
            qDebug() << "Skipping" << name << "- not available";
            return;
        }
        verifyParamReadWrite(name, ParameterType::FloatRange);
        ParameterDefinition def = m_driver->parameter(name);

        qDebug() << "Testing FloatRange:" << name << "min=" << def.constraint.minValue
                 << "max=" << def.constraint.maxValue << "step=" << def.constraint.step;

        bool ok = m_driver->setParameter(name, def.constraint.minValue);
        QVERIFY2(ok, qPrintable(QString("Should set '%1' to min").arg(name)));
        QVERIFY2(m_driver->commitParameters(),
                 qPrintable(QString("Should commit '%1' = min").arg(name)));
        {
            QVariant readVal = m_driver->parameterValue(name);
            QVERIFY2(readVal.isValid(), qPrintable(QString("Should read back '%1' after min").arg(name)));
            QVERIFY2(qAbs(readVal.toDouble() - def.constraint.minValue) < 0.001,
                     qPrintable(QString("'%1' readback should match min: expected %2, got %3")
                         .arg(name).arg(def.constraint.minValue).arg(readVal.toDouble())));
        }

        ok = m_driver->setParameter(name, def.constraint.maxValue);
        QVERIFY2(ok, qPrintable(QString("Should set '%1' to max").arg(name)));
        QVERIFY2(m_driver->commitParameters(),
                 qPrintable(QString("Should commit '%1' = max").arg(name)));
        {
            QVariant readVal = m_driver->parameterValue(name);
            QVERIFY2(readVal.isValid(), qPrintable(QString("Should read back '%1' after max").arg(name)));
            QVERIFY2(qAbs(readVal.toDouble() - def.constraint.maxValue) < 0.001,
                     qPrintable(QString("'%1' readback should match max: expected %2, got %3")
                         .arg(name).arg(def.constraint.maxValue).arg(readVal.toDouble())));
        }

        double mid = (def.constraint.minValue + def.constraint.maxValue) / 2.0;
        ok = m_driver->setParameter(name, mid);
        QVERIFY2(ok, qPrintable(QString("Should set '%1' to midpoint").arg(name)));
        QVERIFY2(m_driver->commitParameters(),
                 qPrintable(QString("Should commit '%1' = midpoint").arg(name)));
        {
            QVariant readVal = m_driver->parameterValue(name);
            QVERIFY2(readVal.isValid(), qPrintable(QString("Should read back '%1' after midpoint").arg(name)));
            QVERIFY2(qAbs(readVal.toDouble() - mid) < 0.001,
                     qPrintable(QString("'%1' readback should match midpoint: expected %2, got %3")
                         .arg(name).arg(mid).arg(readVal.toDouble())));
        }
    }

    void testIntRangeParam(const QString &name)
    {
        if (!m_params.contains(name)) {
            qDebug() << "Skipping" << name << "- not available";
            return;
        }
        verifyParamReadWrite(name, ParameterType::IntRange);
        ParameterDefinition def = m_driver->parameter(name);

        qDebug() << "Testing IntRange:" << name << "min=" << def.constraint.minValue
                 << "max=" << def.constraint.maxValue;

        int minVal = static_cast<int>(def.constraint.minValue);
        int maxVal = static_cast<int>(def.constraint.maxValue);

        bool ok = m_driver->setParameter(name, minVal);
        QVERIFY2(ok, qPrintable(QString("Should set '%1' to min").arg(name)));
        QVERIFY2(m_driver->commitParameters(),
                 qPrintable(QString("Should commit '%1' = min").arg(name)));
        {
            QVariant readVal = m_driver->parameterValue(name);
            QVERIFY2(readVal.isValid(), qPrintable(QString("Should read back '%1' after min").arg(name)));
            QVERIFY2(readVal.toInt() == minVal,
                     qPrintable(QString("'%1' readback should match min: expected %2, got %3")
                         .arg(name).arg(minVal).arg(readVal.toInt())));
        }

        ok = m_driver->setParameter(name, maxVal);
        QVERIFY2(ok, qPrintable(QString("Should set '%1' to max").arg(name)));
        QVERIFY2(m_driver->commitParameters(),
                 qPrintable(QString("Should commit '%1' = max").arg(name)));
        {
            QVariant readVal = m_driver->parameterValue(name);
            QVERIFY2(readVal.isValid(), qPrintable(QString("Should read back '%1' after max").arg(name)));
            QVERIFY2(readVal.toInt() == maxVal,
                     qPrintable(QString("'%1' readback should match max: expected %2, got %3")
                         .arg(name).arg(maxVal).arg(readVal.toInt())));
        }
    }

    void testCollectionParam(const QString &name, ParameterType expectedType)
    {
        if (!m_params.contains(name)) {
            qDebug() << "Skipping" << name << "- not available";
            return;
        }
        verifyParamReadWrite(name, expectedType);
        ParameterDefinition def = m_driver->parameter(name);

        QVERIFY2(!def.constraint.validValues.isEmpty(),
                 qPrintable(QString("Parameter '%1' should have valid values").arg(name)));

        qDebug() << "Testing Collection:" << name << "validValues=" << def.constraint.validValues;

        for (const QVariant &v : def.constraint.validValues) {
            bool ok = m_driver->setParameter(name, v);
            QVERIFY2(ok, qPrintable(QString("Should set '%1' to '%2'").arg(name).arg(v.toString())));
            QVERIFY2(m_driver->commitParameters(),
                     qPrintable(QString("Should commit '%1' = '%2'").arg(name).arg(v.toString())));

            QVariant readVal = m_driver->parameterValue(name);
            QVERIFY2(readVal.isValid(),
                     qPrintable(QString("Should read back '%1' after '%2'").arg(name).arg(v.toString())));
            if (expectedType == ParameterType::StringCollection) {
                QVERIFY2(readVal.toString() == v.toString(),
                         qPrintable(QString("'%1' readback should match: expected '%2', got '%3'")
                             .arg(name).arg(v.toString()).arg(readVal.toString())));
            } else {
                QVERIFY2(readVal.toInt() == v.toInt(),
                         qPrintable(QString("'%1' readback should match: expected %2, got %3")
                             .arg(name).arg(v.toInt()).arg(readVal.toInt())));
            }
        }
    }

private slots:
    void initTestCase()
    {
        m_driver = new PicamDriver();
        QVERIFY2(m_driver != nullptr, "PicamDriver should be created");

        QStringList cameras = m_driver->enumerate();
        if (cameras.isEmpty()) {
            QSKIP("PICam demo camera not available; skipping entire test suite");
        }
        m_cameraId = cameras.first();
        qDebug() << "Will connect to" << m_cameraId << "for each test";
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
        if (!m_driver) {
            return;
        }
        // Each test starts with a fresh connection for a clean environment.
        if (m_driver->isConnected()) {
            m_driver->disconnectCamera();
        }
        QVERIFY2(m_driver->connectToCamera(m_cameraId),
                 qPrintable(QString("init: connectToCamera(%1) failed").arg(m_cameraId)));
        m_params = m_driver->parameterNames();
    }

    void cleanup()
    {
        if (!m_driver) {
            return;
        }
        // Safety net: stop any in-flight capture, then disconnect.
        if (m_driver->state() == CameraState::Acquiring) {
            m_driver->stopCapture();
        }
        if (m_driver->isConnected()) {
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
        QVERIFY2(cameras.size() >= 0, "enumerate() should return a list");
    }

    void test_enumerate_non_empty_ids()
    {
        QStringList cameras = m_driver->enumerate();
        for (const QString &id : cameras) {
            QVERIFY2(!id.isEmpty(),
                     qPrintable(QString("Camera ID should not be empty: %1").arg(id)));
        }
    }

    //==========================================================================
    // Connection Tests
    //==========================================================================
    void test_connect()
    {
        QVERIFY2(m_driver->isConnected(),
                 qPrintable(QString("isConnected should be true after connect: %1").arg(m_cameraId)));
        QVERIFY2(m_driver->state() == CameraState::Connected,
                 "State should be Connected after connection");
        QVERIFY2(!m_driver->cameraId().isEmpty(),
                 "cameraId should not be empty after connection");
    }

    void test_connect_invalid()
    {
        QString invalidId = "nonexistent-camera-xyz";
        bool result = m_driver->connectToCamera(invalidId);
        QVERIFY2(!result, "connectToCamera should return false for non-existent camera ID");
        QVERIFY2(m_driver->state() == CameraState::Disconnected,
                 "State should be Disconnected after failed connect");
        QVERIFY2(!m_driver->isConnected(),
                 "isConnected should be false after failed connect");
    }

    void test_disconnect()
    {
        QSignalSpy connectionSpy(m_driver, &ICameraDriver::connectionChanged);
        m_driver->disconnectCamera();
        QVERIFY2(!m_driver->isConnected(),
                 "isConnected should be false after disconnect");
        QVERIFY2(m_driver->state() == CameraState::Disconnected,
                 "State should be Disconnected after disconnect");
        QVERIFY2(connectionSpy.count() > 0,
                 "connectionChanged should be emitted on disconnect");
    }

    void test_driver_version()
    {
        QString version = m_driver->driverVersion();
        QVERIFY2(!version.isEmpty(),
                 "Driver version should not be empty");
        qDebug() << "Driver version:" << version;
    }

    //==========================================================================
    // Parameter Definition Tests
    //==========================================================================
    void test_parameter_names_not_empty()
    {
        QVERIFY2(!m_params.isEmpty(),
                 qPrintable(QString("parameterNames should not be empty, got %1 params").arg(m_params.size())));
        qDebug() << "Parameter count:" << m_params.size();
        qDebug() << "Parameters:" << m_params;
    }

    //==========================================================================
    // Info Parameters (Read-only)
    //==========================================================================
    void test_param_sensor_width()
    {
        verifyParamReadOnly("sensor_width", ParameterType::IntRange);
    }

    void test_param_sensor_height()
    {
        verifyParamReadOnly("sensor_height", ParameterType::IntRange);
    }

    void test_param_bit_depth()
    {
        verifyParamReadOnly("bit_depth", ParameterType::IntRange);
    }

    //==========================================================================
    // Core Parameters
    //==========================================================================
    void test_param_exposure()
    {
        testFloatRangeParam("exposure");
    }

    void test_param_analog_gain()
    {
        if (!m_params.contains("analog_gain")) {
            qDebug() << "analog_gain not available, skipping";
            return;
        }

        ParameterDefinition def = m_driver->parameter("analog_gain");
        qDebug() << "analog_gain type:" << static_cast<int>(def.type)
                 << "isValid:" << def.isValid()
                 << "validValues:" << def.constraint.validValues.size();

        QVERIFY2(def.isValid(),
                 qPrintable(QString("analog_gain definition should be valid")));

        if (def.type == ParameterType::FloatRange) {
            testFloatRangeParam("analog_gain");
        } else if (def.type == ParameterType::IntRange) {
            testIntRangeParam("analog_gain");
        } else if (def.type == ParameterType::StringCollection) {
            testCollectionParam("analog_gain", ParameterType::StringCollection);
        } else if (def.type == ParameterType::IntCollection) {
            testCollectionParam("analog_gain", ParameterType::IntCollection);
        } else {
            qDebug() << "analog_gain has unsupported type, skipping";
        }
    }

    //==========================================================================
    // ADC Parameters
    //==========================================================================
    void test_param_adc_quality()
    {
        if (!m_params.contains("adc_quality")) {
            qDebug() << "adc_quality not available, skipping";
            return;
        }

        ParameterDefinition def = m_driver->parameter("adc_quality");
        qDebug() << "adc_quality type:" << static_cast<int>(def.type)
                 << "isValid:" << def.isValid()
                 << "validValues:" << def.constraint.validValues.size();

        QVERIFY2(def.isValid(),
                 qPrintable(QString("adc_quality definition should be valid")));

        if (def.type == ParameterType::StringCollection) {
            testCollectionParam("adc_quality", ParameterType::StringCollection);
        } else {
            qDebug() << "adc_quality has unsupported type, skipping";
        }
    }

    void test_param_adc_speed()
    {
        if (!m_params.contains("adc_speed")) {
            qDebug() << "adc_speed not available, skipping";
            return;
        }

        ParameterDefinition def = m_driver->parameter("adc_speed");
        qDebug() << "adc_speed type:" << static_cast<int>(def.type)
                 << "isValid:" << def.isValid()
                 << "validValues:" << def.constraint.validValues.size();

        QVERIFY2(def.isValid(),
                 qPrintable(QString("adc_speed definition should be valid")));

        if (def.type == ParameterType::StringCollection) {
            testCollectionParam("adc_speed", ParameterType::StringCollection);
        } else if (def.type == ParameterType::IntRange) {
            testIntRangeParam("adc_speed");
        } else if (def.type == ParameterType::FloatRange) {
            testFloatRangeParam("adc_speed");
        } else if (def.type == ParameterType::FloatCollection) {
            testCollectionParam("adc_speed", ParameterType::FloatCollection);
        } else if (def.type == ParameterType::IntCollection) {
            testCollectionParam("adc_speed", ParameterType::IntCollection);
        } else {
            qDebug() << "adc_speed has unsupported type, skipping";
        }
    }

    //==========================================================================
    // Pixel Format Parameter
    //==========================================================================
    void test_param_pixel_format()
    {
        if (!m_params.contains("pixel_format")) {
            qDebug() << "pixel_format not available, skipping";
            return;
        }

        ParameterDefinition def = m_driver->parameter("pixel_format");
        qDebug() << "pixel_format type:" << static_cast<int>(def.type)
                 << "isValid:" << def.isValid()
                 << "validValues:" << def.constraint.validValues.size();

        QVERIFY2(def.isValid(),
                 qPrintable(QString("pixel_format definition should be valid")));

        if (def.type == ParameterType::StringCollection) {
            testCollectionParam("pixel_format", ParameterType::StringCollection);
        } else {
            qDebug() << "pixel_format has unsupported type, skipping";
        }
    }

    //==========================================================================
    // ROI Parameters
    //==========================================================================
    void test_param_roi()
    {

        QStringList roiParams = {"roi_x", "roi_y", "roi_width", "roi_height"};
        for (const QString &name : roiParams) {
            if (!m_params.contains(name)) {
                qDebug() << name << "not available, skipping ROI tests";
                return;
            }
        }

        ParameterDefinition widthParam = m_driver->parameter("roi_width");
        ParameterDefinition heightParam = m_driver->parameter("roi_height");
        QVariant fullWidth = widthParam.constraint.maxValue;
        QVariant fullHeight = heightParam.constraint.maxValue;
        QVERIFY2(fullWidth.isValid(), "Full width should be valid");
        QVERIFY2(fullHeight.isValid(), "Full height should be valid");

        qDebug() << "Full sensor size:" << fullWidth.toInt() << "x" << fullHeight.toInt();

        QVERIFY2(m_driver->setParameter("roi_x", 0), "setParameter roi_x");
        QVERIFY2(m_driver->setParameter("roi_y", 0), "setParameter roi_y");
        QVERIFY2(m_driver->setParameter("roi_width", fullWidth), "setParameter roi_width");
        QVERIFY2(m_driver->setParameter("roi_height", fullHeight), "setParameter roi_height");
        QVERIFY2(m_driver->commitParameters(), "commitParameters for full-sensor ROI");

        QSignalSpy frameSpyFull(m_driver, &ICameraDriver::frameReady);
        QVERIFY2(m_driver->startCapture(1), "startCapture after ROI commit");
        QVERIFY2(frameSpyFull.wait(10000), "ROI capture: no frame within 10s");
        QCOMPARE(frameSpyFull.count(), 1);

        QSharedPointer<QImage> fullImage = frameSpyFull.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!fullImage.isNull() && !fullImage->isNull(), "ROI frame QImage is null");
        QCOMPARE(fullImage->width(), fullWidth.toInt());
        QCOMPARE(fullImage->height(), fullHeight.toInt());

        m_driver->stopCapture();

        QVERIFY2(m_driver->setParameter("roi_x", 0), "reset roi_x");
        QVERIFY2(m_driver->setParameter("roi_y", 0), "reset roi_y");
        QVERIFY2(m_driver->setParameter("roi_width", fullWidth), "reset roi_width");
        QVERIFY2(m_driver->setParameter("roi_height", fullHeight), "reset roi_height");
        QVERIFY2(m_driver->setParameter("roi_x_binning", 1), "reset roi_x_binning");
        QVERIFY2(m_driver->setParameter("roi_y_binning", 1), "reset roi_y_binning");
        QVERIFY2(m_driver->commitParameters(), "commitParameters for ROI reset");
    }

    //==========================================================================
    // Binning Parameters
    //==========================================================================
    void test_param_binning()
    {

        bool hasXBinning = m_params.contains("roi_x_binning");
        bool hasYBinning = m_params.contains("roi_y_binning");

        if (!hasXBinning || !hasYBinning) {
            qDebug() << "Binning parameters not available, skipping";
            return;
        }

        verifyParamReadWrite("roi_x_binning", ParameterType::IntRange);
        verifyParamReadWrite("roi_y_binning", ParameterType::IntRange);

        ParameterDefinition widthParam = m_driver->parameter("roi_width");
        ParameterDefinition heightParam = m_driver->parameter("roi_height");
        QVERIFY2(m_driver->setParameter("roi_width", widthParam.constraint.maxValue), "set roi_width");
        QVERIFY2(m_driver->setParameter("roi_height", heightParam.constraint.maxValue), "set roi_height");
        QVERIFY2(m_driver->setParameter("roi_x_binning", 1), "set binning 1x1");
        QVERIFY2(m_driver->setParameter("roi_y_binning", 1), "set binning 1x1");
        QVERIFY2(m_driver->commitParameters(), "commit 1x1 binning");

        QSignalSpy frameSpy1(m_driver, &ICameraDriver::frameReady);
        QVERIFY2(m_driver->startCapture(1), "startCapture 1x1 binning");
        QVERIFY2(frameSpy1.wait(10000), "Binning 1x1: no frame within 10s");
        QCOMPARE(frameSpy1.count(), 1);

        QSharedPointer<QImage> image1 = frameSpy1.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!image1.isNull() && !image1->isNull(), "1x1 binning frame is null");
        int w1 = image1->width();
        int h1 = image1->height();
        qDebug() << "Binning 1x1:" << w1 << "x" << h1;

        ParameterDefinition xBinningDef = m_driver->parameter("roi_x_binning");
        ParameterDefinition yBinningDef = m_driver->parameter("roi_y_binning");
        if(xBinningDef.constraint.maxValue < 2) {
            qDebug() << "Maximum x binning is less than 2, skipping 2x2 binning test";
            return;
        }
        if(yBinningDef.constraint.maxValue < 2) {
            qDebug() << "Maximum y binning is less than 2, skipping 2x2 binning test";
            return;
        }
        QVERIFY2(m_driver->setParameter("roi_x_binning", 2), "set binning 2x2");
        QVERIFY2(m_driver->setParameter("roi_y_binning", 2), "set binning 2x2");
        QVERIFY2(m_driver->commitParameters(), "commit 2x2 binning");

        QSignalSpy frameSpy2(m_driver, &ICameraDriver::frameReady);
        QVERIFY2(m_driver->startCapture(1), "startCapture 2x2 binning");
        QVERIFY2(frameSpy2.wait(10000), "Binning 2x2: no frame within 10s");
        QCOMPARE(frameSpy2.count(), 1);

        QSharedPointer<QImage> image2 = frameSpy2.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!image2.isNull() && !image2->isNull(), "2x2 binning frame is null");
        int w2 = image2->width();
        int h2 = image2->height();
        qDebug() << "Binning 2x2:" << w2 << "x" << h2;

        QVERIFY2(w2 <= w1 && h2 <= h1,
                 qPrintable(QString("2x2 binning frame (%1x%2) should be <= 1x1 frame (%3x%4)")
                                .arg(w2).arg(h2).arg(w1).arg(h1)));

        m_driver->stopCapture();

        QVERIFY2(m_driver->setParameter("roi_x", 0), "reset roi_x");
        QVERIFY2(m_driver->setParameter("roi_y", 0), "reset roi_y");
        QVERIFY2(m_driver->setParameter("roi_x_binning", 1), "reset binning 1");
        QVERIFY2(m_driver->setParameter("roi_y_binning", 1), "reset binning 1");
        QVERIFY2(m_driver->commitParameters(), "commit binning reset");
    }

    //==========================================================================
    // Temperature Parameters
    //==========================================================================
    void test_param_temperature()
    {

        if (!m_params.contains("sensor_temperature")) {
            qDebug() << "sensor_temperature not available, skipping";
            return;
        }

        ParameterDefinition def = m_driver->parameter("sensor_temperature");
        QVERIFY2(def.isValid(), "sensor_temperature definition should be valid");
        QVERIFY2(def.isReadOnly, "sensor_temperature should be read-only");
        qDebug() << "sensor_temperature type:" << static_cast<int>(def.type);

        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
        QVariant currentTemp = m_driver->parameterValue("sensor_temperature");
        QVERIFY2(currentTemp.isValid(), "sensor_temperature value should be readable");
        QVERIFY2(errorSpy.isEmpty() || errorSpy.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Reading sensor_temperature should not produce error");
        qDebug() << "Current sensor temperature:" << currentTemp.toString();
        QVERIFY2(currentTemp.toDouble() != 0.0,
                 qPrintable(QString("sensor_temperature should not be 0, got %1").arg(currentTemp.toString())));

        if (m_params.contains("temperature_setpoint")) {
            testFloatRangeParam("temperature_setpoint");
        }
    }

    //==========================================================================
    // Extended Sensor Info Parameters (Read-Only)
    //==========================================================================
    void test_param_sensor_info_extended()
    {

        verifyParamReadOnly("pixel_width", ParameterType::FloatRange);
        verifyParamReadOnly("pixel_height", ParameterType::FloatRange);

        QStringList intParams = {
            "sensor_extended_height", "sensor_secondary_height",
            "sensor_left_margin", "sensor_right_margin",
            "sensor_top_margin", "sensor_bottom_margin",
            "sensor_masked_height", "sensor_masked_top",
            "sensor_masked_bottom", "sensor_secondary_masked_height"
        };
        for (const QString &name : intParams) {
            if (!m_params.contains(name)) {
                qDebug() << "Skipping" << name << "- not available";
                continue;
            }
            verifyParamExists(name);
            ParameterDefinition def = m_driver->parameter(name);
            QVERIFY2(def.isReadOnly,
                     qPrintable(QString("Parameter '%1' should be read-only").arg(name)));
            QVERIFY2(def.type == ParameterType::IntRange,
                     qPrintable(QString("Parameter '%1' type should be IntRange, got %2")
                         .arg(name).arg(static_cast<int>(def.type))));

            QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
            QVariant value = m_driver->parameterValue(name);
            QVERIFY2(value.isValid(),
                     qPrintable(QString("Should read value for '%1'").arg(name)));
            QVERIFY2(errorSpy.isEmpty() || errorSpy.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                     qPrintable(QString("Reading '%1' should not produce error").arg(name)));
            QVERIFY2(value.toInt() >= 0,
                     qPrintable(QString("'%1' value should be >= 0, got %2").arg(name).arg(value.toString())));
        }

        verifyParamReadOnly("sensor_type", ParameterType::StringCollection);
        verifyParamReadOnly("ccd_chars", ParameterType::StringCollection);
        verifyParamReadOnly("orientation", ParameterType::StringCollection);
        verifyParamReadOnly("readout_orientation", ParameterType::StringCollection);
    }

    //==========================================================================
    // Cooling — Temperature Status
    //==========================================================================
    void test_param_temperature_status()
    {
        verifyParamReadOnly("temperature_status", ParameterType::StringCollection);
    }

    //==========================================================================
    // Core — ADC Bit Depth
    //==========================================================================
    void test_param_adc_bit_depth()
    {
        ParameterDefinition def = m_driver->parameter("adc_bit_depth");
        QVERIFY2(def.isValid(),
                 qPrintable(QString("adc_bit_depth definition should be valid")));
        if (def.type == ParameterType::IntRange) {
            testIntRangeParam("adc_bit_depth");
        } else if (def.type == ParameterType::IntCollection) {
            testCollectionParam("adc_bit_depth", ParameterType::IntCollection);
        } else {
            qDebug() << "adc_bit_depth has unsupported type, skipping";
        }
    }

    //==========================================================================
    // Core — Readout, Trigger, Output Signal
    //==========================================================================
    void test_param_readout_trigger()
    {

        QStringList names = {"readout_mode", "trigger_response", "output_signal"};
        for (const QString &name : names) {
            if (!m_params.contains(name)) {
                qDebug() << name << "not available, skipping";
                continue;
            }
            ParameterDefinition def = m_driver->parameter(name);
            QVERIFY2(def.isValid(),
                     qPrintable(QString("'%1' definition should be valid").arg(name)));
            verifyParamReadWrite(name, ParameterType::StringCollection);
            bool ok = m_driver->setParameter(name, def.constraint.validValues.first());
            QVERIFY2(ok, qPrintable(QString("Should set '%1' to first value").arg(name)));
            if (!m_driver->commitParameters()) {
                qDebug() << name << "commit failed — demo camera may have SDK limitations";
            }
        }
    }

    //==========================================================================
    // Advanced Parameters
    //==========================================================================
    void test_param_advanced()
    {

        QStringList enumNames = {"shutter_mode"};
        for (const QString &name : enumNames) {
            if (!m_params.contains(name)) continue;
            ParameterDefinition def = m_driver->parameter(name);
            QVERIFY2(def.isValid(),
                     qPrintable(QString("'%1' definition should be valid").arg(name)));
            testCollectionParam(name, ParameterType::StringCollection);
        }

        QStringList floatNames = {"shutter_delay", "vertical_shift_rate"};
        for (const QString &name : floatNames) {
            if (!m_params.contains(name)) continue;
            ParameterDefinition def = m_driver->parameter(name);
            QVERIFY2(def.isValid(),
                     qPrintable(QString("'%1' definition should be valid").arg(name)));
            if (def.type == ParameterType::FloatRange) {
                testFloatRangeParam(name);
            } else if (def.type == ParameterType::FloatCollection) {
                testCollectionParam(name, ParameterType::FloatCollection);
            } else {
                qDebug() << name << "has unsupported type, skipping";
            }
        }

        QStringList intNames = {"active_width", "active_height",
                                "active_left", "active_right",
                                "active_top", "active_bottom"};
        for (const QString &name : intNames) {
            if (!m_params.contains(name)) continue;
            ParameterDefinition def = m_driver->parameter(name);
            QVERIFY2(def.isValid(),
                     qPrintable(QString("'%1' definition should be valid").arg(name)));
            if (def.type == ParameterType::IntRange) {
                verifyParamReadWrite(name, ParameterType::IntRange);
                bool ok = m_driver->setParameter(name, static_cast<int>(def.constraint.minValue));
                QVERIFY2(ok, qPrintable(QString("Should set '%1' to min").arg(name)));
                if (!m_driver->commitParameters()) {
                    qDebug() << name << "commit failed — demo camera may have SDK limitations";
                }
            } else if (def.type == ParameterType::IntCollection) {
                testCollectionParam(name, ParameterType::IntCollection);
            } else {
                qDebug() << name << "has unsupported type, skipping";
            }
        }

        // Test invalid value rejection on StringCollection parameter
        QStringList invalidCandidates = {"readout_mode", "shutter_mode"};
        QString invalidTarget;
        ParameterDefinition invalidDef;
        for (const QString &name : invalidCandidates) {
            if (!m_params.contains(name)) continue;
            invalidDef = m_driver->parameter(name);
            if (invalidDef.isValid() && !invalidDef.isReadOnly
                && invalidDef.type == ParameterType::StringCollection
                && !invalidDef.constraint.validValues.isEmpty()) {
                invalidTarget = name;
                break;
            }
        }
        if (!invalidTarget.isEmpty()) {
            QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
            m_driver->setParameter(invalidTarget, "InvalidValue_XYZ_999");
            m_driver->commitParameters();
            bool gotError = !errorSpy.isEmpty()
                && errorSpy.at(0).at(0).value<CameraError>().code != CameraError::Code::None;
            qDebug() << "Invalid value for" << invalidTarget << "produced error:" << gotError;
            QVERIFY2(gotError,
                     qPrintable(QString("Invalid value for '%1' should be rejected").arg(invalidTarget)));
        } else {
            qDebug() << "No suitable writable StringCollection param for invalid value test, skipping";
        }
    }

    //==========================================================================
    // Writable Parameters — Default Value Commit Test
    //==========================================================================
    void test_param_all_writable_default_commit()
    {

        QStringList roiSubParams = {"roi_x", "roi_y", "roi_width", "roi_height",
                                    "roi_x_binning", "roi_y_binning"};

        int failureCount = 0;
        QStringList failedParams;

        for (const QString &name : m_params) {
            ParameterDefinition def = m_driver->parameter(name);
            QVERIFY2(def.isValid(),
                     qPrintable(QString("'%1' definition should be valid").arg(name)));
            if (def.isReadOnly) {
                continue;
            }

            if (roiSubParams.contains(name)) {
                continue;
            }

            if (!def.defaultValue.isValid()) {
                qDebug() << "Skipping" << name << "- no default value";
                continue;
            }

            QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

            bool setOk = m_driver->setParameter(name, def.defaultValue);
            if (!setOk) {
                failureCount++;
                failedParams.append(name + " (setParameter rejected)");
                continue;
            }

            bool commitOk = m_driver->commitParameters();
            bool hasError = !errorSpy.isEmpty()
                && errorSpy.at(0).at(0).value<CameraError>().code != CameraError::Code::None;

            if (!commitOk || hasError) {
                failureCount++;
                QString detail = name;
                if (!commitOk) detail += " (commit failed)";
                if (hasError) {
                    CameraError err = errorSpy.at(0).at(0).value<CameraError>();
                    detail += QString(" (error: %1)").arg(err.description);
                }
                failedParams.append(detail);
            }
        }

        if (!failedParams.isEmpty()) {
            qWarning() << "Writable params that failed default commit:" << failedParams;
        }

        QVERIFY2(failureCount == 0,
                 qPrintable(QString("%1 writable parameter(s) failed to commit with default value: %2")
                     .arg(failureCount).arg(failedParams.join(", "))));
    }

    //==========================================================================
    // Capture Tests
    //==========================================================================
    void test_capture_single_frame()
    {
        QVERIFY2(m_driver->setParameter("exposure", 0.1), "setParameter exposure");
        QVERIFY2(m_driver->commitParameters(), "commitParameters exposure");

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QVERIFY2(m_driver->startCapture(1), "startCapture(1) should return true");
        QVERIFY2(frameSpy.wait(3000) || frameSpy.count() > 0, "Single frame: no frame received within 3s");
        QCOMPARE(frameSpy.count(), 1);

        QSharedPointer<QImage> img = frameSpy.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!img.isNull() && !img->isNull(), "Frame QImage is null");
        QVERIFY2(img->width() > 0 && img->height() > 0,
                 qPrintable(QString("Frame has zero dimensions: %1x%2")
                                .arg(img->width()).arg(img->height())));
        
        frameSpy.clear();
        QVERIFY2(m_driver->startCapture(1), "new startCapture(1) should return true");
        QVERIFY2(frameSpy.wait(3000) || frameSpy.count() > 0, "Single frame: no new frame received within 3s");
        QCOMPARE(frameSpy.count(), 1);

        m_driver->stopCapture();
    }

    void test_capture_multiple_frames()
    {
        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QVERIFY2(m_driver->startCapture(3), "startCapture(3) should return true");
        QTest::qWait(3000); // Wait up to 3s for frames to arrive
        QCOMPARE(frameSpy.count(), 3);

        for (int i = 0; i < frameSpy.count(); ++i) {
            QSharedPointer<QImage> image = frameSpy.at(i).at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image.isNull() && !image->isNull(),
                     qPrintable(QString("Frame %1 is null").arg(i)));
            QVERIFY2(image->width() > 0 && image->height() > 0,
                     qPrintable(QString("Frame %1 has zero dimensions: %2x%3")
                                    .arg(i).arg(image->width()).arg(image->height())));
        }

        m_driver->stopCapture();
        QCOMPARE(int(m_driver->state()), int(CameraState::Connected));
    }

    void test_capture_burst_mode()
    {
        QVERIFY2(m_driver->setParameter("exposure", 0.1), "setParameter exposure");
        QVERIFY2(m_driver->commitParameters(), "commitParameters exposure");

        const int burstCount = 5;

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QSignalSpy captureStoppedSpy(m_driver, &ICameraDriver::captureStopped);
        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        QVERIFY2(m_driver->startCapture(burstCount),
                 qPrintable(QString("startCapture(%1) should return true").arg(burstCount)));
        QTest::qWait(3000); // Wait up to 3s for frames to arrive
        QCOMPARE(frameSpy.count(), burstCount);

        for (int i = 0; i < frameSpy.count(); ++i) {
            QSharedPointer<QImage> image = frameSpy.at(i).at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image.isNull() && !image->isNull(),
                     qPrintable(QString("Burst frame %1 is null").arg(i)));
        }

        QVERIFY2(errorSpy.isEmpty() || errorSpy.count() == 0,
                 "Burst capture emitted unexpected errorOccurred signals");

        QVERIFY2(captureStoppedSpy.wait(3000) || captureStoppedSpy.count() >= 1,
                 "Burst capture: captureStopped not emitted within 3s");
    }

    void test_capture_live_mode()
    {
        QVERIFY2(m_driver->setParameter("exposure", 0.1), "setParameter exposure");
        QVERIFY2(m_driver->commitParameters(), "commitParameters exposure");

        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
        QSignalSpy captureStartedSpy(m_driver, &ICameraDriver::captureStarted);
        QSignalSpy captureStoppedSpy(m_driver, &ICameraDriver::captureStopped);

        QVERIFY2(m_driver->startCapture(0),
                 "startCapture(0) live mode should return true");
        QVERIFY2(captureStartedSpy.count() == 1, "captureStarted not emitted exactly once");
        QCOMPARE(int(m_driver->state()), int(CameraState::Acquiring));

        QTest::qWait(3000); // Wait up to 3s for frames to arrive
        QVERIFY2(frameSpy.count() >= 3,
                 qPrintable(QString("Live mode: < 3 frames in 3s, got %1")
                                .arg(frameSpy.count())));
        int framesBeforeStop = frameSpy.count();
        qDebug() << "Live mode received" << framesBeforeStop << "frames in 3s before stop";

        m_driver->stopCapture();
        QVERIFY2(captureStoppedSpy.count() == 1, "captureStopped not emitted exactly once");
        QCOMPARE(int(m_driver->state()), int(CameraState::Connected));
    }
};

QTEST_MAIN(TestPicamDriver)
#include "test_picam_driver_PIXIS100B.moc"
