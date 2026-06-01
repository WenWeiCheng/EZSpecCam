#include <QTest>
#include <QDebug>
#include <QSignalSpy>
#include <QImage>
#include <qobject.h>
#include <qtestcase.h>

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
        if(!names.contains("serial_number")){
            QWARN("serial_number parameter does not exist");
        } else {
            param = m_driver->parameter("serial_number");
            QVERIFY2(param.isValid(), "serial_number should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "serial_number should be String type");
        }

        // camera_model
        if(!names.contains("camera_model")){
            QWARN("camera_model parameter does not exist");
        } else {
            param = m_driver->parameter("camera_model");
            QVERIFY2(param.isValid(), "camera_model should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "camera_model should be String type");
        }

        // chipWidth
        if(!names.contains("chipWidth")){
            QWARN("chipWidth parameter does not exist");
        } else {
            param = m_driver->parameter("chipWidth");
            QVERIFY2(param.isValid(), "chipWidth should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "chipWidth should be String type");
        }

        // chipHeight
        if(!names.contains("chipHeight")){
            QWARN("chipHeight parameter does not exist");
        } else {
            param = m_driver->parameter("chipHeight");
            QVERIFY2(param.isValid(), "chipHeight should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "chipHeight should be String type");
        }
        param = m_driver->parameter("chipHeight");
        QVERIFY2(param.isValid(), "chipHeight should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "chipHeight should be String type");

        // imageWidth
        if(!names.contains("imageWidth")){
            QWARN("imageWidth parameter does not exist");
        } else {
            param = m_driver->parameter("imageWidth");
            QVERIFY2(param.isValid(), "imageWidth should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "imageWidth should be String type");
        }

        // imageHeight
        if(!names.contains("imageHeight")){
            QWARN("imageHeight parameter does not exist");
        } else {
            param = m_driver->parameter("imageHeight");
            QVERIFY2(param.isValid(), "imageHeight should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "imageHeight should be String type");
        }

        param = m_driver->parameter("imageHeight");
        QVERIFY2(param.isValid(), "imageHeight should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "imageHeight should be String type");

        // pixelWidth
        QVERIFY2(names.contains("pixelWidth"), "pixelWidth parameter should exist");

        param = m_driver->parameter("pixelWidth");
        QVERIFY2(param.isValid(), "pixelWidth should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "pixelWidth should be String type");

        // pixelHeight
        if(!names.contains("pixelHeight")){
            QWARN("pixelHeight parameter does not exist");
        } else {
            param = m_driver->parameter("pixelHeight");
            QVERIFY2(param.isValid(), "pixelHeight should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "pixelHeight should be String type");
        }

        // imageBytes
        if(!names.contains("imageBytes")){
            QWARN("imageBytes parameter does not exist");
        } else {
            param = m_driver->parameter("imageBytes");
            QVERIFY2(param.isValid(), "imageBytes should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "imageBytes should be String type");
        }
        
        // effective_start_x
        if(!names.contains("effective_start_x")){
            QWARN("effective_start_x parameter does not exist");
        } else {
            param = m_driver->parameter("effective_start_x");
            QVERIFY2(param.isValid(), "effective_start_x should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "effective_start_x should be String type");
        }
        
        // effective_start_y
        if(!names.contains("effective_start_y")){
            QWARN("effective_start_y parameter does not exist");
        } else {
            param = m_driver->parameter("effective_start_y");
            QVERIFY2(param.isValid(), "effective_start_y should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "effective_start_y should be String type");
        }
        
        // effective_width
        if(!names.contains("effective_width")){
            QWARN("effective_width parameter does not exist");
        } else {
            param = m_driver->parameter("effective_width");
            QVERIFY2(param.isValid(), "effective_width should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "effective_width should be String type");
        }
        
        // effective_height
        if(!names.contains("effective_height")){
            QWARN("effective_height parameter does not exist");
        } else {
            param = m_driver->parameter("effective_height");
            QVERIFY2(param.isValid(), "effective_height should be a valid parameter");
            QVERIFY2(param.type == ParameterType::String, "effective_height should be String type");
        }
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
        QVariant minValue = param.constraint.minValue;
        QVERIFY2(minValue.isValid(), "exposure constraint minValue should be valid");
        QVariant maxVal = param.constraint.maxValue;
        QVERIFY2(maxVal.isValid(), "exposure constraint maxValue should be valid");
        QVERIFY2(minValue.toDouble() < maxVal.toDouble(),
                 "exposure minValue should be less than maxValue");
        QVariant step = param.constraint.step;
        QVERIFY2(step.isValid() && step.toDouble() > 0,
                 "exposure step should be positive");

        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
        m_driver->setParameter("exposure", minValue);
        m_driver->commitParameters();
        QVERIFY2(errorSpy.isEmpty() || errorSpy.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting exposure to min should not produce error");
        errorSpy.clear();

        m_driver->setParameter("exposure", maxVal);
        m_driver->commitParameters();
        QVERIFY2(errorSpy.isEmpty() || errorSpy.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting exposure to max should not produce error");

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
        QVERIFY2(defaultD >= minValue.toDouble() && defaultD <= maxVal.toDouble(),
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
        if(!names.contains("gain")){
            QSKIP("gain parameter does not exist");
        }
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

        QSignalSpy errorSpy2(m_driver, &ICameraDriver::errorOccurred);
        m_driver->setParameter("gain", minVal);
        m_driver->setParameter("offset", 10);
        m_driver->commitParameters();
        QVERIFY2(errorSpy2.isEmpty() || errorSpy2.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting gain to min should not produce error");
        errorSpy2.clear();

        m_driver->setParameter("gain", maxVal);
        m_driver->commitParameters();
        QVERIFY2(errorSpy2.isEmpty() || errorSpy2.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting gain to max should not produce error");

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
        if (image0->format() == QImage::Format_Grayscale16) {
            for (int y = 0; y < image0->height(); ++y) {
                const quint16 *sourceLine = reinterpret_cast<const quint16 *>(
                    image0->constBits() + y * image0->bytesPerLine());
                for (int x = 0; x < image0->width(); ++x) {
                    sum0 += sourceLine[x];
                    count0++;
                }
            }
        } else {
            for (int y = 0; y < image0->height(); ++y) {
                for (int x = 0; x < image0->width(); ++x) {
                    QRgb pixel = image0->pixel(x, y);
                    sum0 += qGray(pixel);
                    count0++;
                }
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
        if (image50->format() == QImage::Format_Grayscale16) {
            for (int y = 0; y < image50->height(); ++y) {
                const quint16 *sourceLine = reinterpret_cast<const quint16 *>(
                    image50->constBits() + y * image50->bytesPerLine());
                for (int x = 0; x < image50->width(); ++x) {
                    sum50 += sourceLine[x];
                    count50++;
                }
            }
        } else {
            for (int y = 0; y < image50->height(); ++y) {
                for (int x = 0; x < image50->width(); ++x) {
                    QRgb pixel = image50->pixel(x, y);
                    sum50 += qGray(pixel);
                    count50++;
                }
            }
        }
        double mean50 = (count50 > 0) ? (sum50 / count50) : 0;

        qDebug() << "Gain=0 mean:" << mean0 << "Gain=50 mean:" << mean50;
        QVERIFY2(mean50 > mean0, "Mean at gain=50 should be greater than mean at gain=0");
    }

    void test_param_offset()
    {
        QStringList names = m_driver->parameterNames();
        if(!names.contains("offset")){
            QSKIP("offset parameter does not exist");
        }
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
        
        QSignalSpy errorSpy3(m_driver, &ICameraDriver::errorOccurred);
        m_driver->setParameter("offset", minVal);
        m_driver->commitParameters();
        QVERIFY2(errorSpy3.isEmpty() || errorSpy3.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting offset to min should not produce error");
        errorSpy3.clear();

        m_driver->setParameter("offset", maxVal);
        m_driver->commitParameters();
        QVERIFY2(errorSpy3.isEmpty() || errorSpy3.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting offset to max should not produce error");

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
        if (image0->format() == QImage::Format_Grayscale16) {
            for (int y = 0; y < image0->height(); ++y) {
                const quint16 *sourceLine = reinterpret_cast<const quint16 *>(
                    image0->constBits() + y * image0->bytesPerLine());
                for (int x = 0; x < image0->width(); ++x) {
                    sum0 += sourceLine[x];
                    count0++;
                }
            }
        } else {
            for (int y = 0; y < image0->height(); ++y) {
                for (int x = 0; x < image0->width(); ++x) {
                    QRgb pixel = image0->pixel(x, y);
                    sum0 += qGray(pixel);
                    count0++;
                }
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
        if (image50->format() == QImage::Format_Grayscale16) {
            for (int y = 0; y < image50->height(); ++y) {
                const quint16 *sourceLine = reinterpret_cast<const quint16 *>(
                    image50->constBits() + y * image50->bytesPerLine());
                for (int x = 0; x < image50->width(); ++x) {
                    sum50 += sourceLine[x];
                    count50++;
                }
            }
        } else {
            for (int y = 0; y < image50->height(); ++y) {
                for (int x = 0; x < image50->width(); ++x) {
                    QRgb pixel = image50->pixel(x, y);
                    sum50 += qGray(pixel);
                    count50++;
                }
            }
        }
        double mean50 = (count50 > 0) ? (sum50 / count50) : 0;

        qDebug() << "Offset=0 mean:" << mean0 << "Offset=50 mean:" << mean50;
        QVERIFY2(mean50 > mean0,
                 "Mean at offset=50 should be greater than mean at offset=0");
    }

    //==========================================================================
    // Parameter Tests - ROI
    //==========================================================================
    void test_param_roi()
    {
        // verify existence of parameters
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("roi_x"), "roi_x parameter should exist");
        QVERIFY2(names.contains("roi_y"), "roi_y parameter should exist");
        QVERIFY2(names.contains("roi_width"), "roi_width parameter should exist");
        QVERIFY2(names.contains("roi_height"), "roi_height parameter should exist");

        ParameterDefinition param = m_driver->parameter("roi_x");
        QVERIFY2(param.isValid(), "roi_x should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "roi_x should be IntRange type");

        param = m_driver->parameter("roi_y");
        QVERIFY2(param.isValid(), "roi_y should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "roi_y should be IntRange type");

        param = m_driver->parameter("roi_width");
        QVERIFY2(param.isValid(), "roi_width should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "roi_width should be IntRange type");

        param = m_driver->parameter("roi_height");
        QVERIFY2(param.isValid(), "roi_height should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "roi_height should be IntRange type");
        
        ParameterDefinition widthParam = m_driver->parameter("roi_width");
        ParameterDefinition heightParam = m_driver->parameter("roi_height");
        QVariant fullWidth = widthParam.constraint.maxValue;
        QVariant fullHeight = heightParam.constraint.maxValue;
        QVERIFY2(fullWidth.isValid(), "Full width should be valid");
        QVERIFY2(fullHeight.isValid(), "Full height should be valid");

        m_driver->setParameter("roi_width", fullWidth);
        m_driver->setParameter("roi_height", fullHeight);
        m_driver->commitParameters();

        QSignalSpy frameSpyFull(m_driver, &ICameraDriver::frameReady);
        m_driver->startCapture(1);
        QVERIFY2(frameSpyFull.wait(3000), "Should receive full frame");
        QSharedPointer<QImage> fullImage = frameSpyFull.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!fullImage->isNull(), "Full image should not be null");
        int fullW = fullImage->width();
        int fullH = fullImage->height();
        qDebug() << "Full frame:" << fullW << "x" << fullH;

        int halfWidth = fullW / 2;
        int halfHeight = fullH / 2;
        m_driver->setParameter("roi_width", halfWidth);
        m_driver->setParameter("roi_height", halfHeight);
        m_driver->commitParameters();

        QSignalSpy frameSpyHalf(m_driver, &ICameraDriver::frameReady);
        m_driver->startCapture(1);
        QVERIFY2(frameSpyHalf.wait(3000), "Should receive half frame");
        QSharedPointer<QImage> halfImage = frameSpyHalf.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!halfImage->isNull(), "Half image should not be null");
        qDebug() << "Half frame:" << halfImage->width() << "x" << halfImage->height();
        QVERIFY2(halfImage->width() == halfWidth, "Half frame width should be half of full");
        QVERIFY2(halfImage->height() == halfHeight, "Half frame height should be half of full");
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
        
        QVector<QVariant> validBinnings = param.constraint.validValues;
        QVERIFY2(!validBinnings.isEmpty(), "binning should have valid values");

        m_driver->setParameter("roi_width", param.constraint.maxValue);
        m_driver->setParameter("roi_height", param.constraint.maxValue);
        m_driver->commitParameters();

        QSignalSpy frameSpy1(m_driver, &ICameraDriver::frameReady);
        m_driver->startCapture(1);
        QVERIFY2(frameSpy1.wait(3000), "Should receive frame at binning=1");
        QSharedPointer<QImage> image1 = frameSpy1.takeFirst().at(0).value<QSharedPointer<QImage>>();
        QVERIFY2(!image1->isNull(), "Image at binning=1 should not be null");
        int width1 = image1->width();
        int height1 = image1->height();
        qDebug() << "Binning 1:" << width1 << "x" << height1;

        for (const QVariant &vb : validBinnings) {
            int bin = vb.toInt();
            if (bin <= 1) {
                continue;
            }
            m_driver->setParameter("binning", bin);
            m_driver->commitParameters();

            QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
            m_driver->startCapture(1);
            QVERIFY2(frameSpy.wait(3000), qPrintable(QString("Should receive frame at binning=%1").arg(bin)));
            QSharedPointer<QImage> image = frameSpy.takeFirst().at(0).value<QSharedPointer<QImage>>();
            QVERIFY2(!image->isNull(), qPrintable(QString("Image at binning=%1 should not be null").arg(bin)));
            qDebug() << "Binning" << bin << ":" << image->width() << "x" << image->height();
            QVERIFY2(image->width() == width1 / bin,
                     qPrintable(QString("Width at binning=%1 should be %2 (was %3)")
                                .arg(bin).arg(width1 / bin).arg(image->width())));
            QVERIFY2(image->height() == height1 / bin,
                     qPrintable(QString("Height at binning=%1 should be %2 (was %3)")
                                .arg(bin).arg(height1 / bin).arg(image->height())));
        }
        
    }

    void test_binning_roi_ranges()
    {
        // Get binning parameter to check available values
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("binning"), "binning parameter should exist");

        ParameterDefinition binningParam = m_driver->parameter("binning");
        QVector<QVariant> validBinnings = binningParam.constraint.validValues;

        // Find a binning value > 1
        int testBinning = 1;
        for (const QVariant &vb : validBinnings) {
            if (vb.toInt() > 1) {
                testBinning = vb.toInt();
                break;
            }
        }

        if (testBinning <= 1) {
            QSKIP("Binning > 1 not available");
        }

        // Set binning to 1 and commit
        m_driver->setParameter("binning", 1);
        m_driver->commitParameters();

        // Get initial ROI parameter definitions at binning=1
        ParameterDefinition roiWidth = m_driver->parameter("roi_width");
        ParameterDefinition roiHeight = m_driver->parameter("roi_height");
        ParameterDefinition roiX = m_driver->parameter("roi_x");
        ParameterDefinition roiY = m_driver->parameter("roi_y");

        double initialWidthMax = roiWidth.constraint.maxValue;
        double initialHeightMax = roiHeight.constraint.maxValue;
        double initialXMax = roiX.constraint.maxValue;
        double initialYMax = roiY.constraint.maxValue;

        qDebug() << "Binning 1 - roi_width maxValue:" << initialWidthMax
                 << "roi_height maxValue:" << initialHeightMax;
        qDebug() << "Binning 1 - roi_x maxValue:" << initialXMax
                 << "roi_y maxValue:" << initialYMax;

        // Change binning to testBinning and commit
        m_driver->setParameter("binning", testBinning);
        m_driver->commitParameters();

        // Get ROI parameter definitions again after binning change
        ParameterDefinition newRoiWidth = m_driver->parameter("roi_width");
        ParameterDefinition newRoiHeight = m_driver->parameter("roi_height");
        ParameterDefinition newRoiX = m_driver->parameter("roi_x");
        ParameterDefinition newRoiY = m_driver->parameter("roi_y");

        double newWidthMax = newRoiWidth.constraint.maxValue;
        double newHeightMax = newRoiHeight.constraint.maxValue;
        double newXMax = newRoiX.constraint.maxValue;
        double newYMax = newRoiY.constraint.maxValue;

        qDebug() << "Binning" << testBinning << "- roi_width maxValue:" << newWidthMax
                 << "roi_height maxValue:" << newHeightMax;
        qDebug() << "Binning" << testBinning << "- roi_x maxValue:" << newXMax
                 << "roi_y maxValue:" << newYMax;

        // Verify maxValue constraints are scaled by binning factor
        double expectedWidthMax = initialWidthMax / testBinning;
        double expectedHeightMax = initialHeightMax / testBinning;

        QVERIFY2(qAbs(newWidthMax - expectedWidthMax) < 1e-6,
                 qPrintable(QString("roi_width maxValue should be %1 but got %2 at binning %3")
                            .arg(expectedWidthMax).arg(newWidthMax).arg(testBinning)));
        QVERIFY2(qAbs(newHeightMax - expectedHeightMax) < 1e-6,
                 qPrintable(QString("roi_height maxValue should be %1 but got %2 at binning %3")
                            .arg(expectedHeightMax).arg(newHeightMax).arg(testBinning)));

        // roi_x and roi_y maxValues should also be scaled (they represent max offset which scales with binning)
        QVERIFY2(qAbs((newXMax+1) - (initialXMax+1) / testBinning) < 1e-6,
                 qPrintable(QString("roi_x maxValue should be %1 but got %2 at binning %3")
                            .arg(initialXMax / testBinning).arg(newXMax).arg(testBinning)));
        QVERIFY2(qAbs((newYMax+1) - (initialYMax+1) / testBinning) < 1e-6,
                 qPrintable(QString("roi_y maxValue should be %1 but got %2 at binning %3")
                            .arg(initialYMax / testBinning).arg(newYMax).arg(testBinning)));

        // Verify defaultValue of roi_width/roi_height match the new max values
        QVERIFY2(qAbs(newRoiWidth.defaultValue.toDouble() - newWidthMax) < 1e-6,
                 qPrintable(QString("roi_width defaultValue should be %1 but got %2 at binning %3")
                            .arg(newWidthMax).arg(newRoiWidth.defaultValue.toDouble()).arg(testBinning)));
        QVERIFY2(qAbs(newRoiHeight.defaultValue.toDouble() - newHeightMax) < 1e-6,
                 qPrintable(QString("roi_height defaultValue should be %1 but got %2 at binning %3")
                            .arg(newHeightMax).arg(newRoiHeight.defaultValue.toDouble()).arg(testBinning)));
    }

    //==========================================================================
    // Parameter Tests - Advanced (usb_traffic)
    //==========================================================================
    void test_param_usb_traffic()
    {
        QStringList names = m_driver->parameterNames();
        if(!names.contains("usb_traffic")){
            QSKIP("usb_traffic parameter does not exist");
        }
        QVERIFY2(names.contains("usb_traffic"), "usb_traffic parameter should exist");

        ParameterDefinition param = m_driver->parameter("usb_traffic");
        QVERIFY2(param.isValid(), "usb_traffic should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntRange, "usb_traffic should be IntRange type");
        
        QVariant minUsb = param.constraint.minValue;
        QVariant maxUsb = param.constraint.maxValue;
        QVERIFY2(minUsb.isValid(), "usb_traffic minValue should be valid");
        QVERIFY2(maxUsb.isValid(), "usb_traffic maxValue should be valid");

        QSignalSpy errorSpy6(m_driver, &ICameraDriver::errorOccurred);
        m_driver->setParameter("usb_traffic", minUsb);
        m_driver->commitParameters();
        QVERIFY2(errorSpy6.isEmpty() || errorSpy6.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting usb_traffic to min should not produce error");
        errorSpy6.clear();

        m_driver->setParameter("usb_traffic", maxUsb);
        m_driver->commitParameters();
        QVERIFY2(errorSpy6.isEmpty() || errorSpy6.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting usb_traffic to max should not produce error");
    }

    void test_param_read_mode()
    {
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("read_mode"), "read_mode parameter should exist");

        ParameterDefinition param = m_driver->parameter("read_mode");
        QVERIFY2(param.isValid(), "read_mode should be a valid parameter");
        QVERIFY2(param.type == ParameterType::StringCollection, "read_mode should be StringCollection type");
        
        QVector<QVariant> validModesVec = param.constraint.validValues;
        QStringList validModes;
        for (const QVariant &v : validModesVec) {
            validModes.append(v.toString());
        }
        qDebug() << "Available read_mode values:" << validModes;
        QVERIFY2(!validModes.isEmpty(), "read_mode should have valid values");

        if (!validModes.isEmpty()) {
            QSignalSpy errorSpy8(m_driver, &ICameraDriver::errorOccurred);
            qDebug() << "Setting read_mode to" << validModes.first();
            m_driver->setParameter("read_mode", validModes.first());
            m_driver->commitParameters();
            QVERIFY2(errorSpy8.isEmpty() || errorSpy8.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                     "Setting read_mode to first value should not produce error");
            errorSpy8.clear();

            qDebug() << "Setting read_mode to" << validModes.last();
            m_driver->setParameter("read_mode", validModes.last());
            m_driver->commitParameters();
            QVERIFY2(errorSpy8.isEmpty() || errorSpy8.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                     "Setting read_mode to last value should not produce error");
        }
    }

    void test_param_transfer_bit()
    {
        QStringList names = m_driver->parameterNames();
        if(!names.contains("transfer_bit")){
            qDebug() << "Transfer bit not supported";
        }
        QVERIFY2(names.contains("transfer_bit"), "transfer_bit parameter should exist");

        ParameterDefinition param = m_driver->parameter("transfer_bit");
        QVERIFY2(param.isValid(), "transfer_bit should be a valid parameter");
        QVERIFY2(param.type == ParameterType::IntCollection, "transfer_bit should be IntCollection type");

        QVector<QVariant> validBits = param.constraint.validValues;
        QVERIFY2(!validBits.isEmpty(), "transfer_bit should have valid values");

        QSignalSpy errorSpy9(m_driver, &ICameraDriver::errorOccurred);
        m_driver->setParameter("transfer_bit", validBits.first());
        m_driver->commitParameters();
        m_driver->startCapture(1);
        QVERIFY2(errorSpy9.isEmpty() || errorSpy9.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting transfer_bit to min should not produce error");
        errorSpy9.clear();

        m_driver->setParameter("transfer_bit", validBits.last());
        m_driver->commitParameters();
        m_driver->startCapture(1);
        QVERIFY2(errorSpy9.isEmpty() || errorSpy9.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Setting transfer_bit to max should not produce error");
    }

    //==========================================================================
    // Parameter Tests - Cooling 
    //==========================================================================
    void test_param_cooler()
    {
        QStringList names = m_driver->parameterNames();
        if(!names.contains("cooler_enabled")){
            qDebug() << "Cooler not supported";
        }
        QVERIFY2(names.contains("cooler_enabled"), "cooler_enabled parameter should exist");
        QVERIFY2(names.contains("target_temperature"), "target_temperature parameter should exist");
        QVERIFY2(names.contains("current_temperature"), "current_temperature parameter should exist");

        ParameterDefinition param = m_driver->parameter("cooler_enabled");
        QVERIFY2(param.isValid(), "cooler_enabled should be a valid parameter");
        QVERIFY2(param.type == ParameterType::Boolean, "cooler_enabled should be Boolean type");
        param = m_driver->parameter("target_temperature");
        QVERIFY2(param.isValid(), "target_temperature should be a valid parameter");
        QVERIFY2(param.type == ParameterType::FloatRange, "target_temperature should be FloatRange type");
        param = m_driver->parameter("current_temperature");
        QVERIFY2(param.isValid(), "current_temperature should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "current_temperature should be String type");
        
        QSignalSpy errorSpy10(m_driver, &ICameraDriver::errorOccurred);
        m_driver->setParameter("cooler_enabled", true);
        m_driver->commitParameters();
        QVERIFY2(errorSpy10.isEmpty() || errorSpy10.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Enabling cooler should not produce error");
        errorSpy10.clear();

        m_driver->setParameter("cooler_enabled", false);
        m_driver->commitParameters();
        QVERIFY2(errorSpy10.isEmpty() || errorSpy10.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Disabling cooler should not produce error");

        m_driver->setParameter("cooler_enabled", false);
        m_driver->commitParameters();
        QVariant tempVal = m_driver->parameterValue("current_temperature");
        QVERIFY2(tempVal.isValid(), "current_temperature should be readable");
        qDebug() << "Current temperature:" << tempVal.toString();

        ParameterDefinition targetParam = m_driver->parameter("target_temperature");
        QVariant minTemp = targetParam.constraint.minValue;
        QVariant maxTemp = targetParam.constraint.maxValue;
        QVERIFY2(minTemp.isValid(), "target_temperature minValue should be valid");
        QVERIFY2(maxTemp.isValid(), "target_temperature maxValue should be valid");

        QSignalSpy errorSpy12(m_driver, &ICameraDriver::errorOccurred);
        m_driver->setParameter("cooler_enabled", true);
        m_driver->setParameter("target_temperature", minTemp);
        m_driver->commitParameters();
        QVERIFY2(errorSpy12.isEmpty() || errorSpy12.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 qPrintable(QString("Setting target_temperature to min (%1) should not produce error").arg(minTemp.toString())));
        errorSpy12.clear();

        m_driver->setParameter("target_temperature", maxTemp);
        m_driver->commitParameters();
        QVERIFY2(errorSpy12.isEmpty() || errorSpy12.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 qPrintable(QString("Setting target_temperature to max (%1) should not produce error").arg(maxTemp.toString())));

        m_driver->setParameter("target_temperature", minTemp);
        m_driver->commitParameters();
        qDebug() << "Cooler enabled, target=0, waiting 10s...";
        QTest::qWait(10000);
        QVariant newTempVal = m_driver->parameterValue("current_temperature");
        QVERIFY2(newTempVal.isValid(), "current_temperature should be readable after wait");
        qDebug() << "Temperature after 10s:" << newTempVal.toString();
        bool tempDecreased = false;
        bool ok1 = false, ok2 = false;
        double oldTempD = tempVal.toString().toDouble(&ok1);
        double newTempD = newTempVal.toString().toDouble(&ok2);
        if (ok1 && ok2) {
            tempDecreased = newTempD < oldTempD;
            qDebug() << "Temp change:" << oldTempD << "->" << newTempD << "decreased:" << tempDecreased;
        }
        QVERIFY2(tempDecreased, "Temperature should decrease after enabling cooler with target=0");
    }

    //==========================================================================
    // Parameter Tests - Info/Dynamic (humidity)
    //==========================================================================
    void test_param_humidity()
    {
        QStringList names = m_driver->parameterNames();
        if(!names.contains("humidity")){
            QSKIP("humidity parameter does not exist");
        }
        QVERIFY2(names.contains("humidity"), "humidity parameter should exist");

        ParameterDefinition param = m_driver->parameter("humidity");
        QVERIFY2(param.isValid(), "humidity should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "humidity should be String type");
        
        QSignalSpy errorSpy14(m_driver, &ICameraDriver::errorOccurred);
        QVariant humidityVal = m_driver->parameterValue("humidity");
        QVERIFY2(humidityVal.isValid(), "humidity should be readable");
        QVERIFY2(errorSpy14.isEmpty() || errorSpy14.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Reading humidity should not produce error");
        qDebug() << "Humidity:" << humidityVal.toString();
    }

    void test_param_pressure()
    {
        QStringList names = m_driver->parameterNames();
        if(!names.contains("pressure")){
            QSKIP("pressure parameter does not exist");
        }
        QVERIFY2(names.contains("pressure"), "pressure parameter should exist");

        ParameterDefinition param = m_driver->parameter("pressure");
        QVERIFY2(param.isValid(), "pressure should be a valid parameter");
        QVERIFY2(param.type == ParameterType::String, "pressure should be String type");
        
        QSignalSpy errorSpy15(m_driver, &ICameraDriver::errorOccurred);
        QVariant pressureVal = m_driver->parameterValue("pressure");
        QVERIFY2(pressureVal.isValid(), "pressure should be readable");
        QVERIFY2(errorSpy15.isEmpty() || errorSpy15.at(0).at(0).value<CameraError>().code == CameraError::Code::None,
                 "Reading pressure should not produce error");
        qDebug() << "Pressure:" << pressureVal.toString();
    }

    void test_stream_mode()
    {
        //======================================================================
        // Case 1: Parameter definition
        //======================================================================
        QStringList names = m_driver->parameterNames();
        QVERIFY2(names.contains("stream_mode"), "stream_mode parameter should exist");

        ParameterDefinition param = m_driver->parameter("stream_mode");
        QVERIFY2(param.isValid(), "stream_mode should be a valid parameter");
        QVERIFY2(param.type == ParameterType::StringCollection, "stream_mode should be StringCollection type");
        QVERIFY2(param.category == ParameterCategory::Core, "stream_mode should be Core category");
        QVERIFY2(!param.displayName.isEmpty(), "stream_mode displayName should be non-empty");

        QVector<QVariant> validModesVec = param.constraint.validValues;
        QStringList validModes;
        for (const QVariant &v : validModesVec) {
            validModes.append(v.toString());
        }
        qDebug() << "Available stream_mode values:" << validModes;
        QVERIFY2(!validModes.isEmpty(), "stream_mode should have at least one valid value");

        //======================================================================
        // Case 2: Default value is valid and matches actual parameter value
        //======================================================================
        QVariant defaultVal = param.defaultValue;
        QVERIFY2(validModes.contains(defaultVal.toString()),
                 qPrintable(QString("Default value '%1' should be in valid values: %2")
                            .arg(defaultVal.toString()).arg(validModes.join(", "))));

        QVariant actualVal = m_driver->parameterValue("stream_mode");
        QVERIFY2(actualVal.isValid(), "stream_mode value should be readable");
        qDebug() << "stream_mode default:" << defaultVal << "actual:" << actualVal;

        //======================================================================
        // Case 3 & 4: Set valid modes (Single Frame / Live Video)
        //======================================================================
        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);

        for (const QString &mode : validModes) {
            errorSpy.clear();
            qDebug() << "Setting stream_mode to" << mode;
            m_driver->setParameter("stream_mode", mode);
            m_driver->commitParameters();

            // Verify no error occurred
            bool hasError = !errorSpy.isEmpty() &&
                            errorSpy.at(0).at(0).value<CameraError>().code != CameraError::Code::None;
            QVERIFY2(!hasError,
                     qPrintable(QString("Setting stream_mode to '%1' should not produce error").arg(mode)));

            // Verify the value was actually set
            QVariant setVal = m_driver->parameterValue("stream_mode");
            QVERIFY2(setVal.isValid(), qPrintable(QString("stream_mode '%1' should be readable after set").arg(mode)));
        }

        //======================================================================
        // Case 6: Invalid value is rejected
        //======================================================================
        errorSpy.clear();
        m_driver->setParameter("stream_mode", "InvalidMode123");
        m_driver->commitParameters();

        bool gotError = !errorSpy.isEmpty() &&
                        errorSpy.at(0).at(0).value<CameraError>().code != CameraError::Code::None;
        qDebug() << "Invalid stream_mode produced error:" << gotError;
        QVERIFY2(gotError, "Invalid stream_mode value should be rejected");
    }

    //==========================================================================
    // Capture Mode Tests (captureCount parameter)
    //==========================================================================
    void test_capture_single_mode()
    {
        QVector<QVariant> validModesVec = m_driver->parameter("stream_mode").constraint.validValues;
        QStringList validModes;
        for (const QVariant &v : validModesVec) {
            validModes.append(v.toString());
        }
        qDebug() << "Testing single capture with stream modes:" << validModes;

        for (const QString &mode : validModes) {
            qDebug() << "=== Single capture with stream_mode:" << mode << "===";
            m_driver->setParameter("stream_mode", mode);
            m_driver->setParameter("exposure", 100.0);
            m_driver->commitParameters();

            QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
            QSignalSpy captureStoppedSpy(m_driver, &ICameraDriver::captureStopped);

            bool started = m_driver->startCapture(1);
            QVERIFY2(started, qPrintable(QString("Should start single frame capture (mode: %1)").arg(mode)));

            bool gotFrame = frameSpy.wait(6000);
            QVERIFY2(gotFrame, qPrintable(QString("Should receive single frame (mode: %1)").arg(mode)));

            bool stopped = captureStoppedSpy.wait(3000) || captureStoppedSpy.count() >= 1;
            QVERIFY2(stopped, qPrintable(QString("Single capture should auto-stop after 1 frame (mode: %1)").arg(mode)));

            QList<QList<QVariant>> allFrames = frameSpy;
            qDebug() << "Single mode (" << mode << "): received" << allFrames.size() << "frame(s)";
            QVERIFY2(allFrames.size() == 1,
                     qPrintable(QString("Single mode should capture exactly 1 frame (mode: %1)").arg(mode)));
        }
    }

    void test_capture_live_mode()
    {
        QVector<QVariant> validModesVec = m_driver->parameter("stream_mode").constraint.validValues;
        QStringList validModes;
        for (const QVariant &v : validModesVec) {
            validModes.append(v.toString());
        }
        qDebug() << "Testing live capture with stream modes:" << validModes;

        for (const QString &mode : validModes) {
            qDebug() << "=== Live capture with stream_mode:" << mode << "===";
            m_driver->setParameter("stream_mode", mode);
            m_driver->setParameter("exposure", 100.0);
            m_driver->commitParameters();

            QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
            QSignalSpy captureStoppedSpy(m_driver, &ICameraDriver::captureStopped);

            bool started = m_driver->startCapture(0);
            QVERIFY2(started, qPrintable(QString("Should start live capture (mode: %1)").arg(mode)));

            int frameCount = 0;
            for (int i = 0; i < 3; ++i) {
                bool gotFrame = frameSpy.wait(6000);
                QVERIFY2(gotFrame,
                         qPrintable(QString("Should receive frame %1 in live mode (mode: %2)").arg(i + 1).arg(mode)));
                frameCount = frameSpy.size();
                qDebug() << "Live mode (" << mode << "): received" << frameCount << "frame(s) so far";
            }

            QVERIFY2(captureStoppedSpy.size() == 0,
                     qPrintable(QString("Live mode should not auto-stop (mode: %1)").arg(mode)));

            m_driver->stopCapture(5000);
            bool stopped = captureStoppedSpy.wait(3000) || captureStoppedSpy.count() >= 1;
            QVERIFY2(stopped,
                     qPrintable(QString("Should receive captureStopped after stopCapture() (mode: %1)").arg(mode)));

            qDebug() << "Live mode (" << mode << "): stopped after" << frameSpy.size() << "total frames";
            QVERIFY2(frameSpy.size() >= 3,
                     qPrintable(QString("Live mode should capture multiple frames before stop (mode: %1)").arg(mode)));
        }
    }

    void test_capture_burst_mode()
    {
        QVector<QVariant> validModesVec = m_driver->parameter("stream_mode").constraint.validValues;
        QStringList validModes;
        for (const QVariant &v : validModesVec) {
            validModes.append(v.toString());
        }
        qDebug() << "Testing burst capture with stream modes:" << validModes;

        const int burstCount = 5;

        for (const QString &mode : validModes) {
            qDebug() << "=== Burst capture with stream_mode:" << mode << "===";
            m_driver->setParameter("stream_mode", mode);
            m_driver->setParameter("exposure", 100.0);
            m_driver->commitParameters();

            QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);
            QSignalSpy captureStoppedSpy(m_driver, &ICameraDriver::captureStopped);

            bool started = m_driver->startCapture(burstCount);
            QVERIFY2(started, qPrintable(QString("Should start burst capture (mode: %1)").arg(mode)));

            bool gotAllFrames = frameSpy.wait(15000);
            QVERIFY2(gotAllFrames,
                     qPrintable(QString("Should receive burst frames (mode: %1)").arg(mode)));

            bool stopped = captureStoppedSpy.wait(5000) || captureStoppedSpy.count() >= 1;
            QVERIFY2(stopped,
                     qPrintable(QString("Burst capture should auto-stop after N frames (mode: %1)").arg(mode)));

            QList<QList<QVariant>> allFrames = frameSpy;
            qDebug() << "Burst mode (" << mode << ", N=" << burstCount << "): received"
                     << allFrames.size() << "frame(s)";
            QVERIFY2(allFrames.size() == burstCount,
                     qPrintable(QString("Burst mode should capture exactly %1 frames (mode: %2)")
                                .arg(burstCount).arg(mode)));
        }
    }

    //==========================================================================
    // Signal Tests
    //==========================================================================
    void test_signals()
    {
        QSignalSpy connectionSpy(m_driver, &ICameraDriver::connectionChanged);
        QSignalSpy captureStartedSpy(m_driver, &ICameraDriver::captureStarted);
        QSignalSpy captureStoppedSpy(m_driver, &ICameraDriver::captureStopped);
        QSignalSpy errorSpy(m_driver, &ICameraDriver::errorOccurred);
        QSignalSpy frameSpy(m_driver, &ICameraDriver::frameReady);

        m_driver->disconnectCamera();
        // 信号已经同步发出，直接检查 count
        QVERIFY2(connectionSpy.count() >= 1, "Should have at least one connectionChanged signal");
        bool lastConnState = connectionSpy.last().at(0).toBool();
        QVERIFY2(lastConnState == false, "Last connection state should be false");
        QList<QList<QVariant>> connArgs = connectionSpy;
        QVERIFY2(connArgs.size() >= 1, "Should have at least one connectionChanged signal");

        connectionSpy.clear(); 
        captureStoppedSpy.clear();
        bool reconnected = m_driver->connectToCamera(m_cameraId);
        QVERIFY2(reconnected, "Should reconnect to camera");
        QVERIFY2(connectionSpy.count() >= 1, "Should have at least one connectionChanged signal");
        connArgs = connectionSpy;
        lastConnState = connArgs.last().at(0).toBool();
        QVERIFY2(lastConnState == true, "Last connection state should be true (connected)");

        m_driver->setParameter("exposure", 100.0);
        m_driver->commitParameters();

        bool started = m_driver->startCapture(1);
        QVERIFY2(started, "Should start capture");

        bool gotFrame = frameSpy.wait(6000);

        QVERIFY2(gotFrame, "Should receive frame");

        QVERIFY2(captureStartedSpy.count() >= 1, "Should have at least one captureStarted signal");
        QList<QVariant> startedArgs = captureStartedSpy.last();
        QVERIFY2(startedArgs.at(0).toString() == m_cameraId, "captureStarted cameraId should match");

        m_driver->stopCapture();
        QVERIFY2(captureStoppedSpy.count() >= 1, "Should receive captureStopped signal");
        QList<QVariant> stoppedArgs = captureStoppedSpy.last();
        QVERIFY2(stoppedArgs.at(0).toString() == m_cameraId, "captureStopped cameraId should match");

        errorSpy.clear();
        m_driver->setParameter("exposure", -1.0);
        m_driver->commitParameters();
        QVERIFY2(!errorSpy.isEmpty(), "Should receive error signal for invalid exposure");
        QList<QVariant> errArgs = errorSpy.last();
        CameraError err = errArgs.at(0).value<CameraError>();
        QVERIFY2(err.code != CameraError::Code::None, "Should receive error for invalid exposure");
        qDebug() << "Got expected error code:" << static_cast<int>(err.code) << err.description;
    }

private:
    QHYCCDDriver *m_driver = nullptr;
    QString m_cameraId;
};

QTEST_MAIN(TestQHYCCDDriver)
#include "test_qhyccd_driver_QHY268m.moc"
