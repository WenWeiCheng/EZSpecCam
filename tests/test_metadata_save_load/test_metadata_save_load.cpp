#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

#include "CameraTypes.h"
#include "formats/SaveTypes.h"
#include "formats/TiffFormatHandler.h"
#include "formats/CsvFormatHandler.h"
#include "gui/workers/FileLoaderWorker.h"

class TestMetadataSoftwareSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ---- Save side ----
    void test_tiff_save_writes_2d_original_as_main();
    void test_csv_save_writes_2d_original_as_main();
    void test_tiff_save_falls_back_to_image_when_no_original();
    void test_csv_save_falls_back_to_image_when_no_original();
    void test_tiff_save_always_writes_metadata();
    void test_csv_save_always_writes_metadata();
    void test_empty_software_settings_writes_empty_object();

    // ---- Load side ----
    void test_loader_reads_software_settings_block_tiff();
    void test_loader_handles_missing_metadata_sidecar();
    void test_loader_handles_malformed_metadata_sidecar();
    void test_roundtrip_software_settings_via_tiff();
    void test_csv_image_roundtrip_software_settings();
    void test_legacy_1row_spectrum_file_loads_but_no_original();

private:
    QTemporaryDir m_tempDir;
    QImage makeSyntheticImage(int w = 8, int h = 4) const;
    QString writeImage(const QString &name, const QImage &img) const;
    QJsonObject readMetadataJson(const QString &imgPath) const;
    LoadResult loadSync(const QString &filePath) const;
};

void TestMetadataSoftwareSettings::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestMetadataSoftwareSettings::cleanupTestCase()
{
}

void TestMetadataSoftwareSettings::init()
{
}

void TestMetadataSoftwareSettings::cleanup()
{
}

QImage TestMetadataSoftwareSettings::makeSyntheticImage(int w, int h) const
{
    QImage img(w, h, QImage::Format_Grayscale16);
    for (int y = 0; y < h; ++y) {
        ushort *row = reinterpret_cast<ushort *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            row[x] = static_cast<ushort>((x * 100 + y * 50) & 0xFFFF);
        }
    }
    return img;
}

QString TestMetadataSoftwareSettings::writeImage(const QString &name, const QImage &img) const
{
    QString path = m_tempDir.path() + "/" + name;
    img.save(path);
    return path;
}

LoadResult TestMetadataSoftwareSettings::loadSync(const QString &filePath) const
{
    FileLoaderWorker worker;
    QSignalSpy okSpy(&worker, &FileLoaderWorker::frameLoaded);
    QSignalSpy failSpy(&worker, &FileLoaderWorker::loadFailed);
    worker.loadFrame(filePath);
    if (okSpy.count() == 0) {
        return LoadResult();
    }
    return qvariant_cast<LoadResult>(okSpy.takeFirst().at(0));
}

QJsonObject TestMetadataSoftwareSettings::readMetadataJson(const QString &imgPath) const
{
    QFileInfo fi(imgPath);
    QString metaPath = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_metadata.json";
    QFile f(metaPath);
    QJsonDocument doc;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        doc = QJsonDocument::fromJson(f.readAll());
    }
    return doc.object();
}

// =====================================================================
// Save side
// =====================================================================

void TestMetadataSoftwareSettings::test_tiff_save_writes_2d_original_as_main()
{
    QImage original2D = makeSyntheticImage(8, 4);
    QImage current1Row(8, 1, QImage::Format_Grayscale16);

    QString imgPath = m_tempDir.path() + "/vbin.tiff";

    SaveRequest req;
    req.filePath = imgPath;
    req.frame.image = current1Row;
    req.frame.originalImage = original2D;
    req.frame.cameraId = "cam-x";
    req.frame.timestamp = 1234567890ULL;
    req.frame.frameNumber = 7;
    req.frame.parameters["exposure"] = 100.0;
    req.frame.softwareSettings["softwareVerticalBinning"] = true;
    req.frame.softwareSettings["vBinStartRow"] = 1;
    req.frame.softwareSettings["vBinEndRow"] = 3;

    TiffFormatHandler handler;
    QVERIFY(handler.save(req));

    // 主图必须是 2D original
    QImage loaded(imgPath, "TIFF");
    QCOMPARE(loaded.width(), original2D.width());
    QCOMPARE(loaded.height(), original2D.height());

    // 不再写 _original 边车
    QFileInfo fi(imgPath);
    QString origPath = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_original.tiff";
    QVERIFY(!QFile::exists(origPath));

    // metadata 总是写
    QJsonObject root = readMetadataJson(imgPath);
    QVERIFY(root.contains("softwareSettings"));
    QVERIFY(root.contains("parameters"));
    QJsonObject sw = root.value("softwareSettings").toObject();
    QCOMPARE(sw.value("softwareVerticalBinning").toBool(), true);
    QCOMPARE(sw.value("vBinStartRow").toInt(), 1);
    QCOMPARE(sw.value("vBinEndRow").toInt(), 3);
}

