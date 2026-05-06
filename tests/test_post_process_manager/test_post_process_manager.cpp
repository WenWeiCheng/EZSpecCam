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

    QVERIFY2(manager.isEnabled() == false, "Should be disabled by default");

    manager.setEnabled(true);
    QVERIFY2(manager.isEnabled() == true, "Should be enabled after setEnabled(true)");

    manager.setEnabled(false);
    QVERIFY2(manager.isEnabled() == false, "Should be disabled after setEnabled(false)");
}

void TestPostProcessManager::testOperations()
{
    PostProcessManager manager;

    QVERIFY2(manager.operations() == PostProcessManager::None, "Should have no operations by default");

    manager.setOperationEnabled(PostProcessManager::VerticalBinning, true);
    QVERIFY2(manager.isOperationEnabled(PostProcessManager::VerticalBinning) == true,
             "VerticalBinning should be enabled");

    manager.setOperationEnabled(PostProcessManager::DarkFrameSubtraction, true);
    QVERIFY2(manager.isOperationEnabled(PostProcessManager::DarkFrameSubtraction) == true,
             "DarkFrameSubtraction should be enabled");

    manager.setOperationEnabled(PostProcessManager::VerticalBinning, false);
    QVERIFY2(manager.isOperationEnabled(PostProcessManager::VerticalBinning) == false,
             "VerticalBinning should be disabled after setOperationEnabled(false)");
}

void TestPostProcessManager::testVerticalBinningRowRange()
{
    PostProcessManager manager;

    QPair<int, int> range = manager.verticalBinningRowRange();
    QVERIFY2(range.first == 0, "Default start row should be 0");
    QVERIFY2(range.second == -1, "Default end row should be -1");

    manager.setVerticalBinningRowRange(10, 100);
    range = manager.verticalBinningRowRange();
    QVERIFY2(range.first == 10, "Start row should be 10");
    QVERIFY2(range.second == 100, "End row should be 100");
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

    manager.setEnabled(true);
    manager.setOperationEnabled(PostProcessManager::VerticalBinning, true);
    manager.setVerticalBinningRowRange(0, 4);

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    manager.processFrame(frame);

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

    manager.setEnabled(true);
    manager.setOperationEnabled(PostProcessManager::DarkFrameSubtraction, true);
    manager.setDarkFrame(darkFrame);

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    manager.processFrame(frame);

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

    manager.setEnabled(true);
    manager.setOperationEnabled(PostProcessManager::FlatFieldCorrection, true);
    manager.setFlatField(flatField);

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    manager.processFrame(frame);

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

    manager.setEnabled(false);

    ImageData frame;
    frame.image = testImage;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    QImage originalBefore = frame.image;

    manager.processFrame(frame);

    QVERIFY2(frame.image == originalBefore, "Image should not change when manager is disabled");
    QVERIFY2(frame.hasOriginal() == false, "Should not store original when disabled");
}

QTEST_MAIN(TestPostProcessManager)
#include "test_post_process_manager.moc"
