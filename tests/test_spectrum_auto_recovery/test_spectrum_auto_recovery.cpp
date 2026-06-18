// 测试加载 2D original + metadata 后，PostProcess::verticalBinning
// 自动恢复出的 spectrum 与期望值一致。
//
// 模拟 MainWindow::onFrameLoaded 中
//   if (hasMetadata && hasRange && !fullRange && imgHeight > 1)
//       PostProcess::verticalBinning(frame, start, end);
// 这一段逻辑。

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
#include "SaveTypes.h"
#include "formats/TiffFormatHandler.h"
#include "formats/CsvFormatHandler.h"
#include "FileLoaderWorker.h"
#include "PostProcess.h"

class TestSpectrumAutoRecovery : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void test_recovery_subrange_vbin_tiff();
    void test_recovery_subrange_vbin_csv();
    void test_recovery_full_range_vbin_no_op();
    void test_recovery_missing_vbin_range_no_op();
    void test_recovery_missing_metadata_no_op();
    void test_recovery_subrange_matches_postprocess_directly();

private:
    QTemporaryDir m_tempDir;
    QImage makeImage(int w, int h) const;
    QString writeTiff(const QString &name, const QImage &img) const;
    QString writeCsv(const QString &name, const QImage &img) const;
    void writeMetadata(const QString &imgPath, const QVariantMap &sw) const;
    LoadResult loadSync(const QString &filePath) const;
    QVector<quint64> expectedSpectrum(const QImage &img, int start, int end) const;
};

void TestSpectrumAutoRecovery::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestSpectrumAutoRecovery::cleanupTestCase()
{
}

void TestSpectrumAutoRecovery::init()
{
}

void TestSpectrumAutoRecovery::cleanup()
{
}

QImage TestSpectrumAutoRecovery::makeImage(int w, int h) const
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

QString TestSpectrumAutoRecovery::writeTiff(const QString &name, const QImage &img) const
{
    QString path = m_tempDir.path() + "/" + name;
    img.save(path, "TIFF");
    return path;
}

QString TestSpectrumAutoRecovery::writeCsv(const QString &name, const QImage &img) const
{
    QString path = m_tempDir.path() + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream out(&f);
    for (int y = 0; y < img.height(); ++y) {
        const ushort *row = reinterpret_cast<const ushort *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (x > 0) out << ',';
            out << row[x];
        }
        out << '\n';
    }
    f.close();
    return path;
}