void TestMetadataSoftwareSettings::test_csv_save_writes_2d_original_as_main()
{
    QImage original2D = makeSyntheticImage(6, 3);
    QString imgPath = m_tempDir.path() + "/vbin.csv";

    SaveRequest req;
    req.filePath = imgPath;
    req.frame.image = original2D;
    req.frame.originalImage = original2D;
    req.frame.cameraId = "cam-y";
    req.frame.timestamp = 999ULL;
    req.frame.frameNumber = 1;
    req.frame.softwareSettings["softwareVerticalBinning"] = false;

    CsvFormatHandler handler;
    QVERIFY(handler.save(req));

    // CSV 行数 = 3（2D 矩阵）
    QFile f(imgPath);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    int lines = 0;
    while (!f.atEnd()) {
        QString l = QString::fromUtf8(f.readLine()).trimmed();
        if (!l.isEmpty()) lines++;
    }
    QCOMPARE(lines, 3);

    // 不再写 _original 边车
    QFileInfo fi(imgPath);
    QString origPath = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_original.csv";
    QVERIFY(!QFile::exists(origPath));

    QJsonObject root = readMetadataJson(imgPath);
    QVERIFY(root.contains("softwareSettings"));
}

void TestMetadataSoftwareSettings::test_tiff_save_falls_back_to_image_when_no_original()
{
    QImage img = makeSyntheticImage(10, 5);
    QString imgPath = m_tempDir.path() + "/no_original.tiff";

    SaveRequest req;
    req.filePath = imgPath;
    req.frame.image = img;
    req.frame.originalImage = QImage();  // 空
    req.frame.cameraId = "cam-z";

    TiffFormatHandler handler;
    QVERIFY(handler.save(req));

    QImage loaded(imgPath, "TIFF");
    QCOMPARE(loaded.size(), img.size());
}

void TestMetadataSoftwareSettings::test_csv_save_falls_back_to_image_when_no_original()
{
    QImage img = makeSyntheticImage(7, 3);
    QString imgPath = m_tempDir.path() + "/no_original.csv";

    SaveRequest req;
    req.filePath = imgPath;
    req.frame.image = img;
    req.frame.originalImage = QImage();

    CsvFormatHandler handler;
    QVERIFY(handler.save(req));

    QFile f(imgPath);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    int lines = 0;
    while (!f.atEnd()) {
        QString l = QString::fromUtf8(f.readLine()).trimmed();
        if (!l.isEmpty()) lines++;
    }
    QCOMPARE(lines, 3);
}

void TestMetadataSoftwareSettings::test_tiff_save_always_writes_metadata()
{
    QString imgPath = m_tempDir.path() + "/always_meta.tiff";

    SaveRequest req;
    req.filePath = imgPath;
    req.frame.image = makeSyntheticImage(4, 4);
    req.frame.originalImage = makeSyntheticImage(4, 4);
    req.frame.cameraId = "always";

    TiffFormatHandler handler;
    QVERIFY(handler.save(req));

    QFileInfo fi(imgPath);
    QString metaPath = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_metadata.json";
    QVERIFY(QFile::exists(metaPath));
}

void TestMetadataSoftwareSettings::test_csv_save_always_writes_metadata()
{
    QString imgPath = m_tempDir.path() + "/always_meta.csv";

    SaveRequest req;
    req.filePath = imgPath;
    req.frame.image = makeSyntheticImage(4, 4);

    CsvFormatHandler handler;
    QVERIFY(handler.save(req));

    QFileInfo fi(imgPath);
    QString metaPath = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_metadata.json";
    QVERIFY(QFile::exists(metaPath));
}

