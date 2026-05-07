#include <QCoreApplication>
#include <QtTest>
#include <QImage>

#include "data/PostProcessManager.h"

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
    PostProcessManager manager;

    PostProcessManager::ProcessConfig configDisabled;
    configDisabled.enabled = false;
    QVERIFY2(configDisabled.enabled == false, "Config should be disabled by default");

    PostProcessManager::ProcessConfig configEnabled;
    configEnabled.enabled = true;
    QVERIFY2(configEnabled.enabled == true, "Config should be enabled when set to true");

    configEnabled.enabled = false;
    QVERIFY2(configEnabled.enabled == false, "Config should be disabled when set to false");
}

void TestPostProcessManager::testOperations()
{
    PostProcessManager manager;

    PostProcessManager::ProcessConfig config;
    QVERIFY2(config.operations == PostProcessManager::None, "Should have no operations by default");

    config.operations = PostProcessManager::VerticalBinning;
    QVERIFY2((config.operations & PostProcessManager::VerticalBinning) == PostProcessManager::VerticalBinning,
             "VerticalBinning should be enabled");

    config.operations |= PostProcessManager::DarkFrameSubtraction;
    QVERIFY2((config.operations & PostProcessManager::DarkFrameSubtraction) == PostProcessManager::DarkFrameSubtraction,
             "DarkFrameSubtraction should be enabled");

    config.operations &= ~PostProcessManager::VerticalBinning;
    QVERIFY2((config.operations & PostProcessManager::VerticalBinning) == 0,
             "VerticalBinning should be disabled after clearing it");
}

void TestPostProcessManager::testVerticalBinningRowRange()
{
    PostProcessManager manager;

    PostProcessManager::ProcessConfig config;
    QVERIFY2(config.vBinStartRow == 0, "Default start row should be 0");
    QVERIFY2(config.vBinEndRow == -1, "Default end row should be -1");

    config.vBinStartRow = 10;
    config.vBinEndRow = 100;
    QVERIFY2(config.vBinStartRow == 10, "Start row should be 10");
    QVERIFY2(config.vBinEndRow == 100, "End row should be 100");
}

void TestPostProcessManager::testVerticalBinning()
{
    PostProcessManager manager;

    QImage testImage(10, 5, QImage::Format_Grayscale8);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 10; ++x) {
            testImage.setPixel(x, y, y * 50);
        }
    }

    PostProcessManager::ProcessConfig config;
    config.enabled = true;
    config.operations = PostProcessManager::VerticalBinning;
    config.vBinStartRow = 0;
    config.vBinEndRow = 4;

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    manager.processFrame(frame, config);

    QVERIFY2(frame.image.width() == 10, "Width should remain 10");
    QVERIFY2(frame.image.height() == 1, "Height should be binned to 1");
    QVERIFY2(frame.hasOriginal() == true, "Should have original image stored");
}

void TestPostProcessManager::testDarkFrameSubtraction()
{
    PostProcessManager manager;

    QImage testImage(10, 10, QImage::Format_Grayscale8);
    testImage.fill(200);

    QImage darkFrame(10, 10, QImage::Format_Grayscale8);
    darkFrame.fill(50);

    PostProcessManager::ProcessConfig config;
    config.enabled = true;
    config.operations = PostProcessManager::DarkFrameSubtraction;
    config.darkFrame = darkFrame;

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    manager.processFrame(frame, config);

    QVERIFY2(frame.image.width() == 10, "Width should remain 10");
    QVERIFY2(frame.image.height() == 10, "Height should remain 10");

    int pixelValue = qGray(frame.image.pixel(5, 5));
    QVERIFY2(pixelValue == 150, "Pixel value should be 200 - 50 = 150 after dark frame subtraction");
}

void TestPostProcessManager::testFlatFieldCorrection()
{
    PostProcessManager manager;

    QImage testImage(10, 10, QImage::Format_Grayscale8);
    testImage.fill(128);

    QImage flatField(10, 10, QImage::Format_Grayscale8);
    flatField.fill(64);

    PostProcessManager::ProcessConfig config;
    config.enabled = true;
    config.operations = PostProcessManager::FlatFieldCorrection;
    config.flatField = flatField;

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    manager.processFrame(frame, config);

    QVERIFY2(frame.image.width() == 10, "Width should remain 10");
    QVERIFY2(frame.image.height() == 10, "Height should remain 10");

    int pixelValue = qGray(frame.image.pixel(5, 5));
    QVERIFY2(pixelValue == 255, "Pixel value should be saturated to 255 after flat field correction (128/64*255)");
}

void TestPostProcessManager::testProcessFrame()
{
    PostProcessManager manager;

    QImage testImage(10, 10, QImage::Format_Grayscale8);
    testImage.fill(100);

    PostProcessManager::ProcessConfig config;
    config.enabled = false;

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    QImage originalBefore = frame.image;

    manager.processFrame(frame, config);

    QVERIFY2(frame.image == originalBefore, "Image should not change when config is disabled");
    QVERIFY2(frame.hasOriginal() == false, "Should not store original when disabled");
}

QTEST_MAIN(TestPostProcessManager)
#include "test_post_process_manager.moc"
