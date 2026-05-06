#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>
#include <QImage>

#include "data/DataSaver.h"

class TestDataSaver : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testSaveImage();
    void testSaveFrame();
    void testFrameCounter();
    void testSaveOptions();

private:
    QTemporaryDir m_tempDir;
    QString m_testDir;
    QString m_cameraId;
};

void TestDataSaver::initTestCase()
{
    m_testDir = m_tempDir.path();
    m_cameraId = "test-camera-001";
}

void TestDataSaver::cleanupTestCase()
{
}

void TestDataSaver::init()
{
}

void TestDataSaver::cleanup()
{
}

void TestDataSaver::testSaveImage()
{
    DataSaver saver;

    QImage testImage(100, 100, QImage::Format_Grayscale8);
    testImage.fill(Qt::gray);

    QString filePath = m_testDir + "/test_save.png";
    bool result = saver.saveImage(testImage, filePath);

    QVERIFY2(result == true, "Save image should succeed");
    QVERIFY2(QFile::exists(filePath), "Image file should exist after save");
}

void TestDataSaver::testSaveFrame()
{
    DataSaver saver;

    ImageData frame;
    frame.image = QImage(100, 100, QImage::Format_Grayscale8);
    frame.image.fill(Qt::gray);
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();
    frame.cameraId = m_cameraId;
    frame.frameNumber = 1;

    bool result = saver.saveFrame(frame, m_testDir);

    QVERIFY2(result == true, "Save frame should succeed");

    QString expectedPath = m_testDir + QDir::separator() + "img_000000000001.tiff";
    QVERIFY2(QFile::exists(expectedPath), "Image file should exist after save");
}

void TestDataSaver::testFrameCounter()
{
    DataSaver saver;

    QVERIFY2(saver.frameCounter() == 0, "Frame counter should start at 0");

    saver.resetFrameCounter();
    QVERIFY2(saver.frameCounter() == 0, "Frame counter should be 0 after reset");

    ImageData frame;
    frame.image = QImage(10, 10, QImage::Format_Grayscale8);
    frame.image.fill(Qt::gray);
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();
    frame.cameraId = m_cameraId;

    saver.saveFrame(frame, m_testDir);
    QVERIFY2(saver.frameCounter() == 1, "Frame counter should be 1 after first save");

    saver.saveFrame(frame, m_testDir);
    QVERIFY2(saver.frameCounter() == 2, "Frame counter should be 2 after second save");
}

void TestDataSaver::testSaveOptions()
{
    DataSaver saver;

    ImageSaveOptions opts;
    opts.format = ImageFormat::JPEG;
    opts.quality = 90;

    saver.setSaveOptions(opts);

    ImageSaveOptions loaded = saver.saveOptions();
    QVERIFY2(loaded.format == ImageFormat::JPEG, "Format should be JPEG");
    QVERIFY2(loaded.quality == 90, "Quality should be 90");
}

QTEST_MAIN(TestDataSaver)
#include "tst_test_data_saver.moc"