void TestMetadataSoftwareSettings::test_empty_software_settings_writes_empty_object()
{
    QString imgPath = writeImage("empty_sw.tiff", makeSyntheticImage());

    SaveRequest req;
    req.filePath = imgPath;
    req.frame.image = makeSyntheticImage();
    req.frame.cameraId = "cam-z";

    TiffFormatHandler handler;
    QVERIFY(handler.save(req));

    QJsonObject root = readMetadataJson(imgPath);
    QVERIFY(root.contains("softwareSettings"));
    QJsonObject sw = root.value("softwareSettings").toObject();
    QCOMPARE(sw.size(), 0);
}

// =====================================================================
// Load side
// =====================================================================

void TestMetadataSoftwareSettings::test_loader_reads_software_settings_block_tiff()
{
    QString imgPath = m_tempDir.path() + "/loader_in.tiff";

    SaveRequest req;
    req.filePath = imgPath;
    req.frame.image = makeSyntheticImage(10, 5);
    req.frame.originalImage = makeSyntheticImage(10, 5);
    req.frame.cameraId = "loader-cam";
    req.frame.timestamp = 42ULL;
    req.frame.frameNumber = 3;
    req.frame.parameters["gain"] = 2.5;
    req.frame.softwareSettings["softwareVerticalBinning"] = true;
    req.frame.softwareSettings["vBinStartRow"] = 5;
    req.frame.softwareSettings["vBinEndRow"] = 100;

    TiffFormatHandler handler;
    QVERIFY(handler.save(req));

    FileLoaderWorker worker;
    QSignalSpy okSpy(&worker, &FileLoaderWorker::frameLoaded);
    QSignalSpy failSpy(&worker, &FileLoaderWorker::loadFailed);

    worker.loadFrame(imgPath);

    QCOMPARE(failSpy.count(), 0);
    QCOMPARE(okSpy.count(), 1);

    QList<QVariant> args = okSpy.takeFirst();
    LoadResult res = qvariant_cast<LoadResult>(args.at(0));
    QString signaledPath = args.at(1).toString();

    QCOMPARE(signaledPath, imgPath);
    QVERIFY(res.success);
    QVERIFY(res.hasMetadata);
    QCOMPARE(res.frame.cameraId, QString("loader-cam"));
    QCOMPARE(res.frame.softwareSettings.value("softwareVerticalBinning").toBool(), true);
    QCOMPARE(res.frame.softwareSettings.value("vBinStartRow").toInt(), 5);
    QCOMPARE(res.frame.softwareSettings.value("vBinEndRow").toInt(), 100);
    QCOMPARE(res.frame.parameters.value("gain").toDouble(), 2.5);
    // 新格式：主图就是 2D original
    QCOMPARE(res.frame.image.height(), 5);
    QVERIFY(res.frame.originalImage.isNull());  // loader 不再加载 _original 边车
}

void TestMetadataSoftwareSettings::test_loader_handles_missing_metadata_sidecar()
{
    QString imgPath = writeImage("no_metadata.tiff", makeSyntheticImage(4, 4));

    FileLoaderWorker worker;
    QSignalSpy okSpy(&worker, &FileLoaderWorker::frameLoaded);
    QSignalSpy failSpy(&worker, &FileLoaderWorker::loadFailed);

    worker.loadFrame(imgPath);

    QCOMPARE(failSpy.count(), 0);
    QCOMPARE(okSpy.count(), 1);
    QList<QVariant> args = okSpy.takeFirst();
    LoadResult res = qvariant_cast<LoadResult>(args.at(0));
    QVERIFY(res.success);
    QVERIFY(!res.hasMetadata);
    QVERIFY(res.frame.softwareSettings.isEmpty());
    QVERIFY(!res.frame.image.isNull());
}

