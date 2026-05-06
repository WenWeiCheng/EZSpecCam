#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>
#include <QImage>

#include "data/DataLoader.h"

class TestDataLoader : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testLoadImage();
    void testLoadNonExistentImage();
    void testExtractFrameNumber();
    void testGenerateFilenames();
    void testListImagesInDirectory();
    void testListConfigsInDirectory();

private:
    QTemporaryDir m_tempDir;
    QString m_testDir;
};

void TestDataLoader::initTestCase()
{
    m_testDir = m_tempDir.path();
}

void TestDataLoader::cleanupTestCase()
{
}

void TestDataLoader::init()
{
}

void TestDataLoader::cleanup()
{
}

void TestDataLoader::testLoadImage()
{
    DataLoader loader;

    QString testImagePath = m_testDir + "/test_image.png";
    QImage testImage(100, 100, QImage::Format_Grayscale8);
    testImage.fill(Qt::gray);
    testImage.save(testImagePath);

    QImage loaded = loader.loadImage(testImagePath);

    QVERIFY2(!loaded.isNull(), "Loaded image should not be null");
    QVERIFY2(loaded.width() == 100, "Loaded image width mismatch");
    QVERIFY2(loaded.height() == 100, "Loaded image height mismatch");
}

void TestDataLoader::testLoadNonExistentImage()
{
    DataLoader loader;

    QImage loaded = loader.loadImage(m_testDir + "/non_existent.png");

    QVERIFY2(loaded.isNull(), "Loading non-existent image should return null image");
}

void TestDataLoader::testExtractFrameNumber()
{
    DataLoader loader;

    int frameNum = loader.extractFrameNumber("img_000000000001.tiff");
    QVERIFY2(frameNum == 1, "Should extract frame number 1 from tiff");

    frameNum = loader.extractFrameNumber("img_000000000042.jpg");
    QVERIFY2(frameNum == 42, "Should extract frame number 42 from jpg");

    frameNum = loader.extractFrameNumber("cfg_000000000001.ini");
    QVERIFY2(frameNum == 1, "Should extract frame number 1 from ini");

    frameNum = loader.extractFrameNumber("random_file.txt");
    QVERIFY2(frameNum == -1, "Should return -1 for unrecognized format");
}

void TestDataLoader::testGenerateFilenames()
{
    DataLoader loader;

    QString configFilename = loader.generateConfigFilename(1);
    QVERIFY2(configFilename == "cfg_000000000001.ini", "Config filename mismatch");

    QString imageFilename = loader.generateImageFilename(1);
    QVERIFY2(imageFilename == "img_000000000001.tiff", "Image filename mismatch");

    imageFilename = loader.generateImageFilename(42);
    QVERIFY2(imageFilename == "img_000000000042.tiff", "Image filename mismatch for frame 42");
}

void TestDataLoader::testListImagesInDirectory()
{
    DataLoader loader;

    QDir dir(m_testDir);
    QImage img1(10, 10, QImage::Format_Grayscale8);
    img1.save(m_testDir + "/img_000000000001.png");
    img1.save(m_testDir + "/img_000000000002.tiff");
    img1.save(m_testDir + "/img_000000000003.jpg");

    QStringList images = loader.listImagesInDirectory(m_testDir);

    QVERIFY2(images.size() == 3, "Should find 3 images");
}

void TestDataLoader::testListConfigsInDirectory()
{
    DataLoader loader;

    QFile file1(m_testDir + "/cfg_000000000001.ini");
    file1.open(QIODevice::WriteOnly);
    file1.close();

    QFile file2(m_testDir + "/cfg_000000000002.ini");
    file2.open(QIODevice::WriteOnly);
    file2.close();

    QStringList configs = loader.listConfigsInDirectory(m_testDir);

    QVERIFY2(configs.size() == 2, "Should find 2 config files");
}

QTEST_MAIN(TestDataLoader)
#include "tst_test_data_loader.moc"