void TestSpectrumAutoRecovery::writeMetadata(const QString &imgPath, const QVariantMap &sw) const
{
    QFileInfo fi(imgPath);
    QString metaPath = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_metadata.json";
    QJsonObject root;
    root["cameraId"] = "auto-cam";
    root["timestamp"] = 1;
    root["frameNumber"] = 1;
    QJsonObject swObj;
    for (auto it = sw.constBegin(); it != sw.constEnd(); ++it) {
        swObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    root["softwareSettings"] = swObj;
    QFile f(metaPath);
    f.open(QIODevice::WriteOnly | QIODevice::Text);
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

LoadResult TestSpectrumAutoRecovery::loadSync(const QString &filePath) const
{
    FileLoaderWorker worker;
    QSignalSpy okSpy(&worker, &FileLoaderWorker::frameLoaded);
    worker.loadFrame(filePath);
    if (okSpy.count() == 0) return LoadResult();
    return qvariant_cast<LoadResult>(okSpy.takeFirst().at(0));
}

QVector<quint64> TestSpectrumAutoRecovery::expectedSpectrum(const QImage &img, int start, int end) const
{
    QVector<quint64> spec(img.width(), 0);
    for (int x = 0; x < img.width(); ++x) {
        quint64 sum = 0;
        for (int y = start; y <= end; ++y) {
            sum += reinterpret_cast<const ushort *>(img.constScanLine(y))[x];
        }
        spec[x] = sum;
    }
    return spec;
}

// 模拟 MainWindow::onFrameLoaded 中的自动恢复判定与执行
// 注意：metadata 中的 vBinStartRow/vBinEndRow 是 1-based（与 RowRangeDialog 一致），
//      PostProcess::verticalBinning 内部会 -1 转换为 0-based 索引。
static bool tryAutoRecover(LoadResult &res, int &outStart1Based, int &outEnd1Based)
{
    const QImage &img = res.frame.image;
    const int h = img.height();
    const QVariantMap &sw = res.frame.softwareSettings;

    const int s = sw.value("vBinStartRow", -1).toInt();
    const int e = sw.value("vBinEndRow", -1).toInt();
    const bool hasRange = (s >= 0) && (e >= 0);
    // 注意：metadata 是 1-based，full-range 检测按 1-based
    //      s==1 表示从第 1 行开始，e==h 表示到第 h 行（即全部）
    const bool fullRange = hasRange && (s == 1) && (e == h);
    const bool ok = res.hasMetadata && hasRange && !fullRange && h > 1;
    if (!ok) return false;
    outStart1Based = qBound(1, s, h);
    outEnd1Based = qBound(outStart1Based, e, h);
    PostProcess::verticalBinning(res.frame, outStart1Based, outEnd1Based);
    return true;
}

void TestSpectrumAutoRecovery::test_recovery_subrange_vbin_tiff()
{
    QImage img = makeImage(32, 16);
    QString path = writeTiff("rt_sub.tiff", img);

    // metadata 存 1-based（与 RowRangeDialog 一致）
    // PostProcess::verticalBinning 内部 -1 转换为 0-based
    // 期望：1-based [3, 11] → 0-based [2, 10]
    QVariantMap sw;
    sw["softwareVerticalBinning"] = true;
    sw["vBinStartRow"] = 3;
    sw["vBinEndRow"] = 11;
    writeMetadata(path, sw);

    LoadResult res = loadSync(path);
    QVERIFY(res.success);
    QVERIFY(res.hasMetadata);
    QCOMPARE(res.frame.image.height(), 16);

    int s = -1, e = -1;
    QVERIFY(tryAutoRecover(res, s, e));
    QCOMPARE(s, 3);
    QCOMPARE(e, 11);
    QCOMPARE(res.frame.image.height(), 1);

    QVector<quint64> expected = expectedSpectrum(img, 2, 10);  // 0-based
    QCOMPARE(res.frame.spectrum.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i) {
        QCOMPARE(res.frame.spectrum[i], expected[i]);
    }
}

void TestSpectrumAutoRecovery::test_recovery_subrange_vbin_csv()
{
    QImage img = makeImage(24, 12);
    QString path = writeCsv("rt_sub.csv", img);

    // 1-based [2, 9] → 0-based [1, 8]
    QVariantMap sw;
    sw["softwareVerticalBinning"] = true;
    sw["vBinStartRow"] = 2;
    sw["vBinEndRow"] = 9;
    writeMetadata(path, sw);

    LoadResult res = loadSync(path);
    QVERIFY(res.success);
    QVERIFY(res.hasMetadata);
    QCOMPARE(res.frame.image.height(), 12);

    int s = -1, e = -1;
    QVERIFY(tryAutoRecover(res, s, e));
    QCOMPARE(s, 2);
    QCOMPARE(e, 9);
    QCOMPARE(res.frame.image.height(), 1);

    QVector<quint64> expected = expectedSpectrum(img, 1, 8);  // 0-based
    QCOMPARE(res.frame.spectrum.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i) {
        QCOMPARE(res.frame.spectrum[i], expected[i]);
    }
}

void TestSpectrumAutoRecovery::test_recovery_full_range_vbin_no_op()
{
    QImage img = makeImage(16, 8);
    QString path = writeTiff("rt_full.tiff", img);

    // 1-based 满范围：[1, h] 表示从第 1 行到第 h 行（即整张图）
    QVariantMap sw;
    sw["softwareVerticalBinning"] = true;
    sw["vBinStartRow"] = 1;
    sw["vBinEndRow"] = 8;  // == h
    writeMetadata(path, sw);

    LoadResult res = loadSync(path);
    QVERIFY(res.success);
    QVERIFY(res.hasMetadata);

    int s = -1, e = -1;
    QVERIFY(!tryAutoRecover(res, s, e));
    // 不应改变 image 高度
    QCOMPARE(res.frame.image.height(), 8);
    QVERIFY(res.frame.spectrum.isEmpty());
}

void TestSpectrumAutoRecovery::test_recovery_missing_vbin_range_no_op()
{
    QImage img = makeImage(16, 8);
    QString path = writeTiff("rt_norange.tiff", img);

    QVariantMap sw;
    sw["softwareVerticalBinning"] = false;
    // 没有 vBinStartRow / vBinEndRow
    writeMetadata(path, sw);

    LoadResult res = loadSync(path);
    QVERIFY(res.success);
    QVERIFY(res.hasMetadata);

    int s = -1, e = -1;
    QVERIFY(!tryAutoRecover(res, s, e));
    QCOMPARE(res.frame.image.height(), 8);
}

void TestSpectrumAutoRecovery::test_recovery_missing_metadata_no_op()
{
    QImage img = makeImage(16, 8);
    QString path = writeTiff("rt_nometa.tiff", img);
    // 不写 _metadata.json

    LoadResult res = loadSync(path);
    QVERIFY(res.success);
    QVERIFY(!res.hasMetadata);

    int s = -1, e = -1;
    QVERIFY(!tryAutoRecover(res, s, e));
    QCOMPARE(res.frame.image.height(), 8);
}

void TestSpectrumAutoRecovery::test_recovery_subrange_matches_postprocess_directly()
{
    // 验证：自动恢复出来的 spectrum 等于
    // 直接对原图调用 PostProcess::verticalBinning 的结果
    QImage img = makeImage(40, 20);
    QString path = writeTiff("rt_match.tiff", img);

    // 1-based [6, 16] → 0-based [5, 15]
    QVariantMap sw;
    sw["softwareVerticalBinning"] = true;
    sw["vBinStartRow"] = 6;
    sw["vBinEndRow"] = 16;
    writeMetadata(path, sw);

    LoadResult resLoaded = loadSync(path);
    QVERIFY(resLoaded.success);

    int s = -1, e = -1;
    QVERIFY(tryAutoRecover(resLoaded, s, e));
    const QVector<quint64> recovered = resLoaded.frame.spectrum;

    // 独立路径
    ImageData fresh;
    fresh.image = img;
    PostProcess::verticalBinning(fresh, 6, 16);

    QCOMPARE(recovered.size(), fresh.spectrum.size());
    for (int i = 0; i < recovered.size(); ++i) {
        QCOMPARE(recovered[i], fresh.spectrum[i]);
    }
}

QTEST_MAIN(TestSpectrumAutoRecovery)
#include "test_spectrum_auto_recovery.moc"