void TestMetadataSoftwareSettings::test_loader_handles_malformed_metadata_sidecar()
{
    QString imgPath = writeImage("bad_metadata.tiff", makeSyntheticImage(4, 4));
    QFileInfo fi(imgPath);
    QString metaPath = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_metadata.json";
    QFile f(metaPath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("{ this is not valid json :::");
    f.close();

    FileLoaderWorker worker;
    QSignalSpy okSpy(&worker, &FileLoaderWorker::frameLoaded);
    QSignalSpy failSpy(&worker, &FileLoaderWorker::loadFailed);

    worker.loadFrame(imgPath);

    QCOMPARE(failSpy.count(), 0);
    QCOMPARE(okSpy.count(), 1);
    QList<QVariant> args = okSpy.takeFirst();
    LoadResult res = qvariant_cast<LoadResult>(args.at(0));
    QVERIFY(res.success);
    QVERIFY(!res.hasMetadata);
    QVERIFY(!res.frame.image.isNull());
}

void TestMetadataSoftwareSettings::test_roundtrip_software_settings_via_tiff()
{
    QString imgPath = m_tempDir.path() + "/roundtrip.tiff";

    SaveRequest writeReq;
    writeReq.filePath = imgPath;
    writeReq.frame.image = makeSyntheticImage(12, 6);
    writeReq.frame.originalImage = makeSyntheticImage(12, 6);
    writeReq.frame.cameraId = "rt-cam";
    writeReq.frame.timestamp = 1000ULL;
    writeReq.frame.frameNumber = 1;
    writeReq.frame.softwareSettings["softwareVerticalBinning"] = true;
    writeReq.frame.softwareSettings["vBinStartRow"] = 1;
    writeReq.frame.softwareSettings["vBinEndRow"] = 5;

    TiffFormatHandler saver;
    QVERIFY(saver.save(writeReq));

    FileLoaderWorker loader;
    QSignalSpy spy(&loader, &FileLoaderWorker::frameLoaded);
    loader.loadFrame(imgPath);
    QCOMPARE(spy.count(), 1);

    LoadResult res = qvariant_cast<LoadResult>(spy.takeFirst().at(0));
    QVERIFY(res.success);
    QVERIFY(res.hasMetadata);
    QVERIFY(res.frame.softwareSettings.value("softwareVerticalBinning").toBool());
    QCOMPARE(res.frame.softwareSettings.value("vBinStartRow").toInt(), 1);
    QCOMPARE(res.frame.softwareSettings.value("vBinEndRow").toInt(), 5);
    QCOMPARE(res.frame.image.size(), makeSyntheticImage(12, 6).size());
}

void TestMetadataSoftwareSettings::test_csv_image_roundtrip_software_settings()
{
    QString imgPath = m_tempDir.path() + "/csv_rt.csv";
    QImage src = makeSyntheticImage(5, 3);

    SaveRequest writeReq;
    writeReq.filePath = imgPath;
    writeReq.frame.image = src;
    writeReq.frame.originalImage = src;
    writeReq.frame.cameraId = "csv-cam";
    writeReq.frame.timestamp = 555ULL;
    writeReq.frame.softwareSettings["softwareVerticalBinning"] = true;
    writeReq.frame.softwareSettings["vBinStartRow"] = 1;
    writeReq.frame.softwareSettings["vBinEndRow"] = 2;

    CsvFormatHandler saver;
    QVERIFY(saver.save(writeReq));

    FileLoaderWorker loader;
    QSignalSpy spy(&loader, &FileLoaderWorker::frameLoaded);
    QSignalSpy failSpy(&loader, &FileLoaderWorker::loadFailed);

    loader.loadFrame(imgPath);
    QCOMPARE(failSpy.count(), 0);
    QCOMPARE(spy.count(), 1);

    LoadResult res = qvariant_cast<LoadResult>(spy.takeFirst().at(0));
    QVERIFY(res.success);
    QVERIFY(res.hasMetadata);
    QCOMPARE(res.frame.image.width(), src.width());
    QCOMPARE(res.frame.image.height(), src.height());
    QVERIFY(res.frame.softwareSettings.value("softwareVerticalBinning").toBool());
}

void TestMetadataSoftwareSettings::test_legacy_1row_spectrum_file_loads_but_no_original()
{
    // 模拟旧格式：主文件是 1 行 spectrum（无 _original 边车）
    QString spectrumPath = m_tempDir.path() + "/legacy_spectrum.tiff";
    QImage spectrum(64, 1, QImage::Format_Grayscale16);
    ushort *bits = reinterpret_cast<ushort *>(spectrum.bits());
    for (int x = 0; x < 64; ++x) {
        bits[x] = static_cast<ushort>(x * 137);
    }
    QVERIFY(spectrum.save(spectrumPath, "TIFF"));

    LoadResult res = loadSync(spectrumPath);
    QVERIFY(res.success);
    QCOMPARE(res.frame.image.height(), 1);
    // loader 不再尝试找 _original 边车
    QVERIFY(res.frame.originalImage.isNull());
    QVERIFY(!res.hasMetadata);
}

QTEST_MAIN(TestMetadataSoftwareSettings)
#include "test_metadata_save_load.moc"
