#include <QCoreApplication>
#include <QtTest>
#include <QImage>

#include "PostProcess.h"

class TestPostProcessManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testEnabled();
    void testOperations();
    void testVerticalBinningRowRange();
    void testVerticalBinning();
    void testDarkFrameSubtraction();
    void testFlatFieldCorrection();
    void testProcessFrame();

private:
};

void TestPostProcessManager::initTestCase()
{
}

void TestPostProcessManager::cleanupTestCase()
{
}

void TestPostProcessManager::init()
{
}

void TestPostProcessManager::cleanup()
{
}

void TestPostProcessManager::testEnabled()
{

    PostProcess::ProcessConfig configDisabled;
    configDisabled.enabled = false;
    QVERIFY2(configDisabled.enabled == false, "Config should be disabled by default");

    PostProcess::ProcessConfig configEnabled;
    configEnabled.enabled = true;
    QVERIFY2(configEnabled.enabled == true, "Config should be enabled when set to true");

    configEnabled.enabled = false;
    QVERIFY2(configEnabled.enabled == false, "Config should be disabled when set to false");
}

void TestPostProcessManager::testOperations()
{

    PostProcess::ProcessConfig config;
    QVERIFY2(config.operations == PostProcess::None, "Should have no operations by default");

    config.operations = PostProcess::VerticalBinning;
    QVERIFY2((config.operations & PostProcess::VerticalBinning) == PostProcess::VerticalBinning,
             "VerticalBinning should be enabled");

    config.operations |= PostProcess::DarkFrameSubtraction;
    QVERIFY2((config.operations & PostProcess::DarkFrameSubtraction) == PostProcess::DarkFrameSubtraction,
             "DarkFrameSubtraction should be enabled");

    config.operations &= ~PostProcess::VerticalBinning;
    QVERIFY2((config.operations & PostProcess::VerticalBinning) == 0,
             "VerticalBinning should be disabled after clearing it");
}

void TestPostProcessManager::testVerticalBinningRowRange()
{

    PostProcess::ProcessConfig config;
    QVERIFY2(config.vBinStartRow == 0, "Default start row should be 0");
    QVERIFY2(config.vBinEndRow == -1, "Default end row should be -1");

    config.vBinStartRow = 10;
    config.vBinEndRow = 100;
    QVERIFY2(config.vBinStartRow == 10, "Start row should be 10");
    QVERIFY2(config.vBinEndRow == 100, "End row should be 100");
}

void TestPostProcessManager::testVerticalBinning()
{

    QImage testImage(10, 5, QImage::Format_Grayscale8);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 10; ++x) {
            testImage.setPixel(x, y, y * 50);
        }
    }

    PostProcess::ProcessConfig config;
    config.enabled = true;
    config.operations = PostProcess::VerticalBinning;
    config.vBinStartRow = 0;
    config.vBinEndRow = 4;

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    PostProcess::processFrame(frame, config);

    QVERIFY2(frame.image.width() == 10, "Width should remain 10");
    QVERIFY2(frame.image.height() == 1, "Height should be binned to 1");
    QVERIFY2(frame.hasOriginal() == true, "Should have original image stored");
}

void TestPostProcessManager::testDarkFrameSubtraction()
{

    QImage testImage(10, 10, QImage::Format_Grayscale8);
    testImage.fill(200);

    QImage darkFrame(10, 10, QImage::Format_Grayscale8);
    darkFrame.fill(50);

    PostProcess::ProcessConfig config;
    config.enabled = true;
    config.operations = PostProcess::DarkFrameSubtraction;
    config.darkFrame = darkFrame;

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    PostProcess::processFrame(frame, config);

    QVERIFY2(frame.image.width() == 10, "Width should remain 10");
    QVERIFY2(frame.image.height() == 10, "Height should remain 10");

    int pixelValue = qGray(frame.image.pixel(5, 5));
    QVERIFY2(pixelValue == 150, "Pixel value should be 200 - 50 = 150 after dark frame subtraction");
}

void TestPostProcessManager::testFlatFieldCorrection()
{

    QImage testImage(10, 10, QImage::Format_Grayscale8);
    testImage.fill(128);

    QImage flatField(10, 10, QImage::Format_Grayscale8);
    flatField.fill(64);

    PostProcess::ProcessConfig config;
    config.enabled = true;
    config.operations = PostProcess::FlatFieldCorrection;
    config.flatField = flatField;

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    PostProcess::processFrame(frame, config);

    QVERIFY2(frame.image.width() == 10, "Width should remain 10");
    QVERIFY2(frame.image.height() == 10, "Height should remain 10");

    int pixelValue = qGray(frame.image.pixel(5, 5));
    QVERIFY2(pixelValue == 255, "Pixel value should be saturated to 255 after flat field correction (128/64*255)");
}

void TestPostProcessManager::testProcessFrame()
{

    QImage testImage(10, 10, QImage::Format_Grayscale8);
    testImage.fill(100);

    PostProcess::ProcessConfig config;
    config.enabled = false;

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    QImage originalBefore = frame.image;

    PostProcess::processFrame(frame, config);

    QVERIFY2(frame.image == originalBefore, "Image should not change when config is disabled");
    QVERIFY2(frame.hasOriginal() == false, "Should not store original when disabled");
}

QTEST_MAIN(TestPostProcessManager)
#include "test_post_process_manager.moc"
